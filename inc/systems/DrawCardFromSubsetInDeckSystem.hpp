#pragma once

#include <BNM/BasicMonoStructures.hpp>
#include <BNM/Class.hpp>
#include <BNM/Field.hpp>
#include <BNM/Method.hpp>
#include <BNM/MethodBase.hpp>

#include <algorithm>
#include <dlfcn.h>
#include <map>
#include <mutex>
#include <set>
#include <unordered_map>
#include <vector>

#include "components/DrawCardFromSubsetInDeckEffectDescriptor.hpp"
#include "effects/DrawCardFromSubsetInDeckEffect.hpp"
#include "core/CustomComponentRegistry.hpp"
#include "systems/CustomFieldBridgeSystem.hpp"
#include "log.h"

namespace CustomComponents::Systems::DrawCardFromSubsetInDeckSystem {

    using Obj = BNM::IL2CPP::Il2CppObject;
    template<typename T> using MonoList = BNM::Structures::Mono::List<T>;

    // -------------------------------------------------------------------------
    // Managed identities
    // -------------------------------------------------------------------------

    inline BNM::MonoType* descriptorType = nullptr;
    inline BNM::MonoType* runtimeType = nullptr;
    inline BNM::MonoType* stockRuntimeType = nullptr;

    // -------------------------------------------------------------------------
    // Engine API
    // -------------------------------------------------------------------------

    inline BNM::Field<int> effectSource;
    inline BNM::Field<int> effectPlayer;
    inline BNM::Field<int> effectGroup;

    inline BNM::Field<int> customDrawAmount;
    inline BNM::Field<bool> descriptorFailSafe;

    inline BNM::Field<MonoList<Obj*>*> modelEntities;
    inline BNM::Field<MonoList<Obj*>*> entityComponents;
    inline BNM::Field<int> entityId;
    inline BNM::Field<Obj*> subsetSystemModel;
    inline BNM::Field<Obj*> subsetSystemFinder;
    inline BNM::Field<Obj*> abilitySystemModel;
    inline BNM::Field<Obj*> subsetEffectQuery;

    inline BNM::Method<int> subsetHandId;
    inline BNM::Method<int> drawHandId;
    inline BNM::Method<int> drawDeckId;
    inline BNM::Method<int> finderFindCard;
    inline BNM::Method<void> drawCardsFromDeck;
    inline BNM::Method<void> effectCancel;

    inline BNM::Field<int> cardGuid;
    inline BNM::Field<MonoList<Obj*>*> poolEntries;
    inline BNM::Field<int> entryGuid;
    inline BNM::Field<int> entryEntityId;

    // -------------------------------------------------------------------------
    // Originals
    // -------------------------------------------------------------------------

    using DescriptorToEffectFn =
        Obj* (*)(void*, Obj*, Obj*, const void*);
    using RegisterForEffectFn =
        void (*)(void*, void*, void*, const void*);
    using DrawSubsetAbilityFn =
        void (*)(void*, Obj*, const void*);
    using ProcessSubsetFn =
        void (*)(void*, Obj*, const void*);
    using ProcessDrawFn =
        void (*)(void*, Obj*, const void*);
    using PickRandomGuidFn =
        int (*)(void*, Obj*, const void*);
    using RegisterHandlersFn =
        void (*)(void*, Obj*, const void*);

    using DescriptorCopyFn = void (*)(void*, Obj*, Obj*, const void*);
    using DescriptorPoolFn = void (*)(void*, Obj*, const void*);
    inline DescriptorCopyFn originalDescriptorCopy = nullptr;
    inline DescriptorPoolFn originalDescriptorPool = nullptr;

    inline std::mutex descriptorPolicyMutex;
    inline std::unordered_map<Obj*, bool> descriptorPolicy;

    inline DescriptorToEffectFn originalDescriptorToEffect = nullptr;
    inline RegisterForEffectFn originalRegisterForEffect = nullptr;
    inline DrawSubsetAbilityFn originalDrawSubsetAbility = nullptr;
    inline ProcessSubsetFn originalProcessSubset = nullptr;
    inline ProcessDrawFn originalProcessDraw = nullptr;
    inline PickRandomGuidFn originalPickRandomGuid = nullptr;
    inline RegisterHandlersFn originalRegisterHandlers = nullptr;

