#pragma once
#include "diagnostics/DiagnosticLogging.hpp"

#include <array>
#include <cstring>
#include <cstdint>
#include <string>

#include <BNM/BasicMonoStructures.hpp>
#include <BNM/Class.hpp>
#include <BNM/Field.hpp>
#include <BNM/Method.hpp>
#include <dobby.h>

#include "triggers/ReturnedFromDiscardPileTrigger.hpp"
#include "triggers/PickFromThreeTrigger.hpp"
#include "zones/DiscardProvenanceSystem.hpp"
#include "triggers/DiscardOutsidePlayTrigger.hpp"
#include "triggers/CustomTrigger.hpp"
#include "triggers/FireTriggerEffectDescriptor.hpp"
#include "triggers/FireCustomTriggerEffectDescriptor.hpp"
#include "core/CustomComponentRegistry.hpp"
#include "core/CustomFieldBridgeSystem.hpp"
#include "presentation/ZoneTriggeredAbilityPresentationSystem.hpp"
#include "diagnostics/DiagnosticLogging.hpp"
#include "log.h"

namespace CustomComponents::Systems::CustomTriggerSystem {

    using Obj = BNM::IL2CPP::Il2CppObject;
    template<typename T> using MonoList = BNM::Structures::Mono::List<T>;

    // Capture coherent stock TriggerSystem<T> producer contexts.
    //
    // Stock TriggerSystem<T> owns the InstanceManager, TriggeredEffectCollection,
    // and GenericEffectEntityFinder used to CREATE TriggerData.  The separate
    // TriggeredEffectQueuingSystem is only the later consumer of that shared
    // collection and must not be used as our producer context.
    // PvZ Heroes constructs many TriggerSystem<T> instances per live/prediction
    // EntityModel. Sixteen entries is far too small: later prediction-model
    // registrations evict the real match model before a custom event fires.
    // Keep a much larger recency window; matching still scans newest-first.
    inline constexpr std::size_t kMaxRuntimeContexts = 256;

    struct StockTriggerContext {
        Obj* triggerSystem = nullptr;
        Obj* instances = nullptr;
        Obj* triggeredEffects = nullptr;
        Obj* finder = nullptr;
        Obj* model = nullptr;
    };

    inline std::array<StockTriggerContext, kMaxRuntimeContexts> stockTriggerContexts{};
    inline std::size_t stockTriggerContextCount = 0;

    inline constexpr std::size_t kMaxCarrierMetadata = 64;

    struct CarrierMetadata {
        Obj* effect = nullptr;
        BNM::Structures::Mono::String* triggerName = nullptr;
        bool ignoreTriggerConditions = false;
        bool targeted = false;
    };

    inline std::array<CarrierMetadata, kMaxCarrierMetadata> carrierMetadata{};

    inline void StoreCarrierMetadata(
        Obj* effect,
        BNM::Structures::Mono::String* triggerName,
        bool ignoreTriggerConditions,
        bool targeted
    ) {
        if (!effect)
            return;

        for (auto& entry : carrierMetadata) {
            if (entry.effect == effect || !entry.effect) {
                entry.effect = effect;
                entry.triggerName = triggerName;
                entry.ignoreTriggerConditions = ignoreTriggerConditions;
                entry.targeted = targeted;
                return;
            }
        }

        LOG_ERR(
            "CustomComponents/CustomTrigger/Carrier: metadata table full; effect=%p",
            effect
        );
    }

    inline bool TakeCarrierMetadata(
        Obj* effect,
        CarrierMetadata& out
    ) {
        if (!effect)
            return false;

        for (auto& entry : carrierMetadata) {
            if (entry.effect != effect)
                continue;

            out = entry;
            entry = {};
            return true;
        }

        return false;
    }

    // Shared TriggerSystem<object> native layout in this build:
    //   +0x10 InstanceManager
    //   +0x18 TriggeredEffectCollection
    //   +0x20 EffectEntityFinder
    //
    // GenericEffectEntityFinder<T>:
    //   +0x20 EntityModel
    inline constexpr std::uintptr_t kTriggerSystemInstancesOffset = 0x10;
    inline constexpr std::uintptr_t kTriggerSystemTriggeredEffectsOffset = 0x18;
    inline constexpr std::uintptr_t kTriggerSystemFinderOffset = 0x20;

