#include "core/HookRegistry.hpp"
#include "engine/EffectExecution.hpp"
#pragma once
#include "integration/ReplayEventBridge.hpp"
#include "core/SceneViewContext.hpp"
#include <cstdio>
#include "engine/TriggerDispatch.hpp"
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
#include "core/WeakObjectSettings.hpp"
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
#include "variables/QueuedTriggerValues.hpp"

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
    inline Core::WeakObjectSettings<std::shared_ptr<Core::ManagedRuntime::Root>> queueOwners;

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

    inline BNM::Field<MonoList<Obj*>*> modelEntities;
    inline BNM::Field<MonoList<Obj*>*> entityComponents;
    inline BNM::Field<int> entityId;

    inline BNM::Field<BNM::Structures::Mono::String*> customTriggerName;

    inline BNM::Field<int> effectEffectEntityId,effectSpringboardId,effectGroupId;
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
    inline BNM::Field<Obj*> queueEffectsField,queueTriggeredEffectsField,queueInstancesField,queueModelField;
    inline BNM::Field<Obj*> producerInstancesField, producerTriggeredEffectsField, producerFinderField;
    inline BNM::Field<Obj*> finderModelField, finderValidatorField;
    inline BNM::Field<Obj*> stateCheckAdderField, stateCheckAdderInstancesField;
    inline void LogPick3ServiceProcess(const char* phase, Obj* service) {
        CC_SELECTION_DIAG("CustomComponents/Pick3/SERVICE_PROCESS: phase=%s service=%p", phase, service);
    }
    using RegisterHandlersFn = void (*)(void*, Obj*, const void*);
    using ProcessEffectFn = void (*)(void*, Obj*, const void*);
    inline RegisterHandlersFn originalQueueRegisterHandlers = nullptr;
    inline ProcessEffectFn originalQueueProcessEffect = nullptr;
    inline std::array<Obj*, 48> gateQueueSystems{};
    inline std::size_t gateQueueSystemCount = 0;
    inline Obj* gateObservedTriggers = nullptr;
    inline int gateObservedRemaining = 0;

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
            Obj* triggered=queueTriggeredEffectsField.IsValid()?Core::Metadata::Bind(queueTriggeredEffectsField,candidate).Get():nullptr;
            if (triggered != expectedTriggers) continue;
            // Queue system and producer share the exact injected collection.
            matchedSystem = candidate;
            effects = queueEffectsField.IsValid()?Core::Metadata::Bind(queueEffectsField,candidate).Get():nullptr;
            break;
        }
        if (!matchedSystem || !effects) {
            CC_SELECTION_DIAG("CustomComponents/Pick3/PICK3_QUEUE_GATE: phase=%s matched=0 triggered=%p candidates=%zu",
                     phase, (void*)expectedTriggers, gateQueueSystemCount);
            return;
        }
        const bool pending = Core::Metadata::Bind(gateHasTriggers,expectedTriggers)();
        const bool hasNext = Core::Metadata::Bind(gateHasNext,effects)();
        const bool groupReady = Core::Metadata::Bind(gateShiftWillChangeGroup,effects)();
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
        // This stock system is injected with the authoritative EntityModel.
        // Register every router before any ordinary effect processing, rather
        // than only when a Withering discard eventually occurs.
        Obj* model=system&&queueModelField.IsValid()?Core::Metadata::Bind(queueModelField,system).Get():nullptr;
        Obj* collection=system&&queueTriggeredEffectsField.IsValid()
            ?Core::Metadata::Bind(queueTriggeredEffectsField,system).Get():nullptr;
        if(collection)queueOwners.Set(collection,std::make_shared<Core::ManagedRuntime::Root>(system,true));
        CustomComponents::Pick3::TransactionRuntime::RegisterRouter(router,model);
        if (router && model) {
            CustomComponents::Engine::RouterContext::RegisterRouter(router,model);
            static std::atomic<unsigned> samples{0};
            if (samples.fetch_add(1)<24) LOG_INFO("CustomComponents/RouterModel/BOUND: router=%p model=%p",router,model);
        }
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
        // Fallback for builds where RegisterEffectHandlers runs before all
        // injected fields become available. Bind from this exact stock queue
        // system's injected model + instance manager; no cross-match guessing.
        Obj* queueSystem=static_cast<Obj*>(self);
        Obj* model=queueSystem&&queueModelField.IsValid()?Core::Metadata::Bind(queueModelField,queueSystem).Get():nullptr;
        Obj* manager=queueSystem&&queueInstancesField.IsValid()?Core::Metadata::Bind(queueInstancesField,queueSystem).Get():nullptr;
        if(model && manager){
            Obj* router=Engine::Match::Provide(manager,BNM::Class("PvZCards.Engine","EffectRouter").GetMonoType());
            if(router && !Engine::RouterContext::RouterModel(router)){
                Engine::RouterContext::RegisterRouter(router,model);
                static std::atomic<unsigned> samples{0};if(samples.fetch_add(1)<16)
                    LOG_INFO("CustomComponents/RouterModel/LATE_BOUND: router=%p model=%p queue=%p",router,model,self);
            }
        }
        Obj* triggers=queueSystem&&queueTriggeredEffectsField.IsValid()?Core::Metadata::Bind(queueTriggeredEffectsField,queueSystem).Get():nullptr;
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
            const int sourceId=effectSourceId.IsValid()?Core::Metadata::Bind(effectSourceId,effect).Get():-1;
            const int targetId=effectTargetId.IsValid()?Core::Metadata::Bind(effectTargetId,effect).Get():-1;
            const int springboardId=effectSpringboardId.IsValid()?Core::Metadata::Bind(effectSpringboardId,effect).Get():-1;
            const int effectId=effectEffectEntityId.IsValid()?Core::Metadata::Bind(effectEffectEntityId,effect).Get():-1;
            const int groupId=effectGroupId.IsValid()?Core::Metadata::Bind(effectGroupId,effect).Get():-1;
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
                     self, queueEffectsField.IsValid()?Core::Metadata::Bind(queueEffectsField,queueSystem).Get():nullptr, effect, triggers, gateObservedRemaining);
            LogPick3QueueGate("STOCK_PROCESS_ENTER", triggers);
        }
        // Patch 2.2: use the already-installed stock execution detour.
        // The scoped observer only compares immutable effect/group IDs with
        // snapshot history; it never changes return values or queue state.
        const int executionEffect=effect&&effectEffectEntityId.IsValid()?Core::Metadata::Bind(effectEffectEntityId,effect).Get():-1;
        const int executionGroup=effect&&effectGroupId.IsValid()?Core::Metadata::Bind(effectGroupId,effect).Get():-1;
        const int executionSource=effect&&effectSourceId.IsValid()?Core::Metadata::Bind(effectSourceId,effect).Get():-1;
        const int executionTarget=effect&&effectTargetId.IsValid()?Core::Metadata::Bind(effectTargetId,effect).Get():-1;
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
            if (gateObservedRemaining == 0 || !Core::Metadata::Bind(gateHasTriggers,triggers)()) {
                gateObservedTriggers = nullptr;
                gateObservedRemaining = 0;
            }
        }
    }


    using DescriptorToEffectFn = Obj* (*)(void*, Obj*, Obj*, const void*);
    inline RegisterHandlersFn originalTriggerSystemRegisterHandlers = nullptr;
    inline ProcessEffectFn originalStateCheckProcessEffect = nullptr;
    inline DescriptorToEffectFn originalStockTriggerCarrierToEffect = nullptr;

    // The StateCheckSystem currently processing an effect is our strongest
    // live-simulation scope anchor. Keep it per-thread because prediction work
    // also runs StateCheck processing on worker threads.
    inline thread_local Obj* activeStateCheckInstances = nullptr;

    #include "triggers/detail/TriggerProducerContext.inl"
    #include "triggers/detail/CustomTriggerDispatch.inl"
    #include "triggers/detail/CustomTriggerCarrier.inl"
    inline Obj* ModelForTriggeredCollection(Obj* collection) {
        if(!collection)return nullptr;
        if(auto owner=queueOwners.Get(collection)){
            Obj* system=owner->Get();
            if(system && queueModelField.IsValid())return Core::Metadata::Bind(queueModelField,system).Get();
        }
        for(auto& producer:SnapshotStockTriggerContexts())
            if(producer.triggeredEffects==collection)return producer.model;
        return nullptr;
    }
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
        auto producerSystem = BNM::Class("PvZCards.Core.Systems", "TriggerSystem`1");
        auto genericFinder = BNM::Class("PvZCards.Core.Systems.Utilities", "GenericEffectEntityFinder`1");
        auto stateCheckAdder = BNM::Class("PvZCards.Core.Utilities", "StateCheckEffectAdder");

        if (!queueSystem.IsValid() || !stateCheckSystem.IsValid() ||
            !stockTriggerCarrierDescriptor.IsValid() ||
            !customTrigger.IsValid() || !fireTriggerDescriptor.IsValid() ||
            !fireCustomTriggerDescriptor.IsValid() ||
            !entity.IsValid() || !model.IsValid() ||
            !effect.IsValid() || !triggerData.IsValid() || !effectContext.IsValid() ||
            !effectData.IsValid() || !entityQueryContext.IsValid() ||
            !triggerValidator.IsValid() ||
            !effectEntity.IsValid() || !triggeredCollection.IsValid() ||
            !instanceManager.IsValid() || !producerSystem.IsValid() ||
            !genericFinder.IsValid() || !stateCheckAdder.IsValid()) {
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
        effectSpringboardId=effect.GetField("SpringboardId").cast<int>();
        effectGroupId=effect.GetField("EffectGroupId").cast<int>();

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
        for(auto candidate:effectContext.GetMethods())
            if(auto* info=candidate.GetInfo();info&&std::string_view(info->name)=="CopyFrom"&&info->parameters_count==1&&BNM::Class(info->parameters[0]).GetClass()==entityQueryContext.GetClass()){
                effectContextCopyFromQuery=candidate.cast<void>();
                break;
            }
        effectEntityGetSource = effectEntity.GetMethod("GetSource", 1).cast<int>();
        triggeredEffectsAdd = triggeredCollection.GetMethod("Add", 1).cast<void>();
        gateHasTriggers = triggeredCollection.GetMethod("HasTriggers", 0).cast<bool>();
        auto concreteQueue = BNM::Class("PvZCards.Engine", "EffectQueue");
        if (concreteQueue.IsValid()) {
            gateHasNext = concreteQueue.GetMethod("HasNext", 0).cast<bool>();
            gateShiftWillChangeGroup = concreteQueue.GetMethod("ShiftWillChangeGroup", 0).cast<bool>();
        }


        auto queueRegisterHandlers = queueSystem.GetMethod("RegisterEffectHandlers", 1);
        queueEffectsField=queueSystem.GetField("<Effects>k__BackingField").cast<Obj*>();
        queueTriggeredEffectsField=queueSystem.GetField("<TriggeredEffects>k__BackingField").cast<Obj*>();
        queueInstancesField=queueSystem.GetField("<InstanceManager>k__BackingField").cast<Obj*>();
        queueModelField=queueSystem.GetField("<Model>k__BackingField").cast<Obj*>();
        producerInstancesField=producerSystem.GetField("<Instances>k__BackingField").cast<Obj*>();
        producerTriggeredEffectsField=producerSystem.GetField("<TriggeredEffects>k__BackingField").cast<Obj*>();
        producerFinderField=producerSystem.GetField("<Finder>k__BackingField").cast<Obj*>();
        finderModelField=genericFinder.GetField("<Model>k__BackingField").cast<Obj*>();
        finderValidatorField=genericFinder.GetField("<Validator>k__BackingField").cast<Obj*>();
        auto objectType=BNM::Class("System","Object").GetCompileTimeClass();
        auto producerObject=producerSystem.GetGeneric({objectType});
        auto finderObject=genericFinder.GetGeneric({objectType});
        // Open generic fields have no concrete instance layout. Resolve the
        // shared reference-type layout before reading any injected service.
        producerInstancesField=producerObject.GetField("<Instances>k__BackingField").cast<Obj*>();
        producerTriggeredEffectsField=producerObject.GetField("<TriggeredEffects>k__BackingField").cast<Obj*>();
        producerFinderField=producerObject.GetField("<Finder>k__BackingField").cast<Obj*>();
        finderModelField=finderObject.GetField("<Model>k__BackingField").cast<Obj*>();
        finderValidatorField=finderObject.GetField("<Validator>k__BackingField").cast<Obj*>();
        if(!producerInstancesField.IsValid() || !producerTriggeredEffectsField.IsValid() ||
           !producerFinderField.IsValid() || !finderModelField.IsValid() || !finderValidatorField.IsValid() ||
           producerInstancesField.GetOffset()<sizeof(Obj) || finderValidatorField.GetOffset()<sizeof(Obj)){
            LOG_ERR("CustomComponents/CustomTrigger: concrete generic layout unavailable");return false;
        }
        LOG_INFO("CustomComponents/CustomTrigger: concrete layout instances=%zu collection=%zu finder=%zu model=%zu validator=%zu",
            size_t(producerInstancesField.GetOffset()),size_t(producerTriggeredEffectsField.GetOffset()),
            size_t(producerFinderField.GetOffset()),size_t(finderModelField.GetOffset()),size_t(finderValidatorField.GetOffset()));
        stateCheckAdderField=stateCheckSystem.GetField("<EffectAdder>k__BackingField").cast<Obj*>();
        stateCheckAdderInstancesField=stateCheckAdder.GetField("<Instances>k__BackingField").cast<Obj*>();
        auto stateCheckProcessEffect =
            stateCheckSystem.GetMethod("ProcessEffect", 1);
        auto stockTriggerCarrierToEffect =
            stockTriggerCarrierDescriptor.GetMethod("ToEffect", 2);

        // Resolve the shared reference-type generic implementation by metadata.
        // This replaces the old build-specific RVA arithmetic, which silently
        // became stale when Sprig/game versions changed.
        auto triggerSystemObject = producerSystem.GetGeneric({
            BNM::Class("System", "Object").GetCompileTimeClass()
        });
        auto triggerSystemRegisterHandlers =
            triggerSystemObject.GetMethod("RegisterEffectHandlers", 1);

        const bool valid =
            modelEntities.IsValid() &&
            entityComponents.IsValid() &&
            entityId.IsValid() &&
            customTriggerName.IsValid() &&
            effectSourceId.IsValid() &&
            effectTargetId.IsValid() &&
            effectEffectEntityId.IsValid() &&
            effectSpringboardId.IsValid() && effectGroupId.IsValid() &&
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
            queueEffectsField.IsValid()&&queueTriggeredEffectsField.IsValid()&&queueInstancesField.IsValid()&&queueModelField.IsValid()&&
            producerInstancesField.IsValid() && producerTriggeredEffectsField.IsValid() &&
            producerFinderField.IsValid() && finderModelField.IsValid() &&


            finderValidatorField.IsValid() && stateCheckAdderField.IsValid() &&
            stateCheckAdderInstancesField.IsValid() &&
            stateCheckProcessEffect.IsValid() &&
            stockTriggerCarrierToEffect.IsValid() &&
            triggerSystemObject.IsValid() &&
            triggerSystemRegisterHandlers.IsValid();

        if (!valid) {
            LOG_ERR("CustomComponents/CustomTrigger: metadata method/field lookup failed");
            return false;
        }

        const bool triggerSystemHookInstalled = CustomComponents::Core::Hooks::Install(
            triggerSystemRegisterHandlers,
            TriggerSystemRegisterEffectHandlersHook,
            originalTriggerSystemRegisterHandlers
        );
        if (!triggerSystemHookInstalled || !originalTriggerSystemRegisterHandlers) {
            LOG_ERR(
                "CustomComponents/CustomTrigger: TriggerSystem<object>.RegisterEffectHandlers hook installation failed"
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
            "CustomComponents/CustomTrigger: hooked TriggerSystem<object>.RegisterEffectHandlers (lazy producer refresh)"
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
        Variables::QueuedTriggerValues::queueModelLookup=ModelForTriggeredCollection;
        const bool installed=RegisterTypes() && InstallHooks();
        if(installed) {
            Engine::TriggerQueue::AddFilter(+[](const Engine::TriggerQueue::Context& queued) {
                Obj* model=ModelForTriggeredCollection(queued.collection);
                Obj* context=Core::Metadata::Bind(triggerDataContext,queued.data).Get();
                const auto event=CustomComponents::Pick3::TransactionRuntime::ReadContext(context);
                return ReturnToLaneMoveFilter::ShouldSuppress(model,
                    Core::Metadata::Bind(triggerDataEffectEntityId,queued.data).Get(),event.effect,FindEntityById,HasNamedComponent);
            });
            Engine::TriggerQueue::recycle=+[](const Engine::TriggerQueue::Context& queued) {
                if(auto owner=queueOwners.Get(queued.collection)){
                    Obj* system=owner->Get();
                    Obj* instances=system && queueInstancesField.IsValid()?Core::Metadata::Bind(queueInstancesField,system).Get():nullptr;
                    if(instances){Core::Metadata::Bind(returnToPool,instances)(queued.data);return true;}
                }
                for(auto& producer:SnapshotStockTriggerContexts()) if(producer.triggeredEffects==queued.collection) {
                    Core::Metadata::Bind(returnToPool,producer.instances)(queued.data);return true;
                }
                return false;
            };

        }
        return installed;
    }

    inline bool InitializePick3AfterVariables(){
        const bool transactionReady=CustomComponents::Pick3::TransactionRuntime::Initialize();
#if CUSTOMCOMPONENTS_PF3_TRANSACTION_PIPELINE
        CustomComponents::Pick3::TransactionRuntime::pipelineReady=transactionReady && CustomComponents::Pick3::Acceptance::Initialize();
#else
        CustomComponents::Pick3::TransactionRuntime::pipelineReady=false;
#endif
        if(!transactionReady)LOG_ERR("CustomComponents/Pick3/PF3_TX: phase=INIT_FAILED_AFTER_VARIABLES baselinePreserved=1");
        if(CustomComponents::Pick3::TransactionRuntime::pipelineReady) CustomComponents::Pick3::TransactionRuntime::onStockAccepted =
            +[](Obj* model,Obj* effect,CustomComponents::Pick3::Transaction::Ref value,
                CustomComponents::Pick3::Transaction::Context context,int group) {
                if(!value || value->cards.empty())return;
                if(!CustomComponents::Pick3::TransactionRuntime::AllTargetsBound(value)) {
                    CustomComponents::Pick3::TransactionRuntime::AwaitCreation(value,context,group);
                    CC_SELECTION_DIAG("CustomComponents/Pick3/PF3_TX: phase=EVENT_AWAIT_CREATED_ENTITY effect=%d group=%d seq=%llu",value->effect,group,(unsigned long long)value->sequence);
                    return;
                }
                Pick3EffectIdentitySnapshot identity;
                // A multi-pick keeps the stock event target. Its full selected set is in the transaction.
                identity.sourceId=context.source;
                identity.targetId=value->cards.size()==1
                    ? CustomComponents::Pick3::Transaction::LiveTarget(value,0) : context.target;
                identity.springboardId=context.springboard;identity.effectEntityId=value->effect;
                identity.effectGroupId=group;identity.valid=true;identity.selectionSequence=value->sequence;
                const bool fired=FireInModel("PickFromThreeTrigger",effect,model,&identity);
                LOG_INFO("CustomComponents/Pick3/PF3_TX: phase=TRIGGER_FIRE effect=%d group=%d sequence=%llu selected=%zu target=%d result=%d",
                    value->effect,group,(unsigned long long)value->sequence,value->cards.size(),identity.targetId,int(fired));
            };
        LOG_INFO("CustomComponents/Pick3/PF3_TX: phase=POST_VARIABLE_INIT ready=%d createHook=%d",
            int(CustomComponents::Pick3::TransactionRuntime::pipelineReady),int(CustomComponents::Variables::QueuedTriggerValues::originalCreateEffect!=nullptr));
        return CustomComponents::Pick3::TransactionRuntime::pipelineReady;
    }

} // namespace CustomComponents::Systems::CustomTriggerSystem
