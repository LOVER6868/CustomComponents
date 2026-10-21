#include "diagnostics/DiagnosticLogging.hpp"
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

        if constexpr (CustomComponents::Diagnostics::kVerbose) CC_DIAG(
            "CustomComponents/CustomTrigger: FIRE before ToEntityQueryContext instances=%p",
            instances
        );

        Obj* queryContext = BuildEntityQueryContextStockEquivalent(firingEffect, instances);

        if constexpr (CustomComponents::Diagnostics::kVerbose) CC_DIAG(
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
                    if constexpr (CustomComponents::Diagnostics::kVerbose) CC_DIAG(
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

        CC_DIAG(
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
        CC_DIAG(
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

        if constexpr (CustomComponents::Diagnostics::kVerbose) CC_DIAG(
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
        if constexpr (CustomComponents::Diagnostics::kVerbose) CC_DIAG(
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

        if constexpr (CustomComponents::Diagnostics::kVerbose) CC_DIAG(
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

                if constexpr (CustomComponents::Diagnostics::kVerbose) CC_DIAG(
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
                    if constexpr (CustomComponents::Diagnostics::kVerbose) CC_DIAG(
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

        CC_DIAG(
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