    inline BNM::Field<MonoList<Obj*>*> modelEntities;
    inline BNM::Field<MonoList<Obj*>*> entityComponents;
    inline BNM::Field<int> entityId;

    inline BNM::Field<BNM::Structures::Mono::String*> customTriggerName;

    inline BNM::Field<int> effectEffectEntityId;
    inline BNM::Field<BNM::Structures::Mono::String*> fireTriggerName;
    inline BNM::Field<bool> fireTriggerIgnoreTriggerConditions;
    inline BNM::Field<BNM::Structures::Mono::String*> fireCustomTriggerName;

    inline BNM::Field<int> effectSourceId;
    inline BNM::Field<int> effectTargetId;

    inline BNM::Field<int> triggerDataEffectEntityId;
    inline BNM::Field<int> triggerDataSourceId;
    inline BNM::Field<Obj*> triggerDataContext;

    inline BNM::Method<Obj*> acquire;
    inline BNM::Method<void> returnToPool;
    inline BNM::Method<Obj*> effectToEntityQueryContext;
    inline BNM::Method<void> effectDataCopyFrom;
    inline BNM::Method<bool> triggerValidatorShouldTrigger;
    inline Obj* entityQueryContextType = nullptr;
    inline BNM::Method<void> effectContextCopyFromQuery;
    inline BNM::Method<int> effectEntityGetSource;
    inline BNM::Method<void> triggeredEffectsAdd;

    // Read-only Pick3 execution-gate probe. Capture the stock queuing system
    // at registration; fields are lazily populated by DI after registration.
    inline BNM::Method<bool> gateHasTriggers;
    inline BNM::Method<bool> gateHasNext;
    inline BNM::Method<bool> gateShiftWillChangeGroup;
    using RegisterHandlersFn = void (*)(void*, Obj*, const void*);
    using ProcessEffectFn = void (*)(void*, Obj*, const void*);
    inline RegisterHandlersFn originalQueueRegisterHandlers = nullptr;
    inline ProcessEffectFn originalQueueProcessEffect = nullptr;
    inline std::array<Obj*, 48> gateQueueSystems{};
    inline std::size_t gateQueueSystemCount = 0;
    inline Obj* gateObservedTriggers = nullptr;
    inline int gateObservedRemaining = 0;

