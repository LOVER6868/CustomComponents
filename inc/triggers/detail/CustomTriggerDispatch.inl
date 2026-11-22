#include "diagnostics/DiagnosticLogging.hpp"
    // Generic named-marker dispatcher.  The public trigger remains a real
    // managed component; this routine only replaces the closed TriggerSystem<T>
    // lookup step. Once TriggerData is added, EA's normal
    // TriggeredEffectQueuingSystem owns the rest of execution.
    // Captured while the blocking effect is still owned by stock Process().
    // Plain integers, not a GC root: pooling cannot rewrite this snapshot.
    struct Pick3EffectIdentitySnapshot {
        int sourceId = -1;
        int targetId = -1;
        int springboardId = -1;
        int effectEntityId = -1;
        int effectGroupId = -1;
        bool valid = false;
        std::uint64_t selectionSequence = 0;
    };

    inline Pick3EffectIdentitySnapshot ReadPick3EffectIdentity(Obj* effect) {
        Pick3EffectIdentitySnapshot result;
        if (!effect) return result;
        result.sourceId = Core::Metadata::Bind(effectSourceId,effect).Get();
        result.targetId = Core::Metadata::Bind(effectTargetId,effect).Get();
        result.springboardId = Core::Metadata::Bind(effectSpringboardId,effect).Get();
        result.effectEntityId = Core::Metadata::Bind(effectEffectEntityId,effect).Get();
        result.effectGroupId = Core::Metadata::Bind(effectGroupId,effect).Get();
        result.valid = true;
        return result;
    }

    inline Obj* BuildEntityQueryContextStockEquivalent(
        Obj* effect,
        Obj* instances,
        const Pick3EffectIdentitySnapshot* identitySnapshot = nullptr
    ) {
        if (!effect || !instances || !entityQueryContextType)
            return nullptr;

        // Use the stock context builder with the verified InstanceManager.
        // It supplies the injected model and copies EffectData using the game
        // lifecycle, so this path has no version-specific memory offsets.
        Obj* queryContext=nullptr;
        auto error=BNM::TryInvoke([&]{
            queryContext=Core::Metadata::Bind(effectToEntityQueryContext,effect)(instances);
        });
        if(error.IsValid() || !queryContext)return nullptr;
        if(identitySnapshot && identitySnapshot->valid){
            auto type=BNM::Class("PvZCards.Engine","EntityQueryContext");
            auto source=type.GetMethod("set_SourceId",1).cast<void>();
            auto target=type.GetMethod("set_TargetId",1).cast<void>();
            auto origin=type.GetMethod("set_EffectEntityId",1).cast<void>();
            auto springboard=type.GetMethod("set_SpringboardId",1).cast<void>();
            if(!source.IsValid() || !target.IsValid() || !origin.IsValid() || !springboard.IsValid()){
                Core::Metadata::Bind(returnToPool,instances)(queryContext);return nullptr;
            }
            source[queryContext](identitySnapshot->sourceId);
            target[queryContext](identitySnapshot->targetId);
            origin[queryContext](identitySnapshot->effectEntityId);
            springboard[queryContext](identitySnapshot->springboardId);
        }
        return queryContext;
    }

    // Diagnostic only: preserve every stock validation decision and field.
    struct Pick3ValidationObservation {
        int listenerSourceId = -1;
        int previousSelfId = -1;
        int validationSelfId = -1;
        bool validatorAvailable = false;
        bool sourceExists = false;
        bool validatorCalled = false;
        bool validatorResult = false;
    };

    inline bool ValidateWithStockTriggerValidator(
        Obj* stockFinder,
        Obj* model,
        Obj* effectEntity,
        Obj* queryContext,
        Pick3ValidationObservation* observation = nullptr
    ) {
        if (observation) *observation = {};
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
        Obj* validator = finderValidatorField.IsValid()
            ? Core::Metadata::Bind(finderValidatorField,stockFinder).Get() : nullptr;
        if (!validator || std::strcmp(TypeName(validator),"TriggerValidator")!=0) {
            LOG_ERR("CustomComponents/CustomTrigger: invalid validator finder=%p validator=%p",stockFinder,validator);
            return false;
        }
        if (observation) observation->validatorAvailable = true;

        const int listenerSourceId =
            Core::Metadata::Bind(effectEntityGetSource,static_cast<Obj*>(nullptr))(effectEntity);

        auto selfField=BNM::Class("PvZCards.Engine","EntityQueryContext").GetField("SelfId").cast<int>();
        if(!selfField.IsValid())return false;
        const int previousSelfId =
            selfField[queryContext].Get();

        selfField[queryContext].Set(listenerSourceId);

        const bool sourceExists = listenerSourceId >= 0 &&
            ModelContainsEntityId(model, listenerSourceId);
        if (observation) {
            observation->listenerSourceId = listenerSourceId;
            observation->previousSelfId = previousSelfId;
            observation->validationSelfId = listenerSourceId;
            observation->sourceExists = sourceExists;
        }
        bool result = false;
        if (sourceExists) {
            if (observation) observation->validatorCalled = true;
            result = Core::Metadata::Bind(triggerValidatorShouldTrigger,validator)(
                effectEntity,
                queryContext
            );
            if (observation) observation->validatorResult = result;
        }

        selfField[queryContext].Set(previousSelfId);

        return result;
    }

    inline bool FireEvent(
        const char* markerComponentName,
        const char* eventName,
        Obj* firingEffect,
        Obj* expectedModel = nullptr,
        const Pick3EffectIdentitySnapshot* identitySnapshot = nullptr
    ) {
        if (!eventName || !*eventName || !firingEffect)
            return false;

        StockTriggerContext* stockContext = expectedModel
            ? FindStockTriggerContextForModel(
                expectedModel,
                activeStateCheckInstances
              )
            : FindStockTriggerContextForEffect(
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

        const auto producerLease=stockContext->lifetime;
        Obj* model = stockContext->model;
        // Custom marker events do not pass through the stock generic
        // TriggerSystem<T> processor. Supply their model at the same shared
        // accepted-enqueue boundary so every marker type receives a snapshot.
        CustomComponents::Variables::QueuedTriggerValues::ModelScope snapshotModel(model);
        Obj* instances = stockContext->instances;
        Obj* triggeredEffects = stockContext->triggeredEffects;
        Obj* stockFinder = stockContext->finder;
        if (std::strcmp(eventName, "PickFromThreeTrigger") == 0)
            LogPick3QueueGate("BEFORE_CUSTOM_FIRE", triggeredEffects);

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
        Engine::TriggerDispatch::Scope triggerScope({model,instances,firingEffect,eventName,++Engine::EffectGuards::eventSerial});

        if constexpr (CustomComponents::Diagnostics::kVerbose) CC_DIAG(
            "CustomComponents/CustomTrigger: FIRE before ToEntityQueryContext instances=%p",
            instances
        );

        Obj* queryContext = BuildEntityQueryContextStockEquivalent(firingEffect, instances, identitySnapshot);

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

        namespace TX=CustomComponents::Pick3::Transaction;
        namespace RT=CustomComponents::Pick3::TransactionRuntime;
        // Use the pre-pooling identity supplied by the selection producer.
        // Reading firingEffect again can yield cleared SourceId/TargetId.
        const auto eventIdentity=identitySnapshot && identitySnapshot->valid
            ? *identitySnapshot : ReadPick3EffectIdentity(firingEffect);
        auto eventSelection=TX::active.value;
        if(!TX::Current(eventSelection) || eventSelection->model!=RT::ModelOf(model)) eventSelection={};
        if(RT::pipelineReady && std::strcmp(eventName,"PickFromThreeTrigger")==0) {
            eventSelection=RT::Capture(model,eventIdentity.effectEntityId,
                eventIdentity.effectGroupId,eventIdentity.sourceId,eventIdentity.selectionSequence);
            if(eventSelection && !TX::ClaimEvent(eventSelection)) {
                Core::Metadata::Bind(returnToPool,instances)(queryContext);
                return true; // already delivered this exact model/generation
            }
        }
        TX::Scoped eventSelectionScope(eventSelection,eventIdentity.effectEntityId);
        if(eventSelection) LOG_INFO("CustomComponents/Pick3/PF3_TX: phase=SELECTION_EVENT_BOUND event=%s effect=%d group=%d seq=%llu",eventName,eventSelection->effect,eventSelection->group,(unsigned long long)eventSelection->sequence);

        const bool tracePick3Event =
            std::strcmp(eventName, "PickFromThreeTrigger") == 0;
        if (tracePick3Event) {
            // EntityQueryContext.Model is injected at +0x10, not copied from
            // Effect. Read only after successful pooled acquisition.
            auto* qb = reinterpret_cast<std::uint8_t*>(queryContext);
            Obj* injectedModel = *reinterpret_cast<Obj**>(qb + 0x10);
            Obj* effectCtx = *reinterpret_cast<Obj**>(qb + 0x18);
            const auto* eb = reinterpret_cast<const std::uint8_t*>(firingEffect);
            const auto* cb = reinterpret_cast<const std::uint8_t*>(effectCtx);
            LOG_INFO(
                "CustomComponents/Pick3/PICK3_CONTEXT: firingEffect=%p firingEffectEntity=%d firingSource=%d firingTarget=%d firingSpringboard=%d query=%p queryModel=%p expectedModel=%p producerModel=%p modelMatches=%d querySelf=%d copiedSource=%d copiedTarget=%d copiedEffectEntity=%d",
                firingEffect,
                *reinterpret_cast<const int*>(eb + 0x30),
                *reinterpret_cast<const int*>(eb + 0x24),
                *reinterpret_cast<const int*>(eb + 0x28),
                *reinterpret_cast<const int*>(eb + 0x2C),
                queryContext, injectedModel, expectedModel, model,
                int(injectedModel == model),
                *reinterpret_cast<const int*>(qb + 0x20),
                cb ? *reinterpret_cast<const int*>(cb + 0x14) : -1,
                cb ? *reinterpret_cast<const int*>(cb + 0x10) : -1,
                cb ? *reinterpret_cast<const int*>(cb + 0x18) : -1
            );
        }

        if (tracePick3Event && identitySnapshot) {
            LOG_INFO("CustomComponents/Pick3/PICK3_SNAPSHOT_USED: valid=%d source=%d target=%d springboard=%d effectEntity=%d group=%d",
                int(identitySnapshot->valid), identitySnapshot->sourceId, identitySnapshot->targetId,
                identitySnapshot->springboardId, identitySnapshot->effectEntityId, identitySnapshot->effectGroupId);
        }

        auto triggerDataClass = BNM::Class("PvZCards.Engine", "TriggerData");
        if (!triggerDataClass.IsValid()) {
            Core::Metadata::Bind(returnToPool,instances)(queryContext);
            return false;
        }

        const bool returnedEvent = std::strcmp(eventName, "ReturnedFromDiscardPileTrigger") == 0;
        const int returnedCardId = Core::Metadata::Bind(effectTargetId,firingEffect).Get();
        if (returnedEvent)
            DiscardedSystem::BeginReturn(model, returnedCardId, triggeredEffects);

        int candidates = 0;
        int rejected = 0;
        int queued = 0;
        // 2.10: The firing effect and its accepted event target are constant
        // throughout this dispatch. Resolve the in-flight proof once, not for
        // every candidate listener. Keep the proof available for all event
        // kinds: a non-PlayTrigger dependent filter may use the same scope.
        const bool mayBindInFlight = inFlightSelectionProof != nullptr;
        const auto firingIdentityForProof = mayBindInFlight
            ? eventIdentity : Pick3EffectIdentitySnapshot{};
        const int proofSource = firingIdentityForProof.valid
            ? firingIdentityForProof.sourceId : -1;
        const int proofEffect = firingIdentityForProof.valid
            ? firingIdentityForProof.effectEntityId : -1;
        const int proofTarget = firingIdentityForProof.valid
            ? firingIdentityForProof.targetId : -1;
        const bool proofAccepted = mayBindInFlight &&
            firingIdentityForProof.valid &&
            inFlightSelectionProof(model, proofSource, proofEffect, proofTarget);
        Obj* proofEntity = proofAccepted
            ? FindEntityById(model, proofTarget) : nullptr;
        const int proofGuid = proofEntity
            ? CustomComponents::Queries::InDeckCardGuidOf(proofEntity) : -1;
        const bool hasProof = proofAccepted && proofGuid > 0;

        auto* entities = Core::Metadata::Bind(modelEntities,model).Get();
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
                // The custom PlayTrigger dispatcher is NOT the stock PlayTrigger
                // producer. Log per-listener verdicts only when this custom
                // dispatcher is explicitly invoked, without changing eligibility.
                const bool tracePlayListener = std::strcmp(eventName, "PlayTrigger") == 0;
                const bool tracePick3Listener = std::strcmp(eventName, "PickFromThreeTrigger") == 0;
                const int traceEffectId = tracePlayListener ? Core::Metadata::Bind(entityId,effectEntity).Get() : -1;
                const int traceSourceId = tracePlayListener
                    ? Core::Metadata::Bind(effectEntityGetSource,static_cast<Obj*>(nullptr))(effectEntity) : -1;

                // This is the stock GenericEffectEntityFinder<T>::WillTrigger
                // path.  It sets/restores EntityQueryContext.SelfId and runs
                // TriggerValidator over every Condition component attached to
                // the effect entity.  Do not special-case TriggerTargetFilter,
                // SelfEntityFilter, lane filters, once-per-turn conditions, etc.
                Pick3ValidationObservation validation;
                // A listener may observe the accepted selection only when it
                // belongs to the firing source. Never widen this to a global
                // recent selection or to listeners of other cards.
                const int listenerSourceForProof = hasProof
                    ? (tracePlayListener ? traceSourceId
                       : Core::Metadata::Bind(effectEntityGetSource,static_cast<Obj*>(nullptr))(effectEntity))
                    : -1;
                const bool sameSource = listenerSourceForProof > 0 &&
                    listenerSourceForProof == proofSource;
                const bool bound = hasProof && sameSource;
                if (tracePlayListener && traceSourceId == proofSource && !bound &&
                    proofEffect > 0 && proofTarget > 0) {
                    static thread_local unsigned proofMisses = 0;
                    if (++proofMisses <= 32)
                        LOG_INFO("CustomComponents/Pick3/PATCH2_INFLIGHT: phase=INFLIGHT_PROOF_MISS effect=%d source=%d target=%d listener=%d proven=%d sourceMatches=%d selectedGuid=%d legacyPreserved=1",
                            proofEffect,proofSource,proofTarget,traceEffectId,int(proofAccepted),int(sameSource),proofGuid);
                }
                if (bound && tracePlayListener)
                    LOG_INFO("CustomComponents/Pick3/PATCH2_INFLIGHT: phase=INFLIGHT_SELECTION_BOUND effect=%d source=%d target=%d guid=%d listener=%d proof=STOCK_ACCEPTED_EVENT_TARGET",
                        proofEffect,proofSource,proofTarget,proofGuid,traceEffectId);
                const CustomComponents::Pick3::Picked::InFlight::Identity selection{
                    model, queryContext, proofEffect, proofSource, proofTarget,
                    proofGuid, bound
                };
                bool eligible = false;
                {
                    CustomComponents::Pick3::Picked::InFlight::Scoped selectionScope(selection);
                    eligible = ValidateWithStockTriggerValidator(
                        stockFinder, model, effectEntity, queryContext,
                        tracePick3Listener ? &validation : nullptr);
                }
                if(bound && tracePlayListener)
                    LOG_INFO("CustomComponents/Pick3/PATCH2_INFLIGHT: phase=INFLIGHT_SELECTION_EXPIRED effect=%d listener=%d eligible=%d",
                        proofEffect,traceEffectId,int(eligible));
                if (tracePick3Listener) LOG_INFO(
                    "CustomComponents/Pick3/PICK3_VALIDATE: listenerEffect=%d listenerSource=%d priorSelf=%d validationSelf=%d sourceExists=%d validatorAvailable=%d validatorCalled=%d validatorResult=%d eligible=%d",
                    Core::Metadata::Bind(entityId,effectEntity).Get(), validation.listenerSourceId,
                    validation.previousSelfId, validation.validationSelfId,
                    int(validation.sourceExists), int(validation.validatorAvailable),
                    int(validation.validatorCalled), int(validation.validatorResult), int(eligible)
                );
                if (!eligible) {
                    ++rejected;
                    if (tracePick3Listener) LOG_INFO(
                        "CustomComponents/Pick3/PICK3_LISTENER: verdict=rejected listenerEffect=%d listenerSource=%d model=%p",
                        Core::Metadata::Bind(entityId,effectEntity).Get(),
                        Core::Metadata::Bind(effectEntityGetSource,static_cast<Obj*>(nullptr))(effectEntity), model);
                    if (tracePlayListener) {
                        LOG_INFO(
                            "CustomComponents/PickFromThreeTrace: CUSTOM_PLAY_LISTENER effect=%d listenerEffect=%d listenerSource=%d verdict=rejected model=%p",
                            Core::Metadata::Bind(effectSourceId,firingEffect).Get(), traceEffectId, traceSourceId, (void*)model);
                        LOG_INFO(
                            "CustomComponents/Pick3/PATCH2_TRIGGER: phase=PLAY_LISTENER_REJECTED listenerEffect=%d listenerSource=%d reason=STOCK_TRIGGER_VALIDATOR_FALSE group=UNKNOWN model=%p",
                            traceEffectId, traceSourceId, (void*)model);
                    }
                    if constexpr (CustomComponents::Diagnostics::kVerbose) CC_DIAG(
                        "CustomComponents/CustomTrigger: REJECT '%s' EffectEntityId=%d SourceId=%d",
                        eventName,
                        Core::Metadata::Bind(entityId,effectEntity).Get(),
                        Core::Metadata::Bind(effectEntityGetSource,static_cast<Obj*>(nullptr))(effectEntity)
                    );
                    continue;
                }

                Obj* triggerData = Core::Metadata::Bind(acquire,instances)(
                    reinterpret_cast<Obj*>(triggerDataClass.GetMonoType()),
                    false
                );
                if (!triggerData) {
                    if (tracePlayListener)
                        LOG_ERR("CustomComponents/Pick3/PATCH2_TRIGGER: phase=PLAY_LISTENER_FAILURE listenerEffect=%d reason=TRIGGER_DATA_ACQUIRE_NULL",Core::Metadata::Bind(entityId,effectEntity).Get());
                    continue;
                }

                Obj* context = Core::Metadata::Bind(triggerDataContext,triggerData).Get();
                if (!context) {
                    if (tracePlayListener)
                        LOG_ERR("CustomComponents/Pick3/PATCH2_TRIGGER: phase=PLAY_LISTENER_FAILURE listenerEffect=%d reason=TRIGGER_DATA_CONTEXT_NULL",Core::Metadata::Bind(entityId,effectEntity).Get());
                    Core::Metadata::Bind(returnToPool,instances)(triggerData);
                    continue;
                }

                const int listenerEffectEntityId = Core::Metadata::Bind(entityId,effectEntity).Get();
                const int listenerSourceId =
                    Core::Metadata::Bind(effectEntityGetSource,static_cast<Obj*>(nullptr))(effectEntity);

                Core::Metadata::Bind(triggerDataEffectEntityId,triggerData) = listenerEffectEntityId;
                Core::Metadata::Bind(triggerDataSourceId,triggerData) = listenerSourceId;
                Core::Metadata::Bind(effectContextCopyFromQuery,context)(queryContext);
                Core::Metadata::Bind(triggeredEffectsAdd,triggeredEffects)(triggerData);
                // Preserve verified event-to-listener lineage across the stock
                // queued-effect boundary. No global GUID or latest-pick guess.
#if CUSTOMCOMPONENTS_PF3_ENTITY_LINK_ROUTING
                if(bound && std::strcmp(eventName,"PickFromThreeTrigger")==0)
                    CustomComponents::Pick3::Picked::DeferredLink::Enqueue(
                        listenerEffectEntityId,listenerSourceId,proofTarget,proofGuid,
                        proofEffect,firingIdentityForProof.effectGroupId);
#endif
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
                    Core::Metadata::Bind(effectTargetId,firingEffect).Get(),
                    presentationZoneHint
                );

                ++queued;
                if (tracePick3Listener) LOG_INFO(
                    "CustomComponents/Pick3/PICK3_LISTENER: verdict=queued listenerEffect=%d listenerSource=%d model=%p",
                    listenerEffectEntityId, listenerSourceId, model);
                if (tracePlayListener) {
                    LOG_INFO(
                        "CustomComponents/PickFromThreeTrace: CUSTOM_PLAY_LISTENER effect=%d listenerEffect=%d listenerSource=%d verdict=queued model=%p",
                        Core::Metadata::Bind(effectSourceId,firingEffect).Get(), traceEffectId, traceSourceId, (void*)model);
                    // 2.8: immutable integers only; no new managed getters.
                    // Queued means accepted by TriggeredEffectCollection.Add,
                    // NOT processed, PF3-selected, or applied to the model.
                    LOG_INFO(
                        "CustomComponents/Pick3/PATCH2_TRIGGER: phase=PLAY_LISTENER_QUEUED listenerEffect=%d listenerSource=%d dispatchSource=%d model=%p followupApplied=UNKNOWN originGroup=UNAVAILABLE boundary=TRIGGER_COLLECTION_ADD",
                        listenerEffectEntityId, listenerSourceId,
                        Core::Metadata::Bind(effectSourceId,firingEffect).Get(), (void*)model);
                }
            }
        }

        Core::Metadata::Bind(returnToPool,instances)(queryContext);
        if (returnedEvent)
            DiscardedSystem::FinishReturn(model, returnedCardId);

        // A completed scan is not proof that any PlayTrigger was queued.
        // Keep the ordinary Fire() return contract intact, but make this
        // event's result unambiguous in normal (non-verbose) device logs.
        if (std::strcmp(eventName, "PlayTrigger") == 0) {
            LOG_INFO(
                "CustomComponents/CustomTrigger: PLAY_RESULT model=%p expectedModel=%p exactModel=%d candidates=%d rejected=%d queued=%d EventSourceId=%d EventTargetId=%d",
                model,
                expectedModel,
                int(!expectedModel || expectedModel == model),
                candidates,
                rejected,
                queued,
                Core::Metadata::Bind(effectSourceId,firingEffect).Get(),
                Core::Metadata::Bind(effectTargetId,firingEffect).Get()
            );
        }

        if (std::strcmp(eventName, "PickFromThreeTrigger") == 0) {
            // Captures the same TriggeredEffectCollection that received Add().
            // Never invokes ProcessEffect or alters pending trigger order.
            ObservePick3QueueGate(triggeredEffects);
            LOG_INFO("CustomComponents/Pick3/PICK3_LISTENERS: model=%p expectedModel=%p candidates=%d rejected=%d queued=%d eventSource=%d eventTarget=%d",
                model, expectedModel, candidates, rejected, queued,
                Core::Metadata::Bind(effectSourceId,firingEffect).Get(), Core::Metadata::Bind(effectTargetId,firingEffect).Get());
        }
        // Single shared observer for *every* named custom trigger sent through
        // this dispatcher. This reports accepted enqueue count, not effect
        // resolution or model mutation. Predicted-model scans are suppressed.
        if (reinterpret_cast<std::uintptr_t>(model)==Core::SceneViewContext::ModelIdentity()) {
            char replayName[52]{};
            std::snprintf(replayName,sizeof(replayName),"trigger_%s",eventName);
            Integration::ReplayEventBridge::Publish(replayName, {
                {"sourceId",eventIdentity.sourceId},{"targetId",eventIdentity.targetId},
                {"effectEntityId",eventIdentity.effectEntityId},
                {"groupId",eventIdentity.effectGroupId},
                {"candidates",candidates},{"rejected",rejected},{"queued",queued},
                {"queueOnly",1}});
        }
        CC_DIAG(
            "CustomComponents/CustomTrigger: FIRE '%s' candidates=%d rejected=%d queued=%d EventSourceId=%d EventTargetId=%d",
            eventName,
            candidates,
            rejected,
            queued,
            Core::Metadata::Bind(effectSourceId,firingEffect).Get(),
            Core::Metadata::Bind(effectTargetId,firingEffect).Get()
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

        Engine::TriggerDispatch::Scope triggerScope({model,instances,firingEffect,eventName,++Engine::EffectGuards::eventSerial,targetSourceId});
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
            Core::Metadata::Bind(returnToPool,instances)(queryContext);
            return false;
        }

        int sourceMatches = 0;
        int triggerMatches = 0;
        int rejected = 0;
        int queued = 0;

        auto* entities = Core::Metadata::Bind(modelEntities,model).Get();

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
                    Core::Metadata::Bind(effectEntityGetSource,static_cast<Obj*>(nullptr))(effectEntity);
                if (listenerSourceId != targetSourceId)
                    continue;

                ++sourceMatches;

                if constexpr (CustomComponents::Diagnostics::kVerbose) CC_DIAG(
                    "CustomComponents/CustomTrigger: FIRE-TARGETED listener match EffectEntityId=%d SourceId=%d event='%s'",
                    Core::Metadata::Bind(entityId,effectEntity).Get(),
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
                        Core::Metadata::Bind(entityId,effectEntity).Get(),
                        listenerSourceId
                    );
                    continue;
                }

                Obj* triggerData = Core::Metadata::Bind(acquire,instances)(
                    reinterpret_cast<Obj*>(triggerDataClass.GetMonoType()),
                    false
                );
                if (!triggerData)
                    continue;

                Obj* context = Core::Metadata::Bind(triggerDataContext,triggerData).Get();
                if (!context) {
                    Core::Metadata::Bind(returnToPool,instances)(triggerData);
                    continue;
                }

                Core::Metadata::Bind(triggerDataEffectEntityId,triggerData) =
                    Core::Metadata::Bind(entityId,effectEntity).Get();
                Core::Metadata::Bind(triggerDataSourceId,triggerData) = listenerSourceId;
                Core::Metadata::Bind(effectContextCopyFromQuery,context)(queryContext);
                Core::Metadata::Bind(triggeredEffectsAdd,triggeredEffects)(triggerData);


                ZoneTriggeredAbilityPresentationSystem::RegisterOffBoardTriggerPresentation(
                    model,
                    listenerSourceId,
                    Core::Metadata::Bind(effectTargetId,firingEffect).Get()
                );

                ++queued;
            }
        }

        Core::Metadata::Bind(returnToPool,instances)(queryContext);

        LOG_INFO(
            "CustomComponents/CustomTrigger: FIRE-TARGETED '%s' TargetSourceId=%d sourceMatches=%d triggerMatches=%d rejected=%d queued=%d IgnoreTriggerConditions=%d EventSourceId=%d EventTargetId=%d",
            eventName,
            targetSourceId,
            sourceMatches,
            triggerMatches,
            rejected,
            queued,
            ignoreTriggerConditions ? 1 : 0,
            Core::Metadata::Bind(effectSourceId,firingEffect).Get(),
            Core::Metadata::Bind(effectTargetId,firingEffect).Get()
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

    // Used only where the stock hook provides an exact, authoritative model.
    // Never fall back to matching recycled ids in prediction snapshots.
    inline bool FireInModel(
        const char* triggerName,
        Obj* firingEffect,
        Obj* expectedModel,
        const Pick3EffectIdentitySnapshot* identitySnapshot = nullptr
    ) {
        if (!expectedModel)
            return false;
        return FireEvent(
            triggerName,
            triggerName,
            firingEffect,
            expectedModel,
            identitySnapshot
        );
    }

    inline bool FireCustom(
        const char* triggerName,
        Obj* firingEffect
    ) {
        return FireEvent(nullptr, triggerName, firingEffect);
    }

    // Global listener broadcast; caller must provide a verified completion effect.
    inline bool FirePickFromThreeSelection(Obj* firingEffect) {
        return Fire("PickFromThreeTrigger", firingEffect);
    }

    inline bool FireReturnedFromDiscardPile(Obj* firingEffect) {
        return Fire("ReturnedFromDiscardPileTrigger", firingEffect);
    }

    inline bool FireDiscardOutsidePlay(Obj* firingEffect) {
        return Fire("DiscardOutsidePlayTrigger", firingEffect);
    }


