#include "diagnostics/DiagnosticLogging.hpp"
    // Opening-hand / ordinary draw traffic establishes which deck belongs to a
    // hand/player. This is read-only bookkeeping copied from the proven Tutor module.
    static void ProcessDrawHook(
        void* self,
        Obj* effect,
        const void* method
    ) {
        if (effect) {
            const int hand = drawHandId[effect]();
            const int deck = drawDeckId[effect]();
            const int player = effectPlayer[effect].Get();

            std::lock_guard lock(deckMapMutex);
            deckForHand[hand] = deck;
            deckForPlayer[player] = deck;
        }
        originalProcessDraw(self, effect, method);
    }

    // CardFromSubsetFinder still owns all subset semantics. We only change its
    // final choice while one of OUR child effects is being processed.
    static int PickRandomGuidHook(
        void* self,
        Obj* cards,
        const void* method
    ) {
        if (!currentPluck || !currentPluck->active)
            return originalPickRandomGuid(self, cards, method);

        currentPluck->pickerCalled = true;

        if (TypeName(cards) != "List`1") {
            return originalPickRandomGuid(self, cards, method);
        }

        std::set<int> candidateGuids;
        for (auto* card : Items(reinterpret_cast<MonoList<Obj*>*>(cards))) {
            const int guid = GuidOf(card);
            if (guid >= 0)
                candidateGuids.insert(guid);
        }

        // OrderedCardPool.Entries is deck order. First matching entry wins:
        // this is a true top-of-deck tutor, never a random matching copy.
        for (int guid : currentPluck->orderedDeckGuids) {
            if (candidateGuids.count(guid)) {
                currentPluck->foundDeckMatch = true;
                currentPluck->pickedGuid = guid;
                return guid;
            }
        }

        currentPluck->noMatch = true;

        // During the disposable pre-check, never let the stock picker consume RNG
        // or choose a conjure target. The caller inspects noMatch and decides
        // whether to cancel (PluckFailSafe=false) or run vanilla fallback.
        if (currentPluck->precheck)
            return -1;

        // This path is only expected for PluckFailSafe=true fallback.
        return originalPickRandomGuid(self, cards, method);
    }

    static void ProcessSubsetHook(
        void* self,
        Obj* effect,
        const void* method
    ) {
        PendingDraw* pending = FindPending(effect);
        if (!pending) {
            originalProcessSubset(self, effect, method);
            return;
        }

        Obj* model = self ? subsetSystemModel[static_cast<Obj*>(self)].Get() : nullptr;
        const int hand = subsetHandId[effect]();
        const int player = effectPlayer[effect].Get();

        int deck = -1;
        {
            std::lock_guard lock(deckMapMutex);
            if (auto it = deckForHand.find(hand); it != deckForHand.end())
                deck = it->second;
            else if (auto it = deckForPlayer.find(player); it != deckForPlayer.end())
                deck = it->second;
        }

        Obj* deckEntity = FindEntity(model, deck);
        Obj* pool = FindComponent(deckEntity, "OrderedCardPool");
        auto* list = pool ? poolEntries[pool].Get() : nullptr;

        // No usable real deck: TRUE keeps vanilla conjure behavior; FALSE fizzles.
        if (!list) {
            if (pending->pluckFailSafe)
                originalProcessSubset(self, effect, method);
            else
                effectCancel[effect]();
            --pending->remaining;
            CleanupPending();
            return;
        }

        const auto entries = Items(list);
        const auto reserved = Reserved(model, deck, entries);

        ActivePluck state;
        state.active = true;
        state.pluckFailSafe = pending->pluckFailSafe;
        state.deckEntity = deckEntity;
        for (auto* entry : entries) {
            if (entry && !reserved.count(entry))
                state.orderedDeckGuids.push_back(entryGuid[entry].Get());
        }

        ActivePluck* previous = currentPluck;
        currentPluck = &state;

        // Tutor-style disposable pre-check. CardFromSubsetFinder still evaluates
        // the real SubsetQuery, but PickRandomGuid only reports whether a matching
        // card exists in the live deck. No actual subset draw is committed here.
        Obj* finder = subsetSystemFinder[static_cast<Obj*>(self)].Get();
        Obj* query = subsetEffectQuery[effect].Get();
        if (finder && query) {
            state.precheck = true;
            const int probeGuid = finderFindCard[finder](query, effect);
            state.precheck = false;

            // Do not depend solely on PickRandomGuid being reached.  Some finder
            // paths can resolve a single candidate without calling the random
            // picker.  Validate the probe result against the live deck ourselves.
            if (probeGuid < 0) {
                state.noMatch = true;
                state.foundDeckMatch = false;
                state.pickedGuid = -1;
            }
            else if (!state.foundDeckMatch) {
                const auto it = std::find(
                    state.orderedDeckGuids.begin(),
                    state.orderedDeckGuids.end(),
                    probeGuid
                );
                if (it != state.orderedDeckGuids.end()) {
                    state.foundDeckMatch = true;
                    state.noMatch = false;
                    state.pickedGuid = probeGuid;
                }
                else {
                    state.noMatch = true;
                    state.pickedGuid = -1;
                }
            }
            state.pickerCalled = false;
        }
        else {
            LOG_WARN(
                "CustomComponents/DrawCardFromSubsetInDeck: pre-check unavailable (finder=%p query=%p)",
                finder,
                query
            );
        }

        // Hard fizzle: when PluckFailSafe is false, a miss never enters stock
        // DrawCardFromSubset processing, so there is no opportunity to conjure.
        if (state.noMatch && !pending->pluckFailSafe) {
            currentPluck = previous;
            effectCancel[effect]();
            CC_DIAG(
                "CustomComponents/DrawCardFromSubsetInDeck: source=%d no matching card in deck; fizzled",
                effectSource[effect].Get()
            );
            --pending->remaining;
            CleanupPending();
            return;
        }

        // Real draw delivery: move the chosen physical CardPoolEntry to index 0
        // and use the game's own DrawCardsFromDeck path. This counts as a draw and
        // avoids the old conjure-then-remove emulation entirely.
        if (state.pickedGuid >= 0) {
            Obj* abilitySystem = AbilitySystemFor(model);
            Obj* handEntity = FindEntity(model, hand);
            const int index = FindEntry(entries, state.pickedGuid, reserved);
            if (abilitySystem && handEntity && index >= 0) {
                Obj* entry = entries[index];
                currentPluck = previous;
                if (MoveToTop(list, index, entry)) {
                    {
                        std::lock_guard lock(deckMapMutex);
                        reservedEntries[{model, deck}].push_back(entry);
                    }
                    drawCardsFromDeck[abilitySystem](handEntity, effect, 1);

                    // This synthetic DrawCardFromSubsetEffect has delegated its
                    // actual work to a real DrawCardEffect.  Retire the synthetic
                    // child explicitly so no downstream subset lifecycle/presentation
                    // can wait on an unset DrawnCardId.
                    effectCancel[effect]();

                    CC_DIAG(
                        "CustomComponents/DrawCardFromSubsetInDeck: source=%d queued real deck draw guid=%d; delegated subset child cancelled",
                        effectSource[effect].Get(),
                        state.pickedGuid
                    );
                    --pending->remaining;
                    CleanupPending();
                    return;
                }
                currentPluck = &state;
                LOG_WARN(
                    "CustomComponents/DrawCardFromSubsetInDeck: failed to move guid=%d to deck top; using fallback",
                    state.pickedGuid
                );
            }
            else {
                LOG_WARN(
                    "CustomComponents/DrawCardFromSubsetInDeck: draw delivery unavailable (system=%p hand=%p entry=%d); using fallback",
                    abilitySystem,
                    handEntity,
                    index
                );
            }
        }

        currentPluck = previous;

        // A genuine no-match only reaches here when PluckFailSafe=true, which is
        // defined as ordinary stock DrawCardFromSubset/conjure behavior.
        if (state.noMatch && pending->pluckFailSafe) {
            originalProcessSubset(self, effect, method);
        }
        else if (state.pickedGuid >= 0) {
            // We found a deck match but real draw delivery was unavailable. Do not
            // silently conjure when fail-safe is false; cancel instead. With TRUE,
            // stock behavior remains the explicit recovery policy.
            if (pending->pluckFailSafe) {
                originalProcessSubset(self, effect, method);
            }
            else {
                effectCancel[effect]();
            }
        }
        else {
            if (pending->pluckFailSafe) {
                originalProcessSubset(self, effect, method);
            }
            else {
                effectCancel[effect]();
            }
        }

        --pending->remaining;
        CleanupPending();
    }