    inline Obj* GateField(Obj* obj, std::uintptr_t offset) {
        return obj ? *reinterpret_cast<Obj**>(
            reinterpret_cast<std::uint8_t*>(obj) + offset) : nullptr;
    }
    // Passive selection-resume trace. Armed before dispatch, matched by the
    // exact service-owned EffectCollection; never drives processing.
    using ProcessEffectsFn = void (*)(void*, Obj*, const void*);
    inline ProcessEffectsFn originalEffectCollectionProcessEffects = nullptr;
    inline Obj* resumeObservedEffects = nullptr;
    inline int resumeObservationBudget = 0;
    inline int resumeProcessorDepth = 0;
    inline void WatchPick3ResumeEffects(Obj* effects, int effectId, int groupId) {
        resumeObservedEffects = effects;
        resumeObservationBudget = effects ? 24 : 0;
        LOG_INFO("CustomComponents/Pick3/PICK3_RESUME: phase=ARM effect=%d group=%d effects=%p",
                 effectId, groupId, (void*)effects);
    }
    inline void LogPick3ServiceProcess(const char* phase, Obj* service) {
        Obj* effects = GateField(service, 0x30);
        if (effects && effects == resumeObservedEffects && resumeObservationBudget > 0)
            LOG_INFO("CustomComponents/Pick3/PICK3_RESUME: phase=%s service=%p effects=%p depth=%d",
                     phase, (void*)service, (void*)effects, resumeProcessorDepth);
    }
    static void EffectCollectionProcessEffectsTrace(void* self, Obj* effects, const void* method) {
        const bool observed = effects && effects == resumeObservedEffects && resumeObservationBudget > 0;
        if (observed) {
            LOG_INFO("CustomComponents/Pick3/PICK3_RESUME: phase=PROCESS_EFFECTS_ENTER processor=%p effects=%p depth=%d",
                     self, (void*)effects, resumeProcessorDepth);
            ++resumeProcessorDepth;
        }
        originalEffectCollectionProcessEffects(self, effects, method);
        if (observed) {
            --resumeProcessorDepth;
            LOG_INFO("CustomComponents/Pick3/PICK3_RESUME: phase=PROCESS_EFFECTS_EXIT processor=%p effects=%p depth=%d",
                     self, (void*)effects, resumeProcessorDepth);
            --resumeObservationBudget;
        }
    }
    inline void LogPick3QueueGate(const char* phase, Obj* expectedTriggers) {
        if (!gateHasTriggers.IsValid() || !gateHasNext.IsValid() ||
            !gateShiftWillChangeGroup.IsValid() || !expectedTriggers) return;
        Obj* matchedSystem = nullptr;
        Obj* effects = nullptr;
        for (std::size_t i = 0; i < gateQueueSystemCount; ++i) {
            Obj* candidate = gateQueueSystems[i];
            if (GateField(candidate, 0x18) != expectedTriggers) continue;
            // Queue system and producer share the exact injected collection.
            matchedSystem = candidate;
            effects = GateField(candidate, 0x10);
            break;
        }
        if (!matchedSystem || !effects) {
            LOG_INFO("CustomComponents/Pick3/PICK3_QUEUE_GATE: phase=%s matched=0 triggered=%p candidates=%zu",
                     phase, (void*)expectedTriggers, gateQueueSystemCount);
            return;
        }
        const bool pending = gateHasTriggers[expectedTriggers]();
        const bool hasNext = gateHasNext[effects]();
        const bool groupReady = gateShiftWillChangeGroup[effects]();
        LOG_INFO("CustomComponents/Pick3/PICK3_QUEUE_GATE: phase=%s matched=1 system=%p triggered=%p effects=%p hasTriggers=%d hasNext=%d shiftWillChangeGroup=%d",
                 phase, (void*)matchedSystem, (void*)expectedTriggers, (void*)effects,
                 int(pending), int(hasNext), int(groupReady));
    }
    inline void ObservePick3QueueGate(Obj* triggeredEffects) {
        if (!triggeredEffects) return;
        gateObservedTriggers = triggeredEffects;
        gateObservedRemaining = 8;
        LogPick3QueueGate("AFTER_CUSTOM_FIRE", gateObservedTriggers);
    }
    static void QueueRegisterHandlersGateHook(void* self, Obj* router, const void* method) {
        Obj* system = static_cast<Obj*>(self);
        bool exists = false;
        for (std::size_t i=0; i<gateQueueSystemCount; ++i)
            if (gateQueueSystems[i] == system) { exists = true; break; }
        if (!exists) {
            if (gateQueueSystemCount < gateQueueSystems.size())
                gateQueueSystems[gateQueueSystemCount++] = system;
            else {
                for (std::size_t i=1; i<gateQueueSystems.size(); ++i)
                    gateQueueSystems[i-1] = gateQueueSystems[i];
                gateQueueSystems.back() = system;
            }
        }
        originalQueueRegisterHandlers(self, router, method);
    }
    // The existing queue-system hook can admit one verified live selection
    // during the original service's synchronous stock processing pass.
    inline Obj* liveSelectionQueueScope = nullptr;
    inline bool (*liveSelectionQueueCallback)(Obj*) = nullptr;
    inline bool liveSelectionQueueBusy = false;
    static void QueueProcessEffectGateHook(void* self, Obj* effect, const void* method) {
        Obj* triggers = GateField(static_cast<Obj*>(self), 0x18);
        if (triggers && triggers == liveSelectionQueueScope &&
            liveSelectionQueueCallback && !liveSelectionQueueBusy) {
            liveSelectionQueueBusy = true;
            liveSelectionQueueCallback(triggers);
            liveSelectionQueueBusy = false;
        }
        const bool observed = gateObservedRemaining > 0 && triggers && triggers == gateObservedTriggers;
        if (observed) LogPick3QueueGate("STOCK_PROCESS_ENTER", triggers);
        originalQueueProcessEffect(self, effect, method);
        if (observed) {
            LogPick3QueueGate("STOCK_PROCESS_EXIT", triggers);
            --gateObservedRemaining;
            if (gateObservedRemaining == 0 || !gateHasTriggers[triggers]()) {
                gateObservedTriggers = nullptr;
                gateObservedRemaining = 0;
            }
        }
    }