    // -------------------------------------------------------------------------
    // Deck association + pending custom draw state
    // -------------------------------------------------------------------------

    inline std::mutex deckMapMutex;
    inline std::map<int, int> deckForHand;
    inline std::map<int, int> deckForPlayer;

    // Weakly track DrawCardAbilitySystem instances so draw delivery can use the
    // game's real DrawCardsFromDeck path without keeping old matches alive.
    inline uintptr_t(*gcNewWeak)(Obj*, bool) = nullptr;
    inline Obj* (*gcTarget)(uintptr_t) = nullptr;
    inline void (*gcFree)(uintptr_t) = nullptr;
    inline std::vector<uintptr_t> abilitySystems;

    // Entries moved to index 0 for queued tutor draws are temporarily reserved
    // so another queued child cannot select the same physical deck entry.
    inline std::map<std::pair<Obj*, int>, std::vector<Obj*>> reservedEntries;

    struct PendingDraw {
        int sourceId = -1;
        int playerId = -1;
        int effectGroupId = -1;
        int remaining = 0;
        bool pluckFailSafe = false;
    };

    inline thread_local std::vector<PendingDraw> pendingDraws;
    // Stock runtime objects produced by our custom descriptor carry their
    // per-effect PluckFailSafe policy here until DrawCardAbilitySystem processes them.
    inline thread_local std::unordered_map<Obj*, bool> failSafeByAbilityEffect;



    struct ActivePluck {
        bool active = false;
        bool pluckFailSafe = false;
        Obj* deckEntity = nullptr;
        std::vector<int> orderedDeckGuids;
        bool precheck = false;
        bool pickerCalled = false;
        bool noMatch = false;
        bool foundDeckMatch = false;
        int pickedGuid = -1;
    };

    inline thread_local ActivePluck* currentPluck = nullptr;

    // -------------------------------------------------------------------------
    // Helpers
    // -------------------------------------------------------------------------

    template<typename T>
    inline std::vector<T> Items(MonoList<T>* list) {
        std::vector<T> out;
        if (!list || !list->items)
            return out;
        out.reserve(list->size);
        for (int i = 0; i < list->size; ++i)
            out.push_back(list->items->m_Items[i]);
        return out;
    }

    inline std::string TypeName(Obj* object) {
        if (!object)
            return "null";
        auto* klass = BNM::Class(object).GetClass();
        return klass && klass->name ? klass->name : "?";
    }

    inline Obj* FindComponent(Obj* entity, const char* name) {
        if (!entity)
            return nullptr;
        for (auto* component : Items(entityComponents[entity].Get())) {
            if (TypeName(component) == name)
                return component;
        }
        return nullptr;
    }

    inline Obj* FindEntity(Obj* model, int id) {
        if (!model || id < 0)
            return nullptr;
        for (auto* entity : Items(modelEntities[model].Get())) {
            if (entity && entityId[entity].Get() == id)
                return entity;
        }
        return nullptr;
    }

    inline int GuidOf(Obj* entity) {
        auto* card = FindComponent(entity, "Card");
        return card ? cardGuid[card].Get() : -1;
    }

    inline bool IsCustomRuntime(Obj* effect) {
        if (!effect || !runtimeType)
            return false;
        auto customClass = BNM::Class(
            "PvZCards.Engine.Effects",
            "DrawCardFromSubsetInDeckEffect"
        );
        return customClass.IsValid() &&
            BNM::Class(effect).GetClass() == customClass.GetClass();
    }

    inline PendingDraw* FindPending(Obj* effect) {
        if (!effect)
            return nullptr;

        const int source = effectSource[effect].Get();
        const int player = effectPlayer[effect].Get();
        const int group = effectGroup[effect].Get();

        // Prefer exact EffectGroupId. Fall back to source/player because some
        // queue-building paths can assign the child group after CopyDefaultIds.
        for (auto& pending : pendingDraws) {
            if (pending.remaining > 0 &&
                pending.sourceId == source &&
                pending.playerId == player &&
                pending.effectGroupId == group) {
                return &pending;
            }
        }
        for (auto& pending : pendingDraws) {
            if (pending.remaining > 0 &&
                pending.sourceId == source &&
                pending.playerId == player) {
                return &pending;
            }
        }
        return nullptr;
    }

