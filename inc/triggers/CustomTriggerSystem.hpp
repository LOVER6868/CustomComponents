#include "core/HookRegistry.hpp"
#include "engine/EffectExecution.hpp"
#pragma once
#include "diagnostics/DiagnosticLogging.hpp"

#include <array>
#include <atomic>
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
#include "triggers/ReturnToLaneMoveFilter.hpp"
#include "core/ManagedRuntime.hpp"
#include "engine/TriggerQueue.hpp"
#include "pick3/SelectionExecutionScope.hpp"
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
#include "pick3/SelectionTransactionRuntime.hpp"
#include "pick3/SelectionAcceptance.hpp"

namespace CustomComponents::Systems::CustomTriggerSystem {

    using Obj = BNM::IL2CPP::Il2CppObject;
    // Source-agnostic, stock-validated selection producer. The callback may
    // recognize a completed initial Play or another exact in-flight selection;
    // it MUST reject ambiguous, uncommitted, or unrelated source/effect/target.
    using InFlightSelectionProof = bool (*)(Obj* model, int sourceCard,
                                           int effectId, int selectedTargetId);
    inline InFlightSelectionProof inFlightSelectionProof = nullptr;
    inline void RegisterInFlightSelectionProof(InFlightSelectionProof proof) {
        inFlightSelectionProof = proof;
    }

    template<typename T> using MonoList = BNM::Structures::Mono::List<T>;

    // Capture coherent stock TriggerSystem<T> producer contexts.
    //
    // Stock TriggerSystem<T> owns the InstanceManager, TriggeredEffectCollection,
    // and GenericEffectEntityFinder used to CREATE TriggerData.  The separate
    // TriggeredEffectQueuingSystem is only the later consumer of that shared
    // collection and must not be used as our producer context.
    // PvZ Heroes constructs many TriggerSystem<T> instances per live/prediction
    // EntityModel. Keep weak managed references without evicting a still-live
    // producer. Lookup pins each candidate while reading its injected fields.

    struct StockTriggerContext {
        std::shared_ptr<Core::ManagedRuntime::Root> lifetime;
        Obj* triggerSystem = nullptr;
        Obj* instances = nullptr;
        Obj* triggeredEffects = nullptr;
        Obj* finder = nullptr;
        Obj* model = nullptr;
    };

