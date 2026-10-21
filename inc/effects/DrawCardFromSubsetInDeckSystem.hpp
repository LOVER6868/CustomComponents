#pragma once
#include "diagnostics/DiagnosticLogging.hpp"

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

#include "effects/DrawCardFromSubsetInDeckEffectDescriptor.hpp"
#include "effects/DrawCardFromSubsetInDeckEffect.hpp"
#include "core/CustomComponentRegistry.hpp"
#include "core/CustomFieldBridgeSystem.hpp"
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

    #include "effects/detail/DeckDrawStateAndHelpers.inl"
    #include "effects/detail/DrawProducerContext.inl"
    #include "effects/detail/DrawDescriptorBridge.inl"
    #include "effects/detail/SubsetDrawExecution.inl"
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

        CC_DIAG(
            "CustomComponents/DrawCardFromSubsetInDeck: ready"
        );
        return true;
    }

    inline bool Initialize() {
        return RegisterTypes() && InstallHooks();
    }

} // namespace CustomComponents::Systems::DrawCardFromSubsetInDeckSystem