    inline void CleanupPending() {
        pendingDraws.erase(
            std::remove_if(
                pendingDraws.begin(),
                pendingDraws.end(),
                [](const PendingDraw& p) { return p.remaining <= 0; }
            ),
            pendingDraws.end()
        );
    }

    inline BNM::Method<void> ListMethod(MonoList<Obj*>* list, const char* name, int parameters) {
        if (!list)
            return {};
        auto method = BNM::Class(static_cast<Obj*>(list)).GetMethod(name, parameters).cast<void>();
        if (!method.IsValid())
            LOG_ERR("CustomComponents/DrawCardFromSubsetInDeck: List.%s not found", name);
        return method;
    }

    inline int FindEntry(
        const std::vector<Obj*>& entries,
        int guid,
        const std::set<Obj*>& skip
    ) {
        int index = -1;
        for (int i = 0; i < static_cast<int>(entries.size()); ++i) {
            auto* entry = entries[i];
            if (!entry || skip.count(entry) || entryGuid[entry].Get() != guid)
                continue;
            if (index < 0)
                index = i;
            if (entryEntityId[entry].Get() <= 0)
                return i;
        }
        return index;
    }

    inline bool MoveToTop(MonoList<Obj*>* list, int index, Obj* entry) {
        auto removeAt = ListMethod(list, "RemoveAt", 1);
        auto insert = ListMethod(list, "Insert", 2);
        if (!removeAt.IsValid() || !insert.IsValid())
            return false;
        removeAt[static_cast<Obj*>(list)](index);
        insert[static_cast<Obj*>(list)](0, entry);
        return true;
    }

    inline std::set<Obj*> Reserved(
        Obj* model,
        int deck,
        const std::vector<Obj*>& entries
    ) {
        std::lock_guard lock(deckMapMutex);
        std::set<Obj*> present;
        auto it = reservedEntries.find({ model, deck });
        if (it == reservedEntries.end())
            return present;

        const std::set<Obj*> live(entries.begin(), entries.end());
        std::vector<Obj*> keep;
        for (auto* entry : it->second) {
            if (live.count(entry)) {
                keep.push_back(entry);
                present.insert(entry);
            }
        }
        if (keep.empty())
            reservedEntries.erase(it);
        else
            it->second.swap(keep);
        return present;
    }

    inline Obj* AbilitySystemFor(Obj* model) {
        if (!gcTarget)
            return nullptr;
        std::lock_guard lock(deckMapMutex);
        for (auto handle : abilitySystems) {
            Obj* system = gcTarget(handle);
            if (system && abilitySystemModel[system].Get() == model)
                return system;
        }
        return nullptr;
    }

    static void RegisterHandlersHook(
        void* self,
        Obj* router,
        const void* method
    ) {
        originalRegisterHandlers(self, router, method);
        if (!self || !gcNewWeak || !gcTarget || !gcFree)
            return;

        std::lock_guard lock(deckMapMutex);
        std::vector<uintptr_t> alive;
        for (auto handle : abilitySystems) {
            if (gcTarget(handle))
                alive.push_back(handle);
            else
                gcFree(handle);
        }
        alive.push_back(gcNewWeak(static_cast<Obj*>(self), false));
        abilitySystems.swap(alive);
    }

    // -------------------------------------------------------------------------
    // Focused custom-field deserialization + descriptor lifecycle bridge
    // -------------------------------------------------------------------------

    inline bool IsCustomDescriptor(Obj* object) {
        if (!object) return false;
        auto klass = BNM::Class(
            "PvZCards.Engine.Components",
            "DrawCardFromSubsetInDeckEffectDescriptor"
        );
        return klass.IsValid() && BNM::Class(object).GetClass() == klass.GetClass();
    }

    inline void PutDescriptorPolicy(Obj* descriptor, bool value) {
        if (!descriptor) return;
        std::lock_guard lock(descriptorPolicyMutex);
        descriptorPolicy[descriptor] = value;
    }

    inline bool GetDescriptorPolicy(Obj* descriptor, bool fallback = false) {
        if (!descriptor) return fallback;
        std::lock_guard lock(descriptorPolicyMutex);
        const auto it = descriptorPolicy.find(descriptor);
        return it == descriptorPolicy.end() ? fallback : it->second;
    }

