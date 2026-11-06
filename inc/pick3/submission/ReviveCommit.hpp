#include "diagnostics/DiagnosticLogging.hpp"
#pragma once
// Private implementation module. Include PickFromThreeSubmission.hpp, not this file.
namespace CustomComponents::Pick3::Submission {
inline bool EmitSelectedTrigger(const PreviewRequest& request, bool success) {
    // The model-side acceptance/creation pipeline now owns this event. UI
    // success and early queue callbacks cannot race it or claim its generation.
    if(TransactionRuntime::pipelineReady) return false;
    // Only the verified local service success is a safe local emission.
    // A PvP predicted success is NOT a server acknowledgement: fail closed
    // here until stock replicated completion can be identified on both peers.
    const bool triggerIdentityMatches=selectedTriggerEffectRoot &&
        selectedTriggerEffectId==request.effectEntityId &&
        selectedTriggerGroupId==request.effectGroupId;
    bool emitted = false;
    if (success && triggerIdentityMatches && Local()) {
        Obj* sourceEffect=Target(selectedTriggerEffectRoot);
        LogPick3EffectLifetime("TRIGGER_FIRE", sourceEffect);
        Obj* exactModel=Target(modelRoot);
        // The UI/command GameModel is not necessarily the EntityModel used by
        // the local SelectTargetService's simulation finder. Use the SAME
        // service which just accepted the choice as a second exact anchor.
        // Never guess from recycled entity IDs or select an arbitrary producer.
        Obj* serviceModel = nullptr;
        Obj* completionService = Target(serviceRoot);
        Obj* serviceFinder = completionService && localServiceFinder.IsValid()
            ? localServiceFinder[completionService].Get() : nullptr;
        Obj* selectionEntityFinder = serviceFinder && TargetBridge::finderEntityFinder.IsValid()
            ? TargetBridge::finderEntityFinder[serviceFinder].Get() : nullptr;
        if (selectionEntityFinder && TargetBridge::entityFinderModel.IsValid())
            serviceModel = TargetBridge::entityFinderModel[selectionEntityFinder].Get();

        // Prefer the local selection service's own model when it has an exact
        // registered stock TriggerSystem producer. The UI model is a safe
        // fallback only when it too has an exact matching producer.
        Obj* dispatchModel = nullptr;
        auto* serviceProducer = serviceModel
            ? CustomComponents::Systems::CustomTriggerSystem::FindStockTriggerContextForModel(serviceModel)
            : nullptr;
        if (serviceProducer) dispatchModel = serviceModel;
        else if (exactModel && exactModel != serviceModel &&
                 CustomComponents::Systems::CustomTriggerSystem::FindStockTriggerContextForModel(exactModel))
            dispatchModel = exactModel;
        CC_SELECTION_DIAG("CustomComponents/Pick3/PICK3_SCOPE: effect=%d group=%d uiModel=%p serviceModel=%p producerModel=%p serviceMatch=%d local=1",
                 request.effectEntityId,request.effectGroupId,(void*)exactModel,
                 (void*)serviceModel,(void*)dispatchModel,serviceProducer?1:0);
        // The database path starts with a GUID proxy, not a live event target.
        // After stock draw, only the verified, single bound entity may become
        // the target for this synthetic PickFromThree event. Regular modes
        // preserve their original pre-dispatch target without alteration.
        auto dispatchIdentity = selectedTriggerIdentity;
        const bool databaseSelection = request.mode == SelectionMode::CardDatabase;
        int boundDatabaseEntity = -1;
        const bool exactDatabaseBinding = databaseSelection &&
            Picked::TryGetSingleBoundDatabaseEntity(
                request.effectEntityId, request.effectGroupId, boundDatabaseEntity);
        if (databaseSelection && dispatchIdentity.valid && exactDatabaseBinding)
            dispatchIdentity.targetId = boundDatabaseEntity;
        // For live modes, the explicit player choice is an existing entity,
        // not a newly created CardDatabase entity. Preserve the captured
        // effect source; change ONLY the event target, and only when the
        // exact choice remains in the stock-validated selection snapshot.
        int boundLiveEntity = -1;
        bool exactLiveBinding = false;
        if (!databaseSelection && dispatchIdentity.valid && request.requiredTargets == 1) {
            int chosenGuid = -1;
            if (Picked::TryGetSingleLiveEntity(request.effectEntityId,
                    request.effectGroupId, boundLiveEntity, chosenGuid) &&
                verifiedLiveChoice.valid &&
                verifiedLiveChoice.effectId == request.effectEntityId &&
                verifiedLiveChoice.groupId == request.effectGroupId &&
                verifiedLiveChoice.entityId == boundLiveEntity &&
                verifiedLiveChoice.cardGuid == chosenGuid) {
                exactLiveBinding = true;
                dispatchIdentity.targetId = boundLiveEntity;
            }
        }
        CC_SELECTION_DIAG("CustomComponents/Pick3/PICK3_LIVE_CONTEXT: effect=%d group=%d source=%d target=%d verified=%d single=%d",
                 request.effectEntityId, request.effectGroupId, dispatchIdentity.sourceId,
                 boundLiveEntity, int(exactLiveBinding), int(request.requiredTargets == 1));
        CC_SELECTION_DIAG("CustomComponents/Pick3/PICK3_TARGET_CONTEXT: mode=%s effect=%d group=%d snapshotValid=%d originalTarget=%d boundEntity=%d bindingVerified=%d applied=%d finalTarget=%d",
                 databaseSelection ? "database" : "regular",
                 request.effectEntityId, request.effectGroupId,
                 int(selectedTriggerIdentity.valid), selectedTriggerIdentity.targetId,
                 boundDatabaseEntity, int(exactDatabaseBinding),
                 int(databaseSelection && dispatchIdentity.valid && exactDatabaseBinding),
                 dispatchIdentity.targetId);
        // A database choice must be bound to the exact created entity before
        // delivering the event to listeners. Mode alone is not evidence that
        // the stock draw completed or that the event target exists.
        const bool liveTargetReady = dispatchIdentity.valid &&
            (databaseSelection ? exactDatabaseBinding : exactLiveBinding);
        const bool fired=sourceEffect && dispatchModel && liveTargetReady &&
            CustomComponents::Systems::CustomTriggerSystem::FireInModel(
                "PickFromThreeTrigger",sourceEffect,dispatchModel,
                dispatchIdentity.valid ? &dispatchIdentity : nullptr);
        CC_SELECTION_DIAG("CustomComponents/Pick3/PICK3_TRIGGER_TIMELINE: phase=LOCAL_DISPATCH effect=%d group=%d ready=%d fired=%d",
            request.effectEntityId,request.effectGroupId,int(liveTargetReady),int(fired));
        emitted = fired;
        CC_SELECTION_DIAG("CustomComponents/Pick3/PICK3_TRIGGER: effect=%d group=%d local=1 queued=%d model=%p source=%p",
                 request.effectEntityId,request.effectGroupId,fired?1:0,
                 (void*)dispatchModel,(void*)sourceEffect);
    } else if (success) {
        CC_SELECTION_DIAG("CustomComponents/Pick3/PICK3_TRIGGER_TIMELINE: phase=REMOTE_AWAIT_PROVENANCE effect=%d group=%d remote=%d context=%d",
            request.effectEntityId,request.effectGroupId,int(Remote()),int(triggerIdentityMatches));
        CC_SELECTION_DIAG("CustomComponents/Pick3/PICK3_TRIGGER_PROVENANCE: phase=NO_CONFIRMED_REPLICA effect=%d group=%d remote=%d context=%d dispatch=deferred",request.effectEntityId,request.effectGroupId,Remote()?1:0,triggerIdentityMatches?1:0);
        CC_SELECTION_DIAG("CustomComponents/Pick3/PICK3_TRIGGER: deferred effect=%d group=%d remote=%d context=%d reason=not-confirmed-stock-replication",
                 request.effectEntityId,request.effectGroupId,Remote()?1:0,triggerIdentityMatches?1:0);
    }
    return emitted;
}

inline bool OnLiveStockQueueBoundary(Obj* triggers) {
    if (!liveEarlyArmed || liveEarlyQueued || !Local() || !triggers) return false;
    auto* producer = CustomComponents::Systems::CustomTriggerSystem::liveSelectionQueueScope;
    if (producer != triggers) return false;
    liveEarlyArmed = false; // one attempt per transaction; no reentrancy
    const bool fired = EmitSelectedTrigger(liveEarlyRequest, true);
    liveEarlyQueued = fired;
    CC_SELECTION_DIAG("CustomComponents/Pick3/PICK3_LIVE_EARLY: phase=STOCK_QUEUE effect=%d group=%d fired=%d fallback=%d",
             liveEarlyRequest.effectEntityId,liveEarlyRequest.effectGroupId,int(fired),int(!fired));
    return fired;
}

inline void OnVerifiedDatabaseDraw(int sourceEffect, int createdId) {
    if (!databaseEarlyArmed || databaseEarlyBusy || databaseEarlyQueued || !Local() ||
        sourceEffect != databaseEarlyRequest.effectEntityId || createdId <= 0) return;
    int boundId = -1;
    if (!Picked::TryGetSingleBoundDatabaseEntity(databaseEarlyRequest.effectEntityId,
            databaseEarlyRequest.effectGroupId, boundId) || boundId != createdId) return;
    databaseEarlyBusy = true;
    const bool fired = EmitSelectedTrigger(databaseEarlyRequest, true);
    databaseEarlyBusy = false;
    databaseEarlyQueued = fired;
    CC_SELECTION_DIAG("CustomComponents/Pick3/PICK3_EARLY: phase=DRAW_CREATE effect=%d group=%d entity=%d queued=%d fallback=%d",
        sourceEffect, databaseEarlyRequest.effectGroupId, createdId, int(fired), int(!fired));
}

// Read-only snapshot of the concrete blocking-effect queue. Never Peek(),
// dequeue, rebind, or signal the stock input-state holder from this observer.
// Observe the exact stock queued choice stage without changing the queue.
// Phase is a field of ChooseTargetBlockingEffect (0=primary, 1=secondary),
// not a property of the generic BlockingEffect. All reads are synchronous.
// Guarded pre-Process repair for a reversed revive target pair. This is the
// *early* handoff, not a post-callback queue lookup. The two lists of response
// effects are also checked because the game may move effects between them.
inline void CorrectPick3ReviveRolesBeforeStockSelect(Obj* service, int effectId,
    int groupId, int sourceCard, int selectedCard, int ownerPlayer, bool verified) {
    if (!verified || !service || effectId < 0 || groupId < 0 ||
        sourceCard < 0 || selectedCard < 0 ||
        !IsGenericPick3ReviveEffect(effectId) ||
        !localServiceEffects.IsValid() || !effectQueueConcrete.IsValid() ||
        !boundQueueEffects.IsValid() || !boundQueueResponses.IsValid() ||
        !boundQueueGroupResponses.IsValid() ||
        !liveEffectEntityId.IsValid() || !liveEffectGroupId.IsValid() ||
        !pickFromThreeAuditReturnOrder.IsValid() ||
        !pickFromThreeAuditEffectPrimary.IsValid() ||
        !pickFromThreeAuditEffectSecondary.IsValid()) return;
    Obj* finder=localServiceFinder.IsValid()?localServiceFinder[service].Get():nullptr;
    Obj* entityFinder=finder&&TargetBridge::finderEntityFinder.IsValid()?
        TargetBridge::finderEntityFinder[finder].Get():nullptr;
    Obj* model=entityFinder&&TargetBridge::entityFinderModel.IsValid()?
        TargetBridge::entityFinderModel[entityFinder].Get():nullptr;
    const int initialZone=Pick3VerifiedCastTargetFor(model,sourceCard,effectId);
    PF3_REPAIR_TRACE("CustomComponents/Pick3/REPAIR1710: EARLY_HANDOFF_CONTEXT effect=%d group=%d source=%d player=%d model=%p validatedCast=%d cachedDrag=%d",
        effectId,groupId,sourceCard,ownerPlayer,(void*)model,initialZone,FindDeferredInitialZone(sourceCard));
    Obj* queue = localServiceEffects[service].Get();
    if (!queue || !BNM::IsA(queue, effectQueueConcrete)) return;
    auto* effects = boundQueueEffects[queue].Get();
    auto* responses = boundQueueResponses[queue].Get();
    auto* groupResponses = boundQueueGroupResponses[queue].Get();
    auto validList = [](BNM::Structures::Mono::List<Obj*>* l) {
        return !l || (l->size >= 0 && l->size <= 4096 &&
            (l->size == 0 || l->items));
    };
    if (!validList(effects) || !validList(responses) || !validList(groupResponses)) return;
    static BNM::Field<int> sourceField = BNM::Class(
        "PvZCards.Engine", "Effect").GetField("SourceId").cast<int>();
    if (!sourceField.IsValid()) return;
    Obj* match = nullptr;
    int matches = 0;
    const char* collectionName = "none";
    struct Collection { const char* name; BNM::Structures::Mono::List<Obj*>* entries; };
    const Collection lists[] = {{"Effects", effects}, {"ResponseEffects", responses},
                                {"GroupResponseEffects", groupResponses}};
    for (const auto& list : lists) {
        for (int i = 0; list.entries && i < list.entries->size; ++i) {
            Obj* e = list.entries->items->m_Items[i];
            if (!e || liveEffectEntityId[e].Get() != effectId ||
                liveEffectGroupId[e].Get() != groupId ||
                sourceField[e].Get() != sourceCard ||
                pickFromThreeAuditReturnOrder[e].Get() != kReturnToLaneCarrierMarker)
                continue;
            ++matches;
            match = e;
            collectionName = list.name;
        }
    }
    if (matches != 1 || !match || initialZone < 0) {
        CC_SELECTION_DIAG("CustomComponents/Pick3/PICK3_REVIVE_EARLY_HANDOFF: result=SKIP effect=%d group=%d source=%d chosen=%d initialZone=%d matches=%d",
            effectId, groupId, sourceCard, selectedCard, initialZone, matches);
        return;
    }
    const int primary = pickFromThreeAuditEffectPrimary[match].Get();
    const int secondary = pickFromThreeAuditEffectSecondary[match].Get();
    if (primary == initialZone && secondary == selectedCard) {
        pickFromThreeAuditEffectPrimary[match].Set(selectedCard);
        pickFromThreeAuditEffectSecondary[match].Set(-1);
        CC_SELECTION_DIAG("CustomComponents/Pick3/PICK3_REVIVE_EARLY_HANDOFF: result=CORRECTED effect=%d group=%d collection=%s originalPrimary=%d originalSecondary=%d primary=%d secondary=-1",
            effectId, groupId, collectionName, primary, secondary, selectedCard);
    } else {
        CC_SELECTION_DIAG("CustomComponents/Pick3/PICK3_REVIVE_EARLY_HANDOFF: result=UNCHANGED effect=%d group=%d collection=%s primary=%d secondary=%d chosen=%d zone=%d",
            effectId, groupId, collectionName, primary, secondary, selectedCard, initialZone);
    }
}

inline void BindCommittedPick3RevivePrimary(Obj* service, const TargetBridge::ScopedValidationRecipient& recipient) {
    if (!service || recipient.effect < 0 || recipient.group < 0 || recipient.chosen < 0 ||
        recipient.validated.empty() ||
        std::find(recipient.validated.begin(), recipient.validated.end(), recipient.chosen) == recipient.validated.end() ||
        !localServiceEffects.IsValid() || !effectQueueConcrete.IsValid() ||
        !boundQueueEffects.IsValid() || !boundQueueResponses.IsValid() ||
        !boundQueueGroupResponses.IsValid() || !chooseBlockingClass.IsValid() ||
        !liveEffectEntityId.IsValid() || !liveEffectGroupId.IsValid()) return;
    Obj* queue = localServiceEffects[service].Get();
    if (!queue || !BNM::IsA(queue, effectQueueConcrete)) return;
    // Stock maintains three queue collections. Only scanning Effects can
    // produce a false NO_MATCH after Process() moves a continuation to a
    // response collection. Inspect all three synchronously; never Peek().
    auto* effects = boundQueueEffects[queue].Get();
    auto* responses = boundQueueResponses[queue].Get();
    auto* groupResponses = boundQueueGroupResponses[queue].Get();
    auto validList = [](BNM::Structures::Mono::List<Obj*>* list) {
        return !list || (list->size >= 0 && list->size <= 4096 &&
            (list->size == 0 || list->items));
    };
    if (!validList(effects) || !validList(responses) || !validList(groupResponses)) {
        CC_SELECTION_DIAG("CustomComponents/Pick3/PICK3_REVIVE_CONTINUATION: result=QUEUE_LAYOUT_UNAVAILABLE effect=%d group=%d", recipient.effect, recipient.group);
        return;
    }
    static BNM::Field<int> phase = BNM::Class(
        "PvZCards.Engine.Effects", "ChooseTargetBlockingEffect").GetField("Phase").cast<int>();
    static BNM::Field<int> primary = BNM::Class(
        "PvZCards.Engine", "Effect").GetField("TargetId").cast<int>();
    static BNM::Field<int> secondary = BNM::Class(
        "PvZCards.Engine", "Effect").GetField("SecondaryTargetId").cast<int>();
    if (!phase.IsValid() || !primary.IsValid() || !secondary.IsValid()) return;
    Obj* match = nullptr;
    int matching = 0;
    int sameEffectOtherGroup = 0;
    int sameGroupOtherEffect = 0;
    int otherSecondary = 0;
    int examined = 0;
    const char* matchCollection = nullptr;
    struct QueueCollection { const char* name; BNM::Structures::Mono::List<Obj*>* entries; };
    const QueueCollection collections[] = {
        {"Effects", effects}, {"ResponseEffects", responses},
        {"GroupResponseEffects", groupResponses}
    };
    for (const auto& collection : collections) {
        for (int i = 0; collection.entries && i < collection.entries->size; ++i) {
            Obj* e = collection.entries->items->m_Items[i];
            if (!e) continue;
            ++examined;
            const int effectId = liveEffectEntityId[e].Get();
            const int groupId = liveEffectGroupId[e].Get();
            const bool choose = BNM::IsA(e, chooseBlockingClass);
            const int stage = choose ? phase[e].Get() : -1;
            if (choose && stage == 1 && effectId == recipient.effect && groupId == recipient.group) {
                ++matching;
                match = e;
                matchCollection = collection.name;
            } else if (choose && stage == 1) {
                if (effectId == recipient.effect) ++sameEffectOtherGroup;
                if (groupId == recipient.group) ++sameGroupOtherEffect;
                ++otherSecondary;
            }
            // Only report related candidates and secondary choices; do not
            // mistake an undecodable primary choice for a free lane choice.
            if ((choose && stage == 1) || effectId == recipient.effect || groupId == recipient.group)
                CC_SELECTION_DIAG("CustomComponents/Pick3/PICK3_REVIVE_CONTINUATION: result=ENTRY collection=%s index=%d effect=%d group=%d expectedEffect=%d expectedGroup=%d choose=%d stage=%d primary=%d secondary=%d",
                    collection.name, i, effectId, groupId, recipient.effect, recipient.group,
                    int(choose), stage, primary[e].Get(), secondary[e].Get());
        }
    }
    CC_SELECTION_DIAG("CustomComponents/Pick3/PICK3_REVIVE_CONTINUATION: result=ASSOCIATION effect=%d group=%d chosen=%d effects=%d responses=%d groupResponses=%d examined=%d exact=%d sameEffectOtherGroup=%d sameGroupOtherEffect=%d otherSecondary=%d collection=%s",
        recipient.effect, recipient.group, recipient.chosen, effects ? effects->size : 0,
        responses ? responses->size : 0, groupResponses ? groupResponses->size : 0,
        examined, matching, sameEffectOtherGroup,
        sameGroupOtherEffect, otherSecondary, matchCollection ? matchCollection : "none");
    if (matching != 1 || !match) {
        CC_SELECTION_DIAG("CustomComponents/Pick3/PICK3_REVIVE_PRIMARY_BIND: result=%s effect=%d group=%d chosen=%d matches=%d",
            matching == 0 ? "NO_MATCH" : "SKIP_AMBIGUOUS", recipient.effect,
            recipient.group, recipient.chosen, matching);
        return;
    }
    const int oldPrimary = primary[match].Get();
    const int oldSecondary = secondary[match].Get();
    // Never overwrite an already assigned card, nor infer a lane from the
    // initial casting zone. The stock secondary choice still asks for a lane.
    if (oldPrimary != -1 || oldSecondary != -1) {
        CC_SELECTION_DIAG("CustomComponents/Pick3/PICK3_REVIVE_PRIMARY_BIND: result=SKIP_BOUND effect=%d group=%d chosen=%d primary=%d secondary=%d",
            recipient.effect, recipient.group, recipient.chosen, oldPrimary, oldSecondary);
        return;
    }
    primary[match].Set(recipient.chosen);
    CC_SELECTION_DIAG("CustomComponents/Pick3/PICK3_REVIVE_PRIMARY_BIND: result=BOUND effect=%d group=%d chosen=%d primary=%d secondary=%d stage=1 collection=%s",
        recipient.effect, recipient.group, recipient.chosen, primary[match].Get(), secondary[match].Get(), matchCollection);
}
}
