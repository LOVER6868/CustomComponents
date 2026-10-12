#pragma once

#include <array>
#include <cstring>
#include <cstdint>
#include <string>

#include <BNM/BasicMonoStructures.hpp>
#include <BNM/Class.hpp>
#include <BNM/Field.hpp>
#include <BNM/Method.hpp>
#include <dobby.h>

#include "components/ReturnedFromDiscardPileTrigger.hpp"
#include "systems/DiscardedSystem.hpp"
#include "components/DiscardOutsidePlayTrigger.hpp"
#include "components/CustomTrigger.hpp"
#include "components/FireTriggerEffectDescriptor.hpp"
#include "components/FireCustomTriggerEffectDescriptor.hpp"
#include "core/CustomComponentRegistry.hpp"
#include "systems/CustomFieldBridgeSystem.hpp"
#include "systems/ZoneTriggeredAbilityPresentationSystem.hpp"
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

    using RegisterHandlersFn = void (*)(void*, Obj*, const void*);
    using ProcessEffectFn = void (*)(void*, Obj*, const void*);
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

    inline const char* TypeName(Obj* object) {
        if (!object)
            return "null";
        auto* klass = BNM::Class(object).GetClass();
        return klass && klass->name ? klass->name : "?";
    }

    inline Obj* FindNamedComponent(Obj* entity, const char* componentName) {
        if (!entity || !componentName)
            return nullptr;

        auto* components = entityComponents[entity].Get();
        if (!components || !components->items)
            return nullptr;

        for (int i = 0; i < components->size; ++i) {
            Obj* component = components->items->m_Items[i];
            if (component && std::strcmp(TypeName(component), componentName) == 0)
                return component;
        }
        return nullptr;
    }

    inline bool HasNamedComponent(Obj* entity, const char* componentName) {
        return FindNamedComponent(entity, componentName) != nullptr;
    }

    inline bool CustomTriggerMatches(Obj* entity, const char* eventName) {
        if (!entity || !eventName || !*eventName || !customTriggerName.IsValid())
            return false;

        Obj* component = FindNamedComponent(entity, "CustomTrigger");
        if (!component)
            return false;

        auto* managedName = customTriggerName[component].Get();
        return managedName && managedName->str() == eventName;
    }

    inline bool EntityListensForEvent(
        Obj* entity,
        const char* markerComponentName,
        const char* eventName
    ) {
        const bool markerMatch =
            markerComponentName &&
            *markerComponentName &&
            HasNamedComponent(entity, markerComponentName);

        return markerMatch || CustomTriggerMatches(entity, eventName);
    }

    inline Obj* ReadObjectFieldByOffset(Obj* object, std::uintptr_t offset) {
        if (!object)
            return nullptr;
        return *reinterpret_cast<Obj**>(
            reinterpret_cast<std::uintptr_t>(object) + offset
        );
    }

    inline bool RefreshStockTriggerContext(StockTriggerContext& context) {
        if (!context.triggerSystem)
            return false;

        Obj* instances = ReadObjectFieldByOffset(
            context.triggerSystem,
            kTriggerSystemInstancesOffset
        );
        Obj* triggeredEffects = ReadObjectFieldByOffset(
            context.triggerSystem,
            kTriggerSystemTriggeredEffectsOffset
        );
        Obj* finder = ReadObjectFieldByOffset(
            context.triggerSystem,
            kTriggerSystemFinderOffset
        );

        if (!instances || !triggeredEffects || !finder)
            return false;

        if (std::strcmp(TypeName(finder), "GenericEffectEntityFinder`1") != 0)
            return false;

        Obj* model = ReadObjectFieldByOffset(
            finder,
            kGenericFinderModelOffset
        );
        if (!model)
            return false;

        context.instances = instances;
        context.triggeredEffects = triggeredEffects;
        context.finder = finder;
        context.model = model;
        return true;
    }

    inline void RememberStockTriggerCandidate(Obj* triggerSystem) {
        if (!triggerSystem)
            return;

        // The DI container assigns TriggerSystem<T>'s injected fields through
        // generated setters. RegisterEffectHandlers can run before Finder is
        // assigned in this build, so keep the TriggerSystem object itself and
        // resolve its injected fields lazily when an event is fired.
        for (std::size_t i = 0; i < stockTriggerContextCount; ++i) {
            if (stockTriggerContexts[i].triggerSystem != triggerSystem)
                continue;

            StockTriggerContext refreshed = stockTriggerContexts[i];
            RefreshStockTriggerContext(refreshed);

            for (std::size_t j = i + 1; j < stockTriggerContextCount; ++j)
                stockTriggerContexts[j - 1] = stockTriggerContexts[j];

            stockTriggerContexts[stockTriggerContextCount - 1] = refreshed;
            return;
        }

        StockTriggerContext candidate{};
        candidate.triggerSystem = triggerSystem;
        RefreshStockTriggerContext(candidate);

        if (stockTriggerContextCount < stockTriggerContexts.size()) {
            stockTriggerContexts[stockTriggerContextCount++] = candidate;
        } else {
            // Only evict once the large recency window is genuinely full.
            // This preserves all producer systems from the current match and
            // its prediction models under normal play.
            for (std::size_t i = 1; i < stockTriggerContexts.size(); ++i)
                stockTriggerContexts[i - 1] = stockTriggerContexts[i];
            stockTriggerContexts[stockTriggerContexts.size() - 1] = candidate;
        }

        LOG_INFO(
            "CustomComponents/CustomTrigger: captured TriggerSystem candidate system=%p instances=%p triggered=%p finder=%p model=%p count=%zu",
            candidate.triggerSystem,
            candidate.instances,
            candidate.triggeredEffects,
            candidate.finder,
            candidate.model,
            stockTriggerContextCount
        );
    }

    static void TriggerSystemRegisterEffectHandlersHook(
        void* self,
        Obj* router,
        const void* method
    ) {
        // Capture the TriggerSystem object itself, but do not require its
        // injected fields to be populated yet. In this build Finder can still
        // be null when RegisterEffectHandlers runs. RefreshStockTriggerContext
        // resolves Instances/TriggeredEffects/Finder/Model lazily at fire time.
        RememberStockTriggerCandidate(static_cast<Obj*>(self));
        originalTriggerSystemRegisterHandlers(self, router, method);
    }

    inline bool ModelContainsEntityId(Obj* model, int id);

    inline Obj* GetStateCheckInstances(void* stateCheckSystem) {
        if (!stateCheckSystem)
            return nullptr;

        Obj* effectAdder = ReadObjectFieldByOffset(
            static_cast<Obj*>(stateCheckSystem),
            kStateCheckEffectAdderOffset
        );
        if (!effectAdder)
            return nullptr;

        return ReadObjectFieldByOffset(
            effectAdder,
            kStateCheckAdderInstancesOffset
        );
    }

    inline StockTriggerContext* FindStockTriggerContextForEffect(
        Obj* effect,
        Obj* preferredInstances = nullptr
    ) {
        if (!effect)
            return nullptr;

        const int effectEntityId =
            effectEffectEntityId.IsValid()
                ? effectEffectEntityId[effect].Get()
                : -1;
        const int targetId = effectTargetId[effect].Get();
        const int sourceId = effectSourceId[effect].Get();

        // First choice: exact DI/simulation scope identity.
        //
        // Entity ids are reused heavily by prediction/snapshot models, so
        // "model contains id N" is not sufficient to prove a context is live.
        // If StateCheckSystem gave us its live InstanceManager, only accept a
        // TriggerSystem<T> injected with that exact same InstanceManager.
        if (preferredInstances) {
            for (std::size_t i = stockTriggerContextCount; i-- > 0;) {
                StockTriggerContext& context = stockTriggerContexts[i];
                if (!RefreshStockTriggerContext(context))
                    continue;

                if (context.instances != preferredInstances)
                    continue;

                LOG_INFO(
                    "CustomComponents/CustomTrigger: producer select ACTIVE-SCOPE system=%p model=%p instances=%p triggered=%p finder=%p",
                    context.triggerSystem,
                    context.model,
                    context.instances,
                    context.triggeredEffects,
                    context.finder
                );
                return &context;
            }

            // Do not fall back to recycled entity-id matching when a concrete
            // live StateCheck scope is known. A stale model with the same ids
            // is exactly what caused the previous engine lock.
            LOG_ERR(
                "CustomComponents/CustomTrigger: no TriggerSystem producer matched active StateCheck InstanceManager=%p contexts=%zu",
                preferredInstances,
                stockTriggerContextCount
            );
            return nullptr;
        }

        auto matches = [&](StockTriggerContext& context, int id) -> bool {
            if (id < 0)
                return false;

            if (!RefreshStockTriggerContext(context))
                return false;

            return context.model && ModelContainsEntityId(context.model, id);
        };

        // The descriptor EffectEntityId is the strongest discriminator because
        // it identifies the actual ability/effect entity that produced this
        // runtime effect. Prefer contexts containing it, then target, then source.
        if (effectEntityId >= 0) {
            for (std::size_t i = stockTriggerContextCount; i-- > 0;) {
                if (matches(stockTriggerContexts[i], effectEntityId)) {
                    LOG_INFO(
                        "CustomComponents/CustomTrigger: producer select EFFECT-ENTITY system=%p model=%p finder=%p EffectEntityId=%d",
                        stockTriggerContexts[i].triggerSystem,
                        stockTriggerContexts[i].model,
                        stockTriggerContexts[i].finder,
                        effectEntityId
                    );
                    return &stockTriggerContexts[i];
                }
            }
        }

        if (targetId >= 0) {
            for (std::size_t i = stockTriggerContextCount; i-- > 0;) {
                if (matches(stockTriggerContexts[i], targetId)) {
                    LOG_INFO(
                        "CustomComponents/CustomTrigger: producer select TARGET system=%p model=%p finder=%p TargetId=%d",
                        stockTriggerContexts[i].triggerSystem,
                        stockTriggerContexts[i].model,
                        stockTriggerContexts[i].finder,
                        targetId
                    );
                    return &stockTriggerContexts[i];
                }
            }
        }

        if (sourceId >= 0) {
            for (std::size_t i = stockTriggerContextCount; i-- > 0;) {
                if (matches(stockTriggerContexts[i], sourceId)) {
                    LOG_INFO(
                        "CustomComponents/CustomTrigger: producer select SOURCE system=%p model=%p finder=%p SourceId=%d",
                        stockTriggerContexts[i].triggerSystem,
                        stockTriggerContexts[i].model,
                        stockTriggerContexts[i].finder,
                        sourceId
                    );
                    return &stockTriggerContexts[i];
                }
            }
        }

        LOG_ERR(
            "CustomComponents/CustomTrigger: no TriggerSystem producer context matched EffectEntityId=%d TargetId=%d SourceId=%d contexts=%zu",
            effectEntityId,
            targetId,
            sourceId,
            stockTriggerContextCount
        );
        return nullptr;
    }

    inline bool ModelContainsEntityId(Obj* model, int id) {
        if (!model || id < 0)
            return false;

        auto* entities = modelEntities[model].Get();
        if (!entities || !entities->items)
            return false;

        for (int i = 0; i < entities->size; ++i) {
            Obj* entity = entities->items->m_Items[i];
            if (entity && entityId[entity].Get() == id)
                return true;
        }
        return false;
    }

    inline Obj* FindEntityById(Obj* model, int id) {
        if (!model || id < 0)
            return nullptr;

        auto* entities = modelEntities[model].Get();
        if (!entities || !entities->items)
            return nullptr;

        for (int i = 0; i < entities->size; ++i) {
            Obj* entity = entities->items->m_Items[i];
            if (entity && entityId[entity].Get() == id)
                return entity;
        }
        return nullptr;
    }

    // Generic named-marker dispatcher.  The public trigger remains a real
    // managed component; this routine only replaces the closed TriggerSystem<T>
    // lookup step. Once TriggerData is added, EA's normal
    // TriggeredEffectQueuingSystem owns the rest of execution.
    inline Obj* BuildEntityQueryContextStockEquivalent(
        Obj* effect,
        Obj* instances
    ) {
        if (!effect || !instances || !entityQueryContextType)
            return nullptr;

        // Exact behavior of Effect::ToEntityQueryContext at RVA 0x227F2F4:
        //   1) InstanceManager.Acquire(typeof(EntityQueryContext), true)
        //   2) copy SourceId / TargetId / EffectEntityId / SpringboardId
        //      into queryContext->EffectContext
        //   3) queryContext->EffectContext.EffectData.CopyFrom(effect->Data)
        //
        // We intentionally reproduce that native body here instead of calling
        // Effect.ToEntityQueryContext(), which is the call currently hanging
        // when invoked from the custom StateCheck carrier path.
        Obj* queryContext = acquire[instances](entityQueryContextType, true);
        if (!queryContext)
            return nullptr;

        auto* effectBytes = reinterpret_cast<std::uint8_t*>(effect);
        auto* queryBytes = reinterpret_cast<std::uint8_t*>(queryContext);

        Obj* effectContext =
            *reinterpret_cast<Obj**>(queryBytes + 0x18);
        if (!effectContext) {
            returnToPool[instances](queryContext);
            return nullptr;
        }

        auto* contextBytes = reinterpret_cast<std::uint8_t*>(effectContext);

        const int sourceId =
            *reinterpret_cast<int*>(effectBytes + 0x24);
        const int targetId =
            *reinterpret_cast<int*>(effectBytes + 0x28);
        const int springboardId =
            *reinterpret_cast<int*>(effectBytes + 0x2C);
        const int effectEntityId =
            *reinterpret_cast<int*>(effectBytes + 0x30);

        *reinterpret_cast<int*>(contextBytes + 0x14) = sourceId;
        *reinterpret_cast<int*>(contextBytes + 0x10) = targetId;
        *reinterpret_cast<int*>(contextBytes + 0x18) = effectEntityId;
        *reinterpret_cast<int*>(contextBytes + 0x1C) = springboardId;

        Obj* destinationData =
            *reinterpret_cast<Obj**>(contextBytes + 0x20);
        Obj* sourceData =
            *reinterpret_cast<Obj**>(effectBytes + 0x10);

        if (!destinationData || !sourceData) {
            returnToPool[instances](queryContext);
            return nullptr;
        }

        effectDataCopyFrom[destinationData](sourceData);
        return queryContext;
    }

    inline bool ValidateWithStockTriggerValidator(
        Obj* stockFinder,
        Obj* model,
        Obj* effectEntity,
        Obj* queryContext
    ) {
        if (!stockFinder || !model || !effectEntity || !queryContext ||
            !triggerValidatorShouldTrigger.IsValid()) {
            return false;
        }

        // GenericEffectEntityFinder<T>::WillTrigger (RVA 0x282BA3C):
        //   EffectEntity.GetSource(effectEntity)
        //   save context.SelfId
        //   context.SelfId = sourceId
        //   require source entity to exist in Model
        //   TriggerValidator.ShouldTriggerEffect(effectEntity, context)
        //   restore SelfId
        Obj* validator = ReadObjectFieldByOffset(
            stockFinder,
            kGenericFinderValidatorOffset
        );
        if (!validator)
            return false;

        const int listenerSourceId =
            effectEntityGetSource[static_cast<Obj*>(nullptr)](effectEntity);

        auto* contextBytes =
            reinterpret_cast<std::uint8_t*>(queryContext);
        const int previousSelfId =
            *reinterpret_cast<int*>(contextBytes + 0x20);

        *reinterpret_cast<int*>(contextBytes + 0x20) =
            listenerSourceId;

        bool result = false;
        if (listenerSourceId >= 0 &&
            ModelContainsEntityId(model, listenerSourceId)) {
            result = triggerValidatorShouldTrigger[validator](
                effectEntity,
                queryContext
            );
        }

        *reinterpret_cast<int*>(contextBytes + 0x20) =
            previousSelfId;

        return result;
    }

    inline bool FireEvent(
        const char* markerComponentName,
        const char* eventName,
        Obj* firingEffect
    ) {
        if (!eventName || !*eventName || !firingEffect)
            return false;

        StockTriggerContext* stockContext =
            FindStockTriggerContextForEffect(
                firingEffect,
                activeStateCheckInstances
            );
        if (!stockContext) {
            LOG_ERR(
                "CustomComponents/CustomTrigger: FIRE '%s' failed: no coherent stock TriggerSystem context",
                eventName
            );
            return false;
        }

        Obj* model = stockContext->model;
        Obj* instances = stockContext->instances;
        Obj* triggeredEffects = stockContext->triggeredEffects;
        Obj* stockFinder = stockContext->finder;

        if (!model || !instances || !triggeredEffects || !stockFinder) {
            LOG_ERR(
                "CustomComponents/CustomTrigger: FIRE '%s' incomplete TriggerSystem context model=%p instances=%p triggered=%p finder=%p",
                eventName,
                model,
                instances,
                triggeredEffects,
                stockFinder
            );
            return false;
        }

        LOG_INFO(
            "CustomComponents/CustomTrigger: FIRE before ToEntityQueryContext instances=%p",
            instances
        );

        Obj* queryContext = BuildEntityQueryContextStockEquivalent(firingEffect, instances);

        LOG_INFO(
            "CustomComponents/CustomTrigger: FIRE queryContext=%p",
            queryContext
        );

        if (!queryContext) {
            LOG_ERR(
                "CustomComponents/CustomTrigger: FIRE '%s' could not build EntityQueryContext",
                eventName
            );
            return false;
        }

        auto triggerDataClass = BNM::Class("PvZCards.Engine", "TriggerData");
        if (!triggerDataClass.IsValid()) {
            returnToPool[instances](queryContext);
            return false;
        }

        const bool returnedEvent = std::strcmp(eventName, "ReturnedFromDiscardPileTrigger") == 0;
        const int returnedCardId = effectTargetId[firingEffect].Get();
        if (returnedEvent)
            DiscardedSystem::BeginReturn(model, returnedCardId, triggeredEffects);

        int candidates = 0;
        int rejected = 0;
        int queued = 0;
        auto* entities = modelEntities[model].Get();
        if (entities && entities->items) {
            for (int i = 0; i < entities->size; ++i) {
                Obj* effectEntity = entities->items->m_Items[i];
                if (!effectEntity ||
                    !EntityListensForEvent(
                        effectEntity,
                        markerComponentName,
                        eventName
                    )) {
                    continue;
                }

                ++candidates;

                // This is the stock GenericEffectEntityFinder<T>::WillTrigger
                // path.  It sets/restores EntityQueryContext.SelfId and runs
                // TriggerValidator over every Condition component attached to
                // the effect entity.  Do not special-case TriggerTargetFilter,
                // SelfEntityFilter, lane filters, once-per-turn conditions, etc.
                if (!ValidateWithStockTriggerValidator(
                        stockFinder,
                        model,
                        effectEntity,
                        queryContext
                    )) {
                    ++rejected;
                    LOG_INFO(
                        "CustomComponents/CustomTrigger: REJECT '%s' EffectEntityId=%d SourceId=%d",
                        eventName,
                        entityId[effectEntity].Get(),
                        effectEntityGetSource[static_cast<Obj*>(nullptr)](effectEntity)
                    );
                    continue;
                }

                Obj* triggerData = acquire[instances](
                    reinterpret_cast<Obj*>(triggerDataClass.GetMonoType()),
                    false
                );
                if (!triggerData)
                    continue;

                Obj* context = triggerDataContext[triggerData].Get();
                if (!context) {
                    returnToPool[instances](triggerData);
                    continue;
                }

                const int listenerEffectEntityId = entityId[effectEntity].Get();
                const int listenerSourceId =
                    effectEntityGetSource[static_cast<Obj*>(nullptr)](effectEntity);

                triggerDataEffectEntityId[triggerData] = listenerEffectEntityId;
                triggerDataSourceId[triggerData] = listenerSourceId;
                effectContextCopyFromQuery[context](queryContext);
                triggeredEffectsAdd[triggeredEffects](triggerData);
                if (returnedEvent)
                    DiscardedSystem::ReturnListenerQueued(
                        model, returnedCardId, listenerEffectEntityId, listenerSourceId
                    );

                // Presentation-only bridge: preserve the stock listener SourceId
                // used by TriggerData/history, but remember which off-board event
                // card caused this custom trigger.  The presentation system keys
                // this by the exact live EntityModel so prediction/snapshot id
                // reuse cannot bleed into the real match.
                const auto presentationZoneHint =
                    eventName && std::strcmp(eventName, "DiscardOutsidePlayTrigger") == 0
                        ? ZoneTriggeredAbilityPresentationSystem::OffBoardZone::DiscardPile
                        : ZoneTriggeredAbilityPresentationSystem::OffBoardZone::None;

                ZoneTriggeredAbilityPresentationSystem::RegisterOffBoardTriggerPresentation(
                    model,
                    listenerSourceId,
                    effectTargetId[firingEffect].Get(),
                    presentationZoneHint
                );

                ++queued;
            }
        }

        returnToPool[instances](queryContext);
        if (returnedEvent)
            DiscardedSystem::FinishReturn(model, returnedCardId);

        LOG_INFO(
            "CustomComponents/CustomTrigger: FIRE '%s' candidates=%d rejected=%d queued=%d EventSourceId=%d EventTargetId=%d",
            eventName,
            candidates,
            rejected,
            queued,
            effectSourceId[firingEffect].Get(),
            effectTargetId[firingEffect].Get()
        );
        return true;
    }


    // Targeted activation path used by FireTriggerEffectDescriptor.
    //
    // Unlike Fire()/FireCustom(), this is NOT a broadcast. Only effect entities
    // whose EffectEntity.SourceId is the selected primary target card are
    // considered. That lets card creators reactivate one specific card's
    // PlayTrigger / DrawCardTrigger / custom named trigger.
    inline bool FireForEntity(
        const char* eventName,
        int targetSourceId,
        Obj* firingEffect,
        bool ignoreTriggerConditions
    ) {
        LOG_INFO(
            "CustomComponents/CustomTrigger: FIRE-TARGETED ENTER event='%s' TargetSourceId=%d firingEffect=%p IgnoreTriggerConditions=%d",
            eventName ? eventName : "<null>",
            targetSourceId,
            firingEffect,
            ignoreTriggerConditions ? 1 : 0
        );

        if (!eventName || !*eventName || !firingEffect || targetSourceId < 0) {
            LOG_ERR(
                "CustomComponents/CustomTrigger: FIRE-TARGETED invalid arguments"
            );
            return false;
        }

        StockTriggerContext* stockContext =
            FindStockTriggerContextForEffect(
                firingEffect,
                activeStateCheckInstances
            );

        LOG_INFO(
            "CustomComponents/CustomTrigger: FIRE-TARGETED selected producerContext=%p",
            stockContext
        );

        if (!stockContext) {
            LOG_ERR(
                "CustomComponents/CustomTrigger: FIRE-TARGETED '%s' failed: no coherent stock TriggerSystem context TargetSourceId=%d",
                eventName,
                targetSourceId
            );
            return false;
        }

        Obj* model = stockContext->model;
        Obj* instances = stockContext->instances;
        Obj* triggeredEffects = stockContext->triggeredEffects;
        Obj* stockFinder = stockContext->finder;

        if (!model || !instances || !triggeredEffects || !stockFinder) {
            LOG_ERR(
                "CustomComponents/CustomTrigger: FIRE-TARGETED '%s' incomplete TriggerSystem context model=%p instances=%p triggered=%p finder=%p TargetSourceId=%d",
                eventName,
                model,
                instances,
                triggeredEffects,
                stockFinder,
                targetSourceId
            );
            return false;
        }

        Obj* queryContext = BuildEntityQueryContextStockEquivalent(firingEffect, instances);
        LOG_INFO(
            "CustomComponents/CustomTrigger: FIRE-TARGETED queryContext=%p model=%p finder=%p",
            queryContext,
            model,
            stockFinder
        );
        if (!queryContext) {
            LOG_ERR(
                "CustomComponents/CustomTrigger: FIRE-TARGETED '%s' could not build EntityQueryContext TargetSourceId=%d",
                eventName,
                targetSourceId
            );
            return false;
        }

        auto triggerDataClass = BNM::Class("PvZCards.Engine", "TriggerData");
        if (!triggerDataClass.IsValid()) {
            returnToPool[instances](queryContext);
            return false;
        }

        int sourceMatches = 0;
        int triggerMatches = 0;
        int rejected = 0;
        int queued = 0;

        auto* entities = modelEntities[model].Get();

        LOG_INFO(
            "CustomComponents/CustomTrigger: FIRE-TARGETED scan begin entities=%p size=%d",
            entities,
            entities ? entities->size : -1
        );

        if (entities && entities->items) {
            for (int i = 0; i < entities->size; ++i) {
                Obj* effectEntity = entities->items->m_Items[i];
                if (!effectEntity)
                    continue;

                // IMPORTANT:
                // EffectEntity.GetSource(Entity) throws if the entity does not
                // contain an EffectEntity component. model.Entities contains
                // ordinary cards, heroes, zones, etc., so never call GetSource
                // until we have first proven this is one of the requested
                // trigger listener effect entities.
                if (!EntityListensForEvent(
                        effectEntity,
                        eventName,
                        eventName)) {
                    continue;
                }

                ++triggerMatches;

                const int listenerSourceId =
                    effectEntityGetSource[static_cast<Obj*>(nullptr)](effectEntity);
                if (listenerSourceId != targetSourceId)
                    continue;

                ++sourceMatches;

                LOG_INFO(
                    "CustomComponents/CustomTrigger: FIRE-TARGETED listener match EffectEntityId=%d SourceId=%d event='%s'",
                    entityId[effectEntity].Get(),
                    listenerSourceId,
                    eventName
                );

                if (!ignoreTriggerConditions &&
                    !ValidateWithStockTriggerValidator(
                        stockFinder,
                        model,
                        effectEntity,
                        queryContext
                    )) {
                    ++rejected;
                    LOG_INFO(
                        "CustomComponents/CustomTrigger: REJECT-TARGETED '%s' EffectEntityId=%d SourceId=%d",
                        eventName,
                        entityId[effectEntity].Get(),
                        listenerSourceId
                    );
                    continue;
                }

                Obj* triggerData = acquire[instances](
                    reinterpret_cast<Obj*>(triggerDataClass.GetMonoType()),
                    false
                );
                if (!triggerData)
                    continue;

                Obj* context = triggerDataContext[triggerData].Get();
                if (!context) {
                    returnToPool[instances](triggerData);
                    continue;
                }

                triggerDataEffectEntityId[triggerData] =
                    entityId[effectEntity].Get();
                triggerDataSourceId[triggerData] = listenerSourceId;
                effectContextCopyFromQuery[context](queryContext);
                triggeredEffectsAdd[triggeredEffects](triggerData);

                ZoneTriggeredAbilityPresentationSystem::RegisterOffBoardTriggerPresentation(
                    model,
                    listenerSourceId,
                    effectTargetId[firingEffect].Get()
                );

                ++queued;
            }
        }

        returnToPool[instances](queryContext);

        LOG_INFO(
            "CustomComponents/CustomTrigger: FIRE-TARGETED '%s' TargetSourceId=%d sourceMatches=%d triggerMatches=%d rejected=%d queued=%d IgnoreTriggerConditions=%d EventSourceId=%d EventTargetId=%d",
            eventName,
            targetSourceId,
            sourceMatches,
            triggerMatches,
            rejected,
            queued,
            ignoreTriggerConditions ? 1 : 0,
            effectSourceId[firingEffect].Get(),
            effectTargetId[firingEffect].Get()
        );

        return true;
    }

    inline bool Fire(
        const char* triggerComponentName,
        Obj* firingEffect
    ) {
        return FireEvent(
            triggerComponentName,
            triggerComponentName,
            firingEffect
        );
    }

    inline bool FireCustom(
        const char* triggerName,
        Obj* firingEffect
    ) {
        return FireEvent(nullptr, triggerName, firingEffect);
    }

    inline bool FireReturnedFromDiscardPile(Obj* firingEffect) {
        return Fire("ReturnedFromDiscardPileTrigger", firingEffect);
    }

    inline bool FireDiscardOutsidePlay(Obj* firingEffect) {
        return Fire("DiscardOutsidePlayTrigger", firingEffect);
    }


    static Obj* StockTriggerCarrierToEffectHook(
        void* self,
        Obj* instances,
        Obj* effectEntity,
        const void* method
    ) {
        Obj* descriptor = static_cast<Obj*>(self);
        const char* descriptorType = TypeName(descriptor);

        const bool targeted =
            descriptorType &&
            std::strcmp(descriptorType, "FireTriggerEffectDescriptor") == 0;
        const bool global =
            descriptorType &&
            std::strcmp(descriptorType, "FireCustomTriggerEffectDescriptor") == 0;

        if (!targeted && !global) {
            return originalStockTriggerCarrierToEffect(
                self,
                instances,
                effectEntity,
                method
            );
        }

        BNM::Structures::Mono::String* managedName = nullptr;
        bool ignoreTriggerConditions = false;
        if (targeted) {
            managedName = fireTriggerName[descriptor].Get();
            ignoreTriggerConditions =
                fireTriggerIgnoreTriggerConditions[descriptor].Get();
        } else {
            managedName = fireCustomTriggerName[descriptor].Get();
        }

        Obj* runtime = targeted
            ? Components::FireTriggerEffectDescriptor::CreateRuntime(instances)
            : Components::FireCustomTriggerEffectDescriptor::CreateRuntime(instances);

        if (!runtime) {
            LOG_ERR(
                "CustomComponents/CustomTrigger/Carrier: failed to create stock StateCheckEffect type=%s",
                descriptorType ? descriptorType : "<null>"
            );
            return nullptr;
        }

        const int sourceEffectEntityId =
            effectEntity && entityId.IsValid()
                ? entityId[effectEntity].Get()
                : -1;

        effectEffectEntityId[runtime] = sourceEffectEntityId;

        StoreCarrierMetadata(
            runtime,
            managedName,
            ignoreTriggerConditions,
            targeted
        );

        LOG_INFO(
            "CustomComponents/CustomTrigger/Carrier: CREATED stock type=%s runtime=%p TriggerNamePtr=%p TriggerName='%s' IgnoreTriggerConditions=%d EffectEntityId=%d",
            descriptorType ? descriptorType : "<null>",
            runtime,
            managedName,
            managedName ? managedName->str().c_str() : "<null>",
            ignoreTriggerConditions ? 1 : 0,
            sourceEffectEntityId
        );

        return runtime;
    }


    static void StateCheckProcessEffectHook(
        void* self,
        Obj* effect,
        const void* method
    ) {
        Obj* stateCheckInstances = GetStateCheckInstances(self);
        if (stateCheckInstances)
            activeStateCheckInstances = stateCheckInstances;

        LOG_INFO(
            "CustomComponents/CustomTrigger/StateCheck: ENTER self=%p effect=%p activeInstances=%p",
            self,
            effect,
            activeStateCheckInstances
        );

        if (!effect) {
            LOG_ERR(
                "CustomComponents/CustomTrigger/StateCheck: null effect; forwarding stock"
            );
            originalStateCheckProcessEffect(self, effect, method);
            return;
        }

        CarrierMetadata metadata{};
        if (TakeCarrierMetadata(effect, metadata)) {
            const std::string triggerName =
                metadata.triggerName
                    ? metadata.triggerName->str()
                    : std::string();

            LOG_INFO(
                "CustomComponents/CustomTrigger/StateCheck: CUSTOM-CARRIER TriggerName='%s' Targeted=%d IgnoreTriggerConditions=%d SourceId=%d TargetId=%d EffectEntityId=%d",
                triggerName.c_str(),
                metadata.targeted ? 1 : 0,
                metadata.ignoreTriggerConditions ? 1 : 0,
                effectSourceId[effect].Get(),
                effectTargetId[effect].Get(),
                effectEffectEntityId[effect].Get()
            );

            if (triggerName.empty()) {
                LOG_ERR(
                    "CustomComponents/CustomTrigger/StateCheck: custom carrier has empty TriggerName; swallowing"
                );
                return;
            }

            bool fired = false;
            if (metadata.targeted) {
                const int targetId = effectTargetId[effect].Get();
                if (targetId < 0) {
                    LOG_ERR(
                        "CustomComponents/CustomTrigger/StateCheck: targeted carrier missing TargetId"
                    );
                    return;
                }

                fired = FireForEntity(
                    triggerName.c_str(),
                    targetId,
                    effect,
                    metadata.ignoreTriggerConditions
                );
            } else {
                fired = FireCustom(triggerName.c_str(), effect);
            }

            LOG_INFO(
                "CustomComponents/CustomTrigger/StateCheck: custom carrier dispatch result=%d",
                fired ? 1 : 0
            );
            return;
        }

        // Not one of our metadata-tagged carrier effects. Leave stock
        // StateCheck processing completely untouched.
        originalStateCheckProcessEffect(self, effect, method);
        return;
    }

    inline bool RegisterTypes() {
        struct Registration {
            const char* name;
            const char* baseName;
        };

        const Registration registrations[] = {
            {"ReturnedFromDiscardPileTrigger", "MarkerComponent"},
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

            LOG_INFO(
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

        LOG_INFO(
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

        LOG_INFO(
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

        LOG_INFO("CustomComponents/CustomTrigger: runtime dispatcher ready (StateCheck scoped producer + direct TriggerValidator path + carrier bridge)");
        return true;
    }

    inline bool Initialize() {
        return RegisterTypes() && InstallHooks();
    }

} // namespace CustomComponents::Systems::CustomTriggerSystem