    inline void EraseDescriptorPolicy(Obj* descriptor) {
        if (!descriptor) return;
        std::lock_guard lock(descriptorPolicyMutex);
        descriptorPolicy.erase(descriptor);
    }

    static void DescriptorCopyHook(
        void* self, Obj* source, Obj* instances, const void* method
    ) {
        const bool custom = IsCustomDescriptor(static_cast<Obj*>(self));
        const bool policy = custom && source
            ? GetDescriptorPolicy(source, descriptorFailSafe[source].Get())
            : false;

        originalDescriptorCopy(self, source, instances, method);

        if (custom) {
            auto* target = static_cast<Obj*>(self);
            descriptorFailSafe[target] = policy;
            PutDescriptorPolicy(target, policy);
        }
    }

    static void DescriptorPoolHook(void* self, Obj* instances, const void* method) {
        auto* descriptor = static_cast<Obj*>(self);
        const bool custom = IsCustomDescriptor(descriptor);
        if (custom)
            EraseDescriptorPolicy(descriptor);

        originalDescriptorPool(self, instances, method);

        if (custom)
            descriptorFailSafe[descriptor] = false;
    }

    // -------------------------------------------------------------------------
    // Descriptor -> stock runtime policy bridge
    //
    // ComponentSupport dispatches this custom descriptor through the inherited
    // stock ToEffect virtual. Keep the stock runtime (the lifecycle that advances
    // effect groups correctly), but remember which stock runtime came from us and
    // copy PluckFailSafe alongside it.
    // -------------------------------------------------------------------------

    static Obj* DescriptorToEffectHook(
        void* self,
        Obj* source,
        Obj* context,
        const void* method
    ) {
        Obj* runtime = originalDescriptorToEffect(self, source, context, method);

        if (self && runtime &&
            BNM::Class(static_cast<Obj*>(self)).GetClass() ==
                BNM::Class(
                    "PvZCards.Engine.Components",
                    "DrawCardFromSubsetInDeckEffectDescriptor"
                ).GetClass()) {
            auto* descriptor = static_cast<Obj*>(self);
            failSafeByAbilityEffect[runtime] =
                GetDescriptorPolicy(
                    descriptor,
                    descriptorFailSafe[descriptor].Get()
                );
        }

        return runtime;
    }

    // -------------------------------------------------------------------------
    // EffectRouter registration bridge
    //
    // DrawCardFromSubsetInDeckEffect is a NEW runtime Type. We do not replace the
    // stock type. When DrawCardAbilitySystem registers its stock handler for
    // DrawCardFromSubsetAbilityEffect, mirror that same handler onto our subtype.
    // -------------------------------------------------------------------------

    static void RegisterForEffectHook(
        void* self,
        void* effectType,
        void* handler,
        const void* method
    ) {
        // Preserve stock router registration exactly. The actual component path
        // produces DrawCardFromSubsetAbilityEffect, so no custom runtime handler
        // is injected here.
        originalRegisterForEffect(self, effectType, handler, method);
    }