    inline std::mutex producerLock;
    inline std::vector<std::shared_ptr<Core::ManagedRuntime::Root>> producerReferences;

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
        CC_SELECTION_DIAG("CustomComponents/Pick3/PICK3_RESUME: phase=ARM effect=%d group=%d effects=%p",
                 effectId, groupId, (void*)effects);
    }
    inline void LogPick3ServiceProcess(const char* phase, Obj* service) {
        Obj* effects = GateField(service, 0x30);
        if (effects && effects == resumeObservedEffects && resumeObservationBudget > 0)
            CC_SELECTION_DIAG("CustomComponents/Pick3/PICK3_RESUME: phase=%s service=%p effects=%p depth=%d",
                     phase, (void*)service, (void*)effects, resumeProcessorDepth);
    }
    static void EffectCollectionProcessEffectsTrace(void* self, Obj* effects, const void* method) {
        const bool observed = effects && effects == resumeObservedEffects && resumeObservationBudget > 0;
        if (observed) {
            CC_SELECTION_DIAG("CustomComponents/Pick3/PICK3_RESUME: phase=PROCESS_EFFECTS_ENTER processor=%p effects=%p depth=%d",
                     self, (void*)effects, resumeProcessorDepth);
            ++resumeProcessorDepth;
        }
        originalEffectCollectionProcessEffects(self, effects, method);
        if (observed) {
            --resumeProcessorDepth;
            CC_SELECTION_DIAG("CustomComponents/Pick3/PICK3_RESUME: phase=PROCESS_EFFECTS_EXIT processor=%p effects=%p depth=%d",
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
            CC_SELECTION_DIAG("CustomComponents/Pick3/PICK3_QUEUE_GATE: phase=%s matched=0 triggered=%p candidates=%zu",
                     phase, (void*)expectedTriggers, gateQueueSystemCount);
            return;
        }
        const bool pending = gateHasTriggers[expectedTriggers]();
        const bool hasNext = gateHasNext[effects]();
        const bool groupReady = gateShiftWillChangeGroup[effects]();
        CC_SELECTION_DIAG("CustomComponents/Pick3/PICK3_QUEUE_GATE: phase=%s matched=1 system=%p triggered=%p effects=%p hasTriggers=%d hasNext=%d shiftWillChangeGroup=%d",
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
        CustomComponents::Pick3::TransactionRuntime::RegisterRouter(
            router,CustomComponents::Pick3::TransactionRuntime::ObjectAt(system,0x30));
        originalQueueRegisterHandlers(self, router, method);
    }
    // The existing queue-system hook can admit one verified live selection
    // during the original service's synchronous stock processing pass.
    inline Obj* liveSelectionQueueScope = nullptr;
    inline bool (*liveSelectionQueueCallback)(Obj*) = nullptr;
    inline bool liveSelectionQueueBusy = false;
    // A selection-scoped audit window, armed by the existing selection-service
    // hook rather than by custom trigger firing. The queue may process work on
    // a different thread after the service returns; use atomic counters.
    inline std::atomic<unsigned> pick3QueueWindow{0};
    inline std::atomic<unsigned> pick3QueueBudget{0};
    inline std::atomic<unsigned> pick3QueueSequence{0};
    inline std::atomic<int> pick3QueueExpectedEffect{-1};
    inline std::atomic<int> pick3QueueExpectedGroup{-1};
    inline std::atomic<unsigned> pick3QueueMatchedEffects{0};
    inline thread_local unsigned pickFromThreeServiceDepth = 0;
    inline void ArmPick3QueueAudit(const char* reason, int effectId, int groupId) {
        const unsigned window = pick3QueueWindow.fetch_add(1, std::memory_order_relaxed) + 1;
        pick3QueueExpectedEffect.store(effectId, std::memory_order_release);
        pick3QueueExpectedGroup.store(groupId, std::memory_order_release);
        pick3QueueMatchedEffects.store(0, std::memory_order_release);
        pick3QueueBudget.store(96, std::memory_order_release);
        CC_SELECTION_DIAG("CustomComponents/Pick3/PICK3_QUEUE_WINDOW: phase=ARM id=%u reason=%s effect=%d group=%d budget=96",
                 window, reason, effectId, groupId);
    }
    inline bool TakePick3QueueAudit(unsigned& window, unsigned& seq) {
        if constexpr(!Diagnostics::kSelectionTrace)return false;
        unsigned remaining = pick3QueueBudget.load(std::memory_order_acquire);
        while (remaining && !pick3QueueBudget.compare_exchange_weak(
            remaining, remaining - 1, std::memory_order_acq_rel,
            std::memory_order_acquire)) {}
        if (!remaining) return false;
        window = pick3QueueWindow.load(std::memory_order_acquire);
        seq = pick3QueueSequence.fetch_add(1, std::memory_order_relaxed) + 1;
        return true;
    }
    static void QueueProcessEffectGateHook(void* self, Obj* effect, const void* method) {
        Obj* triggers = GateField(static_cast<Obj*>(self), 0x18);
        if (triggers && triggers == liveSelectionQueueScope &&
            liveSelectionQueueCallback && !liveSelectionQueueBusy) {
            liveSelectionQueueBusy = true;
            liveSelectionQueueCallback(triggers);
            liveSelectionQueueBusy = false;
        }
        const bool observed = gateObservedRemaining > 0 && triggers && triggers == gateObservedTriggers;
        unsigned window = 0, seq = 0;
        const bool independentSample = TakePick3QueueAudit(window, seq);
        if (independentSample && effect) {
            // ProcessEffect takes a stock Effect. Read the same fixed Effect
            // fields already used by the trigger dispatcher, with no managed
            // calls, allocation, or dispatch. A queue window is diagnostic,
            // not evidence of authoritative server acceptance.
            const auto* effectBytes = reinterpret_cast<const std::uint8_t*>(effect);
            const int sourceId = *reinterpret_cast<const int*>(effectBytes + 0x24);
            const int targetId = *reinterpret_cast<const int*>(effectBytes + 0x28);
            const int springboardId = *reinterpret_cast<const int*>(effectBytes + 0x2C);
            const int effectId = *reinterpret_cast<const int*>(effectBytes + 0x30);
            const int groupId = *reinterpret_cast<const int*>(effectBytes + 0x3C);
            const int wantedEffect = pick3QueueExpectedEffect.load(std::memory_order_acquire);
            const int wantedGroup = pick3QueueExpectedGroup.load(std::memory_order_acquire);
            const bool matches = wantedEffect >= 0 && wantedGroup >= 0 &&
                effectId == wantedEffect && groupId == wantedGroup;
            if (matches) pick3QueueMatchedEffects.fetch_add(1, std::memory_order_relaxed);
            CustomComponents::Pick3::Provenance::Queue(effectId, groupId, sourceId, targetId,
                pickFromThreeServiceDepth != 0);
            CC_SELECTION_DIAG("CustomComponents/Pick3/PICK3_QUEUE_IDENTITY: window=%u seq=%u source=%d target=%d springboard=%d effectId=%d group=%d expectedEffect=%d expectedGroup=%d exact=%d duringSelectService=%d",
                window, seq, sourceId, targetId, springboardId,
                effectId, groupId, wantedEffect, wantedGroup,
                int(matches), int(pickFromThreeServiceDepth != 0));
        }
        if (independentSample) {
            CC_SELECTION_DIAG("CustomComponents/Pick3/PICK3_STOCK_QUEUE: phase=ENTER window=%u seq=%u duringSelectService=%d queue=%p effect=%p triggers=%p observedGate=%d",
                     window, seq, int(pickFromThreeServiceDepth != 0),
                     self, effect, triggers, int(observed));
        }
        // Individual effect handoff, observed at an already-installed stock
        // queue hook. No new detour, managed getter, or effect dispatch.
        if (observed) {
            CC_SELECTION_DIAG("CustomComponents/Pick3/PICK3_TRIGGER_TIMELINE: phase=QUEUE_EFFECT_ENTER queue=%p effects=%p effect=%p triggers=%p remaining=%d",
                     self, GateField(static_cast<Obj*>(self), 0x10), effect, triggers, gateObservedRemaining);
            LogPick3QueueGate("STOCK_PROCESS_ENTER", triggers);
        }
        // Patch 2.2: use the already-installed stock execution detour.
        // The scoped observer only compares immutable effect/group IDs with
        // snapshot history; it never changes return values or queue state.
        const auto* executionBytes = reinterpret_cast<const std::uint8_t*>(effect);
        const int executionEffect = executionBytes
            ? *reinterpret_cast<const int*>(executionBytes + 0x30) : -1;
        const int executionGroup = executionBytes
            ? *reinterpret_cast<const int*>(executionBytes + 0x3C) : -1;
        const int executionSource = executionBytes
            ? *reinterpret_cast<const int*>(executionBytes + 0x24) : -1;
        const int executionTarget = executionBytes
            ? *reinterpret_cast<const int*>(executionBytes + 0x28) : -1;
        {
            CustomComponents::Pick3::Provenance::ScopedQueueExecution executionScope(
                executionEffect, executionGroup, executionSource, executionTarget);
            Engine::EffectExecution::Scope functionalExecution(executionGroup);
            CustomComponents::Pick3::Picked::DeferredLink::ScopedExecution linkedSelection(
                executionEffect, executionSource, executionTarget);
            originalQueueProcessEffect(self, effect, method);
        }
        if (independentSample) {
            CC_SELECTION_DIAG("CustomComponents/Pick3/PICK3_STOCK_QUEUE: phase=EXIT window=%u seq=%u duringSelectService=%d queue=%p effect=%p",
                     window, seq, int(pickFromThreeServiceDepth != 0), self, effect);
        }
        if (observed) {
            CC_SELECTION_DIAG("CustomComponents/Pick3/PICK3_TRIGGER_TIMELINE: phase=QUEUE_EFFECT_EXIT queue=%p effect=%p triggers=%p remaining=%d",
                     self, effect, triggers, gateObservedRemaining);
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

        const int triggerSystemHookResult = CustomComponents::Core::Hooks::InstallNative(
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
        CustomComponents::Core::Hooks::Install(queueRegisterHandlers, QueueRegisterHandlersGateHook,
                       originalQueueRegisterHandlers);
        CustomComponents::Core::Hooks::Install(queueProcess, QueueProcessEffectGateHook,
                       originalQueueProcessEffect);
        if (!originalQueueRegisterHandlers || !originalQueueProcessEffect) {
            LOG_ERR("CustomComponents/Pick3/PICK3_QUEUE_GATE: stock hooks unavailable");
            return false;
        }
        CC_SELECTION_DIAG("CustomComponents/Pick3/PICK3_QUEUE_GATE: SETUP ready=1 passive=1");
        auto effectProcessor = BNM::Class("PvZCards.Engine", "EffectCollectionProcessor");
        // dump.cs declares this class in PvZCards.Core; fail closed if the
        // method cannot be resolved (other diagnostics remain functional).
        auto processEffects = effectProcessor.GetMethod("ProcessEffects", 1);
        // Safety rollback: this additional global ProcessEffects hook was
        // followed by a stock Discarded hook installation failure during
        // stage 3 startup. Retain the existing queue-gate observation but
        // do not install this competing processor hook.
        (void)processEffects;
        CC_SELECTION_DIAG("CustomComponents/Pick3/PICK3_RESUME: SETUP processorHook=0 passive=1 rollback=discarded-hook-conflict");


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
        CustomComponents::Core::Hooks::Install(
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

        CustomComponents::Core::Hooks::Install(
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
        if(!Core::ManagedRuntime::Initialize()) return false;
        const bool installed=RegisterTypes() && InstallHooks();
        if(installed) {
            Engine::TriggerQueue::AddFilter(+[](const Engine::TriggerQueue::Context& queued) {
                for(auto& producer:SnapshotStockTriggerContexts()) {
                    if(producer.triggeredEffects!=queued.collection) continue;
                    Obj* context=triggerDataContext[queued.data].Get();
                    const auto event=CustomComponents::Pick3::TransactionRuntime::ReadContext(context);
                    return ReturnToLaneMoveFilter::ShouldSuppress(producer.model,
                        triggerDataEffectEntityId[queued.data].Get(),event.effect,FindEntityById,HasNamedComponent);
                }
                return false;
            });
            Engine::TriggerQueue::recycle=+[](const Engine::TriggerQueue::Context& queued) {
                for(auto& producer:SnapshotStockTriggerContexts()) if(producer.triggeredEffects==queued.collection) {
                    returnToPool[producer.instances](queued.data);return true;
                }
                return false;
            };
            const bool transactionReady=CustomComponents::Pick3::TransactionRuntime::Initialize();
#if CUSTOMCOMPONENTS_PF3_TRANSACTION_PIPELINE
            CustomComponents::Pick3::TransactionRuntime::pipelineReady=transactionReady && CustomComponents::Pick3::Acceptance::Initialize();
#else
            CustomComponents::Pick3::TransactionRuntime::pipelineReady=false;
#endif
            if(!transactionReady) LOG_ERR("CustomComponents/Pick3/PF3_TX: phase=INIT_FAILED baselinePreserved=1");
            if(CustomComponents::Pick3::TransactionRuntime::pipelineReady) CustomComponents::Pick3::TransactionRuntime::onStockAccepted =
                +[](Obj* model,Obj* effect,CustomComponents::Pick3::Transaction::Ref value,
                    CustomComponents::Pick3::Transaction::Context context,int group) {
                    if(!value || value->cards.size()!=1) return;
                    const auto& card=value->cards.front();
                    const int target=CustomComponents::Pick3::Transaction::LiveTarget(value,0);
                    if(target<=0) {
                        CustomComponents::Pick3::TransactionRuntime::AwaitCreation(value,context,group);
                        CC_SELECTION_DIAG("CustomComponents/Pick3/PF3_TX: phase=EVENT_AWAIT_CREATED_ENTITY effect=%d group=%d seq=%llu",value->effect,group,(unsigned long long)value->sequence);
                        return;
                    }
                    Pick3EffectIdentitySnapshot identity;
                    identity.sourceId=context.source;identity.targetId=target;
                    identity.springboardId=context.springboard;identity.effectEntityId=value->effect;
                    identity.effectGroupId=group;identity.valid=true;identity.selectionSequence=value->sequence;
                    FireInModel("PickFromThreeTrigger",effect,model,&identity);
                };
        }
        return installed;
    }

} // namespace CustomComponents::Systems::CustomTriggerSystem