    using DescriptorToEffectFn = Obj* (*)(void*, Obj*, Obj*, const void*);
    inline RegisterHandlersFn originalTriggerSystemRegisterHandlers = nullptr;
    inline ProcessEffectFn originalStateCheckProcessEffect = nullptr;
    inline DescriptorToEffectFn originalStockTriggerCarrierToEffect = nullptr;

    inline constexpr std::uintptr_t kQueueRegisterHandlersRva = 0x220611C;
    inline constexpr std::uintptr_t kTriggerSystemRegisterHandlersRva = 0x2C4DBF8;
    inline constexpr std::uintptr_t kGenericFinderModelOffset = 0x20;
    inline constexpr std::uintptr_t kGenericFinderValidatorOffset = 0x28;

    // Verified from dump.cs + ARM64:
    // StateCheckSystem::EffectAdder          = this + 0x10
    // StateCheckEffectAdder::Instances       = adder + 0x10
    inline constexpr std::uintptr_t kStateCheckEffectAdderOffset = 0x10;
    inline constexpr std::uintptr_t kStateCheckAdderInstancesOffset = 0x10;

    // The StateCheckSystem currently processing an effect is our strongest
    // live-simulation scope anchor. Keep it per-thread because prediction work
    // also runs StateCheck processing on worker threads.
    inline thread_local Obj* activeStateCheckInstances = nullptr;

    #include "triggers/detail/TriggerProducerContext.inl"
    #include "triggers/detail/CustomTriggerDispatch.inl"
    #include "triggers/detail/CustomTriggerCarrier.inl"
    inline bool RegisterTypes() {
        struct Registration {
            const char* name;
            const char* baseName;
        };

        const Registration registrations[] = {
            {"ReturnedFromDiscardPileTrigger", "MarkerComponent"},
            {"PickFromThreeTrigger", "MarkerComponent"},
            {"DiscardOutsidePlayTrigger", "MarkerComponent"},
            {"CustomTrigger", "MarkerComponent"},
            {"FireTriggerEffectDescriptor", "ReturnToHandFromPlayEffectDescriptor"},
            {"FireCustomTriggerEffectDescriptor", "ReturnToHandFromPlayEffectDescriptor"}
        };

        for (const auto& registration : registrations) {
            if (CustomComponentRegistry::FindByName(
                    "PvZCards.Engine.Components",
                    registration.name)) {
                continue;
            }

            if (!CustomComponentRegistry::Register({
                    registration.name,
                    "PvZCards.Engine.Components",
                    "EngineLib.dll",
                    registration.baseName,
                    "PvZCards.Engine.Components",
                    "EngineLib.dll"
                })) {
                LOG_ERR(
                    "CustomComponents/CustomTrigger: failed to register %s base=%s",
                    registration.name,
                    registration.baseName
                );
                return false;
            }

            CC_DIAG(
                "CustomComponents/CustomTrigger: registered %s base=%s",
                registration.name,
                registration.baseName
            );
        }

        if (!CustomFieldBridgeSystem::Register(
                "PvZCards.Engine.Components",
                "CustomTrigger",
                "PvZCards.Engine.Components",
                "MarkerComponent",
                {
                    {"TriggerName", CustomFieldBridgeSystem::FieldKind::String}
                },
                false) ||
            !CustomFieldBridgeSystem::Register(
                "PvZCards.Engine.Components",
                "FireTriggerEffectDescriptor",
                "PvZCards.Engine.Components",
                "ReturnToHandFromPlayEffectDescriptor",
                {
                    {"TriggerName", CustomFieldBridgeSystem::FieldKind::String},
                    {"IgnoreTriggerConditions", CustomFieldBridgeSystem::FieldKind::Bool}
                },
                false) ||
            !CustomFieldBridgeSystem::Register(
                "PvZCards.Engine.Components",
                "FireCustomTriggerEffectDescriptor",
                "PvZCards.Engine.Components",
                "ReturnToHandFromPlayEffectDescriptor",
                {
                    {"TriggerName", CustomFieldBridgeSystem::FieldKind::String}
                },
                false)) {
            LOG_ERR(
                "CustomComponents/CustomTrigger: custom-field schema registration failed"
            );
            return false;
        }

        return true;
    }