    // The stock DrawCardAbilitySystem handler builds the ordinary child
    // DrawCardFromSubsetEffect(s). The descriptor bridge marks only stock runtime
    // objects originating from our custom component, so ordinary stock effects
    // remain completely unchanged.
    static void DrawSubsetAbilityHook(
        void* self,
        Obj* effect,
        const void* method
    ) {
        auto it = failSafeByAbilityEffect.find(effect);
        const bool bridgedCustom = it != failSafeByAbilityEffect.end();

        if (bridgedCustom) {
            PendingDraw pending;
            pending.sourceId = effectSource[effect].Get();
            pending.playerId = effectPlayer[effect].Get();
            pending.effectGroupId = effectGroup[effect].Get();
            pending.remaining = std::max(0, customDrawAmount[effect].Get());
            pending.pluckFailSafe = it->second;

            // Consume the bridge now. PendingDraw owns the policy for the children.
            failSafeByAbilityEffect.erase(it);

            if (pending.remaining > 0) {
                pendingDraws.push_back(pending);
            }
        }

        originalDrawSubsetAbility(self, effect, method);
    }

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
            LOG_INFO(
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

                    LOG_INFO(
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

    // -------------------------------------------------------------------------
    // Registration / initialization
    // -------------------------------------------------------------------------

    inline bool RegisterTypes() {
        auto descriptorClass = BNM::Class(
            "PvZCards.Engine.Components",
            "DrawCardFromSubsetInDeckEffectDescriptor"
        );
        auto runtimeClass = BNM::Class(
            "PvZCards.Engine.Effects",
            "DrawCardFromSubsetInDeckEffect"
        );

        if (!descriptorClass.IsValid() || !runtimeClass.IsValid()) {
            LOG_ERR(
                "CustomComponents/DrawCardFromSubsetInDeck: managed classes were not registered"
            );
            return false;
        }

        if (!CustomComponentRegistry::FindByName(
            "PvZCards.Engine.Components",
            "DrawCardFromSubsetInDeckEffectDescriptor")) {
            if (!CustomComponentRegistry::Register({
                    "DrawCardFromSubsetInDeckEffectDescriptor",
                    "PvZCards.Engine.Components",
                    "EngineLib.dll",
                    "DrawCardFromSubsetEffectDescriptor",
                    "PvZCards.Engine.Components",
                    "EngineLib.dll"
                })) {
                return false;
            }
        }

        if (!CustomComponentRegistry::FindByName(
            "PvZCards.Engine.Effects",
            "DrawCardFromSubsetInDeckEffect")) {
            if (!CustomComponentRegistry::Register({
                    "DrawCardFromSubsetInDeckEffect",
                    "PvZCards.Engine.Effects",
                    "EngineLib.dll",
                    "DrawCardFromSubsetAbilityEffect",
                    "PvZCards.Engine.Effects",
                    "EngineLib.dll"
                })) {
                return false;
            }
        }

        auto* descriptor = CustomComponentRegistry::FindByName(
            "PvZCards.Engine.Components",
            "DrawCardFromSubsetInDeckEffectDescriptor"
        );
        auto* runtime = CustomComponentRegistry::FindByName(
            "PvZCards.Engine.Effects",
            "DrawCardFromSubsetInDeckEffect"
        );

        descriptorType = descriptor ? descriptor->monoType : nullptr;
        runtimeType = runtime ? runtime->monoType : nullptr;
        stockRuntimeType = BNM::Class(
            "PvZCards.Engine.Effects",
            "DrawCardFromSubsetAbilityEffect"
        ).GetMonoType();

        if (!descriptorType || !runtimeType || !stockRuntimeType) {
            LOG_ERR(
                "CustomComponents/DrawCardFromSubsetInDeck: MonoType initialization failed"
            );
            return false;
        }

        if (!CustomFieldBridgeSystem::Register(
                "PvZCards.Engine.Components",
                "DrawCardFromSubsetInDeckEffectDescriptor",
                "PvZCards.Engine.Components",
                "DrawCardFromSubsetEffectDescriptor",
                {
                    {"PluckFailSafe", CustomFieldBridgeSystem::FieldKind::Bool}
                },
                false)) {
            LOG_ERR(
                "CustomComponents/DrawCardFromSubsetInDeck: custom-field schema registration failed"
            );
            return false;
        }

        return true;
    }

    inline bool InstallHooks() {
        auto effect = BNM::Class("PvZCards.Engine", "Effect");
        auto router = BNM::Class("PvZCards.Engine", "EffectRouter");
        auto entity = BNM::Class("PvZCards.Engine", "Entity");
        auto model = BNM::Class("PvZCards.Engine", "EntityModel");
        auto drawAbilitySystem = BNM::Class(
            "PvZCards.Core.Systems",
            "DrawCardAbilitySystem"
        );
        auto subsetSystem = BNM::Class(
            "PvZCards.Core.Systems",
            "DrawCardFromSubsetSystem"
        );
        auto drawSystem = BNM::Class(
            "PvZCards.Core.Systems",
            "DrawCardSystem"
        );
        auto finder = BNM::Class(
            "PvZCards.Core.Systems.Utilities",
            "CardFromSubsetFinder"
        );
        auto customDescriptor = BNM::Class(
            "PvZCards.Engine.Components",
            "DrawCardFromSubsetInDeckEffectDescriptor"
        );
        auto customRuntime = BNM::Class(
            "PvZCards.Engine.Effects",
            "DrawCardFromSubsetInDeckEffect"
        );
        auto stockRuntime = BNM::Class(
            "PvZCards.Engine.Effects",
            "DrawCardFromSubsetAbilityEffect"
        );
        auto subsetEffect = BNM::Class(
            "PvZCards.Engine.Effects",
            "DrawCardFromSubsetEffect"
        );
        auto drawEffect = BNM::Class(
            "PvZCards.Engine.Effects",
            "DrawCardEffect"
        );
        auto card = BNM::Class("PvZCards.Engine.Components", "Card");
        auto pool = BNM::Class("PvZCards.Engine.Components", "OrderedCardPool");
        auto entry = BNM::Class("PvZCards.Engine.Components.Data", "CardPoolEntry");

        if (!effect.IsValid() || !router.IsValid() || !entity.IsValid() ||
            !model.IsValid() || !drawAbilitySystem.IsValid() ||
            !subsetSystem.IsValid() || !drawSystem.IsValid() ||
            !finder.IsValid() ||
            !customDescriptor.IsValid() ||
            !customRuntime.IsValid() ||
            !stockRuntime.IsValid() || !subsetEffect.IsValid() ||
            !drawEffect.IsValid() || !card.IsValid() || !pool.IsValid() ||
            !entry.IsValid()) {
            LOG_ERR(
                "CustomComponents/DrawCardFromSubsetInDeck: required classes missing"
            );
            return false;
        }

        effectSource = effect.GetField("SourceId");
        effectPlayer = effect.GetField("PlayerId");
        effectGroup = effect.GetField("EffectGroupId");
        customDrawAmount = stockRuntime.GetField("DrawAmount");
        descriptorFailSafe = customDescriptor.GetField("PluckFailSafe");

        modelEntities = model.GetField("Entities");
        entityComponents = entity.GetField("Components");
        entityId = entity.GetField("<Id>k__BackingField");
        subsetSystemModel = subsetSystem.GetField("<Model>k__BackingField");
        subsetSystemFinder = subsetSystem.GetField("<CardFromSubsetFinder>k__BackingField");
        abilitySystemModel = drawAbilitySystem.GetField("<EntityModel>k__BackingField");

        subsetHandId = subsetEffect.GetMethod("get_HandId", 0);
        subsetEffectQuery = subsetEffect.GetField("SubsetQuery");
        drawHandId = drawEffect.GetMethod("get_HandId", 0);
        drawDeckId = drawEffect.GetMethod("get_DeckId", 0);

        cardGuid = card.GetField("Guid");
        poolEntries = pool.GetField("Entries");
        entryGuid = entry.GetField("Guid");
        entryEntityId = entry.GetField("EntityId");

        auto stockDescriptor = BNM::Class(
            "PvZCards.Engine.Components",
            "DrawCardFromSubsetEffectDescriptor"
        );
        auto descriptorToEffect = stockDescriptor.GetMethod("ToEffect", 2);
        auto descriptorCopy = stockDescriptor.GetMethod("DeepCopyFrom", 2);
        auto descriptorPool = stockDescriptor.GetMethod("HandleReturnToPool", 1);
        auto registerForEffect = router.GetMethod("RegisterForEffect", 2);
        // IMPORTANT: DrawCardAbilitySystem has two ProcessEffect overloads with
        // one argument. Selecting only by parameter count can hook
        // ProcessEffect(DrawCardAbilityEffect), which never sees our custom
        // DrawCardFromSubsetInDeckEffect. Select the subset overload explicitly.
        auto processAbility = drawAbilitySystem.GetMethod(
            "ProcessEffect",
            {
                BNM::CompileTimeClassBuilder(
                    "PvZCards.Engine.Effects",
                    "DrawCardFromSubsetAbilityEffect",
                    "EngineLib.dll"
                ).Build()
            }
        );
        auto processSubset = subsetSystem.GetMethod("ProcessEffect", 1);
        auto processDraw = drawSystem.GetMethod("ProcessEffect", 1);
        auto pickRandomGuid = finder.GetMethod("PickRandomGuid", 1);
        auto registerHandlers = drawAbilitySystem.GetMethod("RegisterEffectHandlers", 1);
        finderFindCard = finder.GetMethod("FindCard", 2);
        drawCardsFromDeck = drawAbilitySystem.GetMethod("DrawCardsFromDeck", 3);
        effectCancel = effect.GetMethod("Cancel", 0);

        if (!effectSource.IsValid() || !effectPlayer.IsValid() ||
            !effectGroup.IsValid() || !customDrawAmount.IsValid() ||
            !descriptorFailSafe.IsValid() || !modelEntities.IsValid() ||
            !entityComponents.IsValid() || !entityId.IsValid() ||
            !subsetSystemModel.IsValid() || !subsetSystemFinder.IsValid() ||
            !abilitySystemModel.IsValid() || !subsetHandId.IsValid() ||
            !subsetEffectQuery.IsValid() || !drawHandId.IsValid() ||
            !drawDeckId.IsValid() || !cardGuid.IsValid() ||
            !poolEntries.IsValid() || !entryGuid.IsValid() ||
            !entryEntityId.IsValid() || !descriptorToEffect.IsValid() ||
            !descriptorCopy.IsValid() || !descriptorPool.IsValid() ||
            !registerForEffect.IsValid() || !processAbility.IsValid() ||
            !processSubset.IsValid() || !processDraw.IsValid() ||
            !pickRandomGuid.IsValid() || !registerHandlers.IsValid() ||
            !finderFindCard.IsValid() || !drawCardsFromDeck.IsValid() ||
            !effectCancel.IsValid()) {
            LOG_ERR(
                "CustomComponents/DrawCardFromSubsetInDeck: required API missing"
            );
            return false;
        }

        if (void* il2cpp = dlopen("libil2cpp.so", RTLD_NOW | RTLD_NOLOAD)) {
            gcNewWeak = reinterpret_cast<decltype(gcNewWeak)>(
                dlsym(il2cpp, "il2cpp_gchandle_new_weakref")
                );
            gcTarget = reinterpret_cast<decltype(gcTarget)>(
                dlsym(il2cpp, "il2cpp_gchandle_get_target")
                );
            gcFree = reinterpret_cast<decltype(gcFree)>(
                dlsym(il2cpp, "il2cpp_gchandle_free")
                );
        }
        if (!gcNewWeak || !gcTarget || !gcFree) {
            LOG_ERR(
                "CustomComponents/DrawCardFromSubsetInDeck: IL2CPP GC API unavailable; custom draw cannot initialize"
            );
            return false;
        }

        BNM::BasicHook(
            descriptorCopy,
            DescriptorCopyHook,
            originalDescriptorCopy
        );
        BNM::BasicHook(
            descriptorPool,
            DescriptorPoolHook,
            originalDescriptorPool
        );
        BNM::BasicHook(
            descriptorToEffect,
            DescriptorToEffectHook,
            originalDescriptorToEffect
        );
        BNM::BasicHook(
            registerHandlers,
            RegisterHandlersHook,
            originalRegisterHandlers
        );
        BNM::BasicHook(
            registerForEffect,
            RegisterForEffectHook,
            originalRegisterForEffect
        );
        BNM::BasicHook(
            processAbility,
            DrawSubsetAbilityHook,
            originalDrawSubsetAbility
        );
        BNM::BasicHook(
            processSubset,
            ProcessSubsetHook,
            originalProcessSubset
        );
        BNM::BasicHook(
            processDraw,
            ProcessDrawHook,
            originalProcessDraw
        );
        BNM::BasicHook(
            pickRandomGuid,
            PickRandomGuidHook,
            originalPickRandomGuid
        );

        if (!originalDescriptorCopy || !originalDescriptorPool ||
            !originalDescriptorToEffect ||
            !originalRegisterForEffect || !originalDrawSubsetAbility ||
            !originalProcessSubset || !originalProcessDraw ||
            !originalPickRandomGuid || !originalRegisterHandlers) {
            LOG_ERR(
                "CustomComponents/DrawCardFromSubsetInDeck: hook installation failed"
            );
            return false;
        }

        LOG_INFO(
            "CustomComponents/DrawCardFromSubsetInDeck: ready"
        );
        return true;
    }

    inline bool Initialize() {
        return RegisterTypes() && InstallHooks();
    }

} // namespace CustomComponents::Systems::DrawCardFromSubsetInDeckSystem
