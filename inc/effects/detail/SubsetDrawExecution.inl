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

    // Explicit live-query mode. Evaluate in the real model and intersect exact
    // entity IDs with this deck's physical entries; never choose a GUID copy.
    inline int FindEntitySubsetEntry(Obj* finder,Obj* query,Obj* effect,Obj* model,
            const std::vector<Obj*>& entries,const std::set<Obj*>& reserved) {
        static auto contextClass=BNM::Class("PvZCards.Engine","EntityQueryContext");
        static auto finderClass=BNM::Class("PvZCards.Engine","EntityFinder");
        static auto instancesField=BNM::Class("PvZCards.Core.Systems.Utilities","CardFromSubsetFinder").GetField("<Instances>k__BackingField").cast<Obj*>();
        static auto acquire=BNM::Class("PvZCards.Engine","InstanceManager").GetMethod("Acquire",2).cast<Obj*>();
        static auto release=BNM::Class("PvZCards.Engine","InstanceManager").GetMethod("Return",1).cast<void>();
        static auto copy=BNM::Class("PvZCards.Engine","EffectData").GetMethod("CopyFrom",1).cast<void>();
        static auto modelField=contextClass.GetField("<Model>k__BackingField").cast<Obj*>();
        static auto selfField=contextClass.GetField("SelfId").cast<int>();
        static auto find=finderClass.GetMethod("FindEntities",{std::string_view("query"),std::string_view("context")}).cast<Obj*>();
        if(!finder || !query || !model || !instancesField.IsValid() || !acquire.IsValid() || !release.IsValid() || !copy.IsValid() || !modelField.IsValid() || !selfField.IsValid() || !find.IsValid()) {
            LOG_ERR("CustomComponents/DrawCardFromSubsetInDeck: ENTITY_QUERY_API_MISSING finder=%d query=%d model=%d instancesField=%d acquire=%d return=%d copy=%d modelField=%d selfField=%d find=%d",
                finder!=nullptr,query!=nullptr,model!=nullptr,instancesField.IsValid(),acquire.IsValid(),release.IsValid(),copy.IsValid(),modelField.IsValid(),selfField.IsValid(),find.IsValid());return -1;
        }
        Obj* instances=instancesField[finder].Get();
        if(!instances) return -1;
        Obj* context=acquire[instances](reinterpret_cast<Obj*>(contextClass.GetMonoType()),true);
        if(!context) return -1;
        Obj* ec=*reinterpret_cast<Obj**>(reinterpret_cast<std::uint8_t*>(context)+0x18);
        if(!ec) {release[instances](context);return -1;}
        modelField[context]=model;
        selfField[context]=effectSource[effect].Get();
        for(auto pair:{std::pair<std::size_t,std::size_t>{0x10,0x28},{0x14,0x24},{0x18,0x30},{0x1C,0x2C}})
            *reinterpret_cast<int*>(reinterpret_cast<std::uint8_t*>(ec)+pair.first)=*reinterpret_cast<int*>(reinterpret_cast<std::uint8_t*>(effect)+pair.second);
        Obj* data=*reinterpret_cast<Obj**>(reinterpret_cast<std::uint8_t*>(ec)+0x20);
        Obj* sourceData=*reinterpret_cast<Obj**>(reinterpret_cast<std::uint8_t*>(effect)+0x10);
        if(!data || !sourceData) {release[instances](context);return -1;}
        copy[data](sourceData);
        Obj* liveFinder=finderClass.CreateNewObjectParameters(model);
        Obj* matches=liveFinder?find[liveFinder](query,context):nullptr;
        std::vector<std::pair<int,int>> identities;
        for(auto* entity:Items(reinterpret_cast<MonoList<Obj*>*>(matches)))
            if(entity && GuidOf(entity)>0) identities.push_back({entityId[entity].Get(),GuidOf(entity)});
        std::vector<CustomComponents::DeckSelection::Entry> physical;
        for(auto* entry:entries)
            physical.push_back(entry?CustomComponents::DeckSelection::Entry{entryEntityId[entry].Get(),entryGuid[entry].Get(),reserved.count(entry)!=0}:CustomComponents::DeckSelection::Entry{});
        const int chosen=CustomComponents::DeckSelection::FirstExactEntry(physical,identities);
        release[instances](context);
        return chosen;
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
            if (pending->pluckFailSafe && !pending->entityQuery)
                originalProcessSubset(self, effect, method);
            else
                effectCancel[effect]();
            --pending->remaining;
            CleanupPending();
            return;
        }

        const auto entries = Items(list);
        const auto reserved = Reserved(model, deck, entries);
        if(pending->entityQuery) {
            Obj* finder=subsetSystemFinder[static_cast<Obj*>(self)].Get();
            const int index=FindEntitySubsetEntry(finder,subsetEffectQuery[effect].Get(),effect,model,entries,reserved);
            Obj* ability=AbilitySystemFor(model);
            Obj* handEntity=FindEntity(model,hand);
            if(index>=0 && ability && handEntity && MoveToTop(list,index,entries[index])) {
                Obj* entry=entries[index];
                {std::lock_guard lock(deckMapMutex);reservedEntries[{model,deck}].push_back(entry);}
                LOG_INFO("CustomComponents/DrawCardFromSubsetInDeck: mode=ENTITY entity=%d guid=%d deck=%d",entryEntityId[entry].Get(),entryGuid[entry].Get(),deck);
                drawCardsFromDeck[ability](handEntity,effect,1);
            } else LOG_INFO("CustomComponents/DrawCardFromSubsetInDeck: mode=ENTITY noDeckMatchOrDelivery=1 deck=%d",deck);
            effectCancel[effect]();
            --pending->remaining;CleanupPending();return;
        }


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