    inline bool InstallHooks() {
        auto queueSystem = BNM::Class(
            "PvZCards.Core.Systems",
            "TriggeredEffectQueuingSystem"
        );
        auto stateCheckSystem = BNM::Class(
            "PvZCards.Core.Systems",
            "StateCheckSystem"
        );
        auto stockTriggerCarrierDescriptor = BNM::Class(
            "PvZCards.Engine.Components",
            "ReturnToHandFromPlayEffectDescriptor"
        );
        auto entity = BNM::Class("PvZCards.Engine", "Entity");
        auto customTrigger = BNM::Class("PvZCards.Engine.Components", "CustomTrigger");
        auto fireTriggerDescriptor = BNM::Class(
            "PvZCards.Engine.Components",
            "FireTriggerEffectDescriptor"
        );
        auto fireCustomTriggerDescriptor = BNM::Class(
            "PvZCards.Engine.Components",
            "FireCustomTriggerEffectDescriptor"
        );
        auto model = BNM::Class("PvZCards.Engine", "EntityModel");
        auto effect = BNM::Class("PvZCards.Engine", "Effect");
        auto triggerData = BNM::Class("PvZCards.Engine", "TriggerData");
        auto effectContext = BNM::Class("PvZCards.Engine", "EffectContext");
        auto effectData = BNM::Class("PvZCards.Engine", "EffectData");
        auto entityQueryContext = BNM::Class("PvZCards.Engine", "EntityQueryContext");
        auto triggerValidator = BNM::Class(
            "PvZCards.Core.Systems.Utilities",
            "TriggerValidator"
        );
        auto effectEntity = BNM::Class("PvZCards.Engine.Components", "EffectEntity");
        auto triggeredCollection = BNM::Class("PvZCards.Engine", "TriggeredEffectCollection");
        auto instanceManager = BNM::Class("PvZCards.Engine", "InstanceManager");

        if (!queueSystem.IsValid() || !stateCheckSystem.IsValid() ||
            !stockTriggerCarrierDescriptor.IsValid() ||
            !customTrigger.IsValid() || !fireTriggerDescriptor.IsValid() ||
            !fireCustomTriggerDescriptor.IsValid() ||
            !entity.IsValid() || !model.IsValid() ||
            !effect.IsValid() || !triggerData.IsValid() || !effectContext.IsValid() ||
            !effectData.IsValid() || !entityQueryContext.IsValid() ||
            !triggerValidator.IsValid() ||
            !effectEntity.IsValid() || !triggeredCollection.IsValid() ||
            !instanceManager.IsValid()) {
            LOG_ERR("CustomComponents/CustomTrigger: metadata class lookup failed");
            return false;
        }

        modelEntities = model.GetField("Entities");
        entityComponents = entity.GetField("Components");
        entityId = entity.GetField("<Id>k__BackingField");
        customTriggerName = customTrigger.GetField("TriggerName");

        effectSourceId = effect.GetField("SourceId");
        effectTargetId = effect.GetField("TargetId");
        effectEffectEntityId = effect.GetField("EffectEntityId");

        fireTriggerName = fireTriggerDescriptor.GetField("TriggerName");
        fireTriggerIgnoreTriggerConditions =
            fireTriggerDescriptor.GetField("IgnoreTriggerConditions");
        fireCustomTriggerName =
            fireCustomTriggerDescriptor.GetField("TriggerName");

        triggerDataEffectEntityId = triggerData.GetField("EffectEntityId");
        triggerDataSourceId = triggerData.GetField("SourceId");
        triggerDataContext = triggerData.GetField("Context");

        acquire = instanceManager.GetMethod("Acquire", 2).cast<Obj*>();
        returnToPool = instanceManager.GetMethod("Return", 1).cast<void>();
        effectToEntityQueryContext = effect.GetMethod("ToEntityQueryContext", 1).cast<Obj*>();
        effectDataCopyFrom = effectData.GetMethod("CopyFrom", 1).cast<void>();
        triggerValidatorShouldTrigger =
            triggerValidator.GetMethod("ShouldTriggerEffect", 2).cast<bool>();
        entityQueryContextType =
            reinterpret_cast<Obj*>(entityQueryContext.GetMonoType());
        effectContextCopyFromQuery = effectContext.GetMethod(
            "CopyFrom",
            {BNM::Class("PvZCards.Engine", "EntityQueryContext").GetCompileTimeClass()}
        ).cast<void>();
        effectEntityGetSource = effectEntity.GetMethod("GetSource", 1).cast<int>();
        triggeredEffectsAdd = triggeredCollection.GetMethod("Add", 1).cast<void>();
        gateHasTriggers = triggeredCollection.GetMethod("HasTriggers", 0).cast<bool>();
        auto concreteQueue = BNM::Class("PvZCards.Engine", "EffectQueue");
        if (concreteQueue.IsValid()) {
            gateHasNext = concreteQueue.GetMethod("HasNext", 0).cast<bool>();
            gateShiftWillChangeGroup = concreteQueue.GetMethod("ShiftWillChangeGroup", 0).cast<bool>();
        }


        auto queueRegisterHandlers = queueSystem.GetMethod("RegisterEffectHandlers", 1);
        auto stateCheckProcessEffect =
            stateCheckSystem.GetMethod("ProcessEffect", 1);
        auto stockTriggerCarrierToEffect =
            stockTriggerCarrierDescriptor.GetMethod("ToEffect", 2);

        // Recover libil2cpp's ASLR base from the concrete queue method, then
        // hook the shared TriggerSystem<T>.RegisterEffectHandlers body only to
        // remember TriggerSystem objects. Injected fields are resolved lazily
        // later; do not hook the tiny GC-write-barrier DI setter thunks.
        const std::uintptr_t queueRegisterAddress =
            static_cast<std::uintptr_t>(queueRegisterHandlers.GetOffset());
        const std::uintptr_t il2cppBase =
            queueRegisterAddress >= kQueueRegisterHandlersRva
                ? queueRegisterAddress - kQueueRegisterHandlersRva
                : 0;
        void* triggerSystemRegisterHandlersAddress = il2cppBase
            ? reinterpret_cast<void*>(
                il2cppBase + kTriggerSystemRegisterHandlersRva
              )
            : nullptr;

        const bool valid =
            modelEntities.IsValid() &&
            entityComponents.IsValid() &&
            entityId.IsValid() &&
            customTriggerName.IsValid() &&
            effectSourceId.IsValid() &&
            effectTargetId.IsValid() &&
            effectEffectEntityId.IsValid() &&
            fireTriggerName.IsValid() &&
            fireTriggerIgnoreTriggerConditions.IsValid() &&
            fireCustomTriggerName.IsValid() &&
            triggerDataEffectEntityId.IsValid() &&
            triggerDataSourceId.IsValid() &&
            triggerDataContext.IsValid() &&
            acquire.IsValid() &&
            returnToPool.IsValid() &&
            effectToEntityQueryContext.IsValid() &&
            effectDataCopyFrom.IsValid() &&
            triggerValidatorShouldTrigger.IsValid() &&
            entityQueryContextType != nullptr &&
            effectContextCopyFromQuery.IsValid() &&
            effectEntityGetSource.IsValid() &&
            triggeredEffectsAdd.IsValid() &&
            gateHasTriggers.IsValid() && gateHasNext.IsValid() &&
            gateShiftWillChangeGroup.IsValid() &&
            queueRegisterHandlers.IsValid() &&
            stateCheckProcessEffect.IsValid() &&
            stockTriggerCarrierToEffect.IsValid() &&
            triggerSystemRegisterHandlersAddress != nullptr;

        if (!valid) {
            LOG_ERR("CustomComponents/CustomTrigger: metadata method/field lookup failed");
            return false;
        }

        const int triggerSystemHookResult = DobbyHook(
            triggerSystemRegisterHandlersAddress,
            reinterpret_cast<void*>(TriggerSystemRegisterEffectHandlersHook),
            reinterpret_cast<void**>(&originalTriggerSystemRegisterHandlers)
        );
        if (triggerSystemHookResult != 0 || !originalTriggerSystemRegisterHandlers) {
            LOG_ERR(
                "CustomComponents/CustomTrigger: TriggerSystem<T>.RegisterEffectHandlers hook installation failed result=%d address=%p base=%p",
                triggerSystemHookResult,
                triggerSystemRegisterHandlersAddress,
                reinterpret_cast<void*>(il2cppBase)
            );
            return false;
        }

        // Observation only: record stock queuing-system scope and its next
        // ProcessEffect boundary; never invoke the router ourselves.
        auto queueProcess = queueSystem.GetMethod("ProcessEffect", 1);
        if (!queueProcess.IsValid()) return false;
        BNM::BasicHook(queueRegisterHandlers, QueueRegisterHandlersGateHook,
                       originalQueueRegisterHandlers);
        BNM::BasicHook(queueProcess, QueueProcessEffectGateHook,
                       originalQueueProcessEffect);
        if (!originalQueueRegisterHandlers || !originalQueueProcessEffect) {
            LOG_ERR("CustomComponents/Pick3/PICK3_QUEUE_GATE: stock hooks unavailable");
            return false;
        }
        LOG_INFO("CustomComponents/Pick3/PICK3_QUEUE_GATE: SETUP ready=1 passive=1");
        auto effectProcessor = BNM::Class("PvZCards.Engine", "EffectCollectionProcessor");
        // dump.cs declares this class in PvZCards.Core; fail closed if the
        // method cannot be resolved (other diagnostics remain functional).
        auto processEffects = effectProcessor.GetMethod("ProcessEffects", 1);
        // Safety rollback: this additional global ProcessEffects hook was
        // followed by a stock Discarded hook installation failure during
        // stage 3 startup. Retain the existing queue-gate observation but
        // do not install this competing processor hook.
        (void)processEffects;
        LOG_INFO("CustomComponents/Pick3/PICK3_RESUME: SETUP processorHook=0 passive=1 rollback=discarded-hook-conflict");


        CC_DIAG(
            "CustomComponents/CustomTrigger: hooked TriggerSystem<T>.RegisterEffectHandlers address=%p base=%p (lazy producer refresh)",
            triggerSystemRegisterHandlersAddress,
            reinterpret_cast<void*>(il2cppBase)
        );

        // FireTriggerEffectDescriptor / FireCustomTriggerEffectDescriptor derive
        // from ReturnToHandFromPlayEffectDescriptor solely to inherit a known-good
        // EffectDescriptor virtual slot. We MUST intercept that stock ToEffect()
        // body; otherwise the inherited implementation literally returns the
        // selected card to hand ("bounce").
        BNM::BasicHook(
            stockTriggerCarrierToEffect,
            StockTriggerCarrierToEffectHook,
            originalStockTriggerCarrierToEffect
        );
        if (!originalStockTriggerCarrierToEffect) {
            LOG_ERR(
                "CustomComponents/CustomTrigger: stock EffectDescriptor carrier hook installation failed"
            );
            return false;
        }

        CC_DIAG(
            "CustomComponents/CustomTrigger: stock EffectDescriptor carrier hook ready address=%p",
            reinterpret_cast<void*>(
                static_cast<std::uintptr_t>(stockTriggerCarrierToEffect.GetOffset())
            )
        );

        BNM::BasicHook(
            stateCheckProcessEffect,
            StateCheckProcessEffectHook,
            originalStateCheckProcessEffect
        );
        if (!originalStateCheckProcessEffect) {
            LOG_ERR(
                "CustomComponents/CustomTrigger: StateCheckSystem.ProcessEffect hook installation failed"
            );
            return false;
        }

        CC_DIAG("CustomComponents/CustomTrigger: runtime dispatcher ready (StateCheck scoped producer + direct TriggerValidator path + carrier bridge)");
        return true;
    }

    inline bool Initialize() {
        return RegisterTypes() && InstallHooks();
    }

} // namespace CustomComponents::Systems::CustomTriggerSystem
