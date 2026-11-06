#include "diagnostics/DiagnosticLogging.hpp"
#pragma once
// Private implementation module. Include PickFromThreeSubmission.hpp, not this file.
namespace CustomComponents::Pick3::Submission {
inline bool Submit(const PreviewRequest& request, const std::vector<int>& indices, bool timeout) {
    CC_SELECTION_DIAG("CustomComponents/PickFromThreeTrace: MANUAL_SUBMIT effect=%d group=%d selected=%zu options=%zu required=%d remote=%d timeout=%d",
             request.effectEntityId, request.effectGroupId, indices.size(),
             request.options.size(), request.requiredTargets, Remote()?1:0, timeout?1:0);
    lastSubmitReachedService = false;
    NetworkTrace::MarkResult("PICK3_CHOICE_ENTER", request.effectEntityId, request.effectGroupId, static_cast<int>(indices.size()));
    if (Remote() && DeadlineExpired()) return false;
    if (!Ready()) {
        LOG_ERR("CustomComponents/Pick3/SUBMIT: REJECT_NOT_READY svc=%d model=%d allowed=%d remote=%d localPhase=%d observed=%d",
            Target(serviceRoot) ? 1 : 0, Target(modelRoot) ? 1 : 0,
            Allowed() ? 1 : 0, Remote() ? 1 : 0,
            observedLocalTargetSubTurn ? 1 : 0, observedVisualPhase ? 1 : 0);
        return false;
    }
    if (request.requiredTargets < 1 || request.requiredTargets > 3 ||
        static_cast<int>(indices.size()) != request.requiredTargets ||
        indices.size() > request.options.size()) {
        LOG_ERR("CustomComponents/Pick3/SUBMIT: REJECT_COUNT required=%d picked=%zu offered=%zu",
                request.requiredTargets, indices.size(), request.options.size());
        return false;
    }
    if (request.effectEntityId == lastEffect && request.effectGroupId == lastGroup) {
        CC_DIAG("CustomComponents/Pick3/SUBMIT: REJECT_DUPLICATE effect=%d", request.effectEntityId);
        return false;
    }
    // Defense in depth: the delayed UI must not submit an opponent's Effect,
    // even if the local turn's visual phase changed after the popup opened.
    if (Remote() && (request.ownerPlayerId < 0 ||
                     request.ownerPlayerId != CapturedLocalPlayerId() ||
                     request.ownerPlayerId != TargetBridge::ActiveOwnerPlayerId())) {
        LOG_ERR("CustomComponents/Pick3/PVP_OWNER: REJECT_FOREIGN_SELECTION effect=%d owner=%d local=%d activeOwner=%d",
                request.effectEntityId, request.ownerPlayerId, CapturedLocalPlayerId(),
                TargetBridge::ActiveOwnerPlayerId());
        return false;
    }
    const bool databaseMode = request.mode == SelectionMode::CardDatabase;
    if (databaseMode && (!ReleaseSafety::CanUseDatabaseSelection(Remote(), request.requiredTargets) ||
        (Remote() && (!originalLocalSelectProcess || !playCard.IsValid() ||
                      !TargetBridge::targetFilterQuery.IsValid() ||
                      !TargetBridge::additionalTargetQuery.IsValid() ||
                      !TargetBridge::findDatabaseEntities.IsValid())))) {
        LOG_ERR("CustomComponents/Pick3/DB5_PVP: TRANSPORT_NOT_READY effect=%d remote=%d required=%d inbound=%d",
                request.effectEntityId, Remote()?1:0, request.requiredTargets,
                originalLocalSelectProcess?1:0);
        return false;
    }
    // A DB5 card GUID is already chosen at this point (never on popup open).
    // The completion recipient is automatic only for one additional target.
    // For non-DB5 Pick3 modes there is NO additional-target auto-selection.
    const int proxy = databaseMode
        ? TargetBridge::SoleDatabaseRecipientAfterCardChoice(request.effectEntityId, request.effectGroupId)
        : kNoEntity;
    if (databaseMode && proxy==kNoEntity) {
        LOG_ERR("CustomComponents/Pick3/DB5: POST_PICK_RECIPIENT_NOT_UNIQUE effect=%d group=%d (no automatic targeting)",
                request.effectEntityId,request.effectGroupId);
        return false;
    }
    if (databaseMode)
        CC_DIAG("CustomComponents/Pick3/DB5: POST_PICK_AUTO_RECIPIENT effect=%d group=%d entity=%d",
                request.effectEntityId,request.effectGroupId,proxy);
    std::vector<int> entities;
    std::vector<CustomComponents::Pick3::Picked::SelectedCard> selectedCards;
    for (int index : indices) {
        if (index < 0 || static_cast<std::size_t>(index) >= request.options.size()) return false;
        const int optionEntity = request.options[static_cast<std::size_t>(index)].entityId;
        const int entity = databaseMode ? proxy : optionEntity;
        // Database recipient has already been resolved against the stock
        // TargetEntityFinder's EntityModel in FindValidHook, and snapshotted
        // as the only legal completion ID. The dependency-injected input
        // model can be a *different* model; re-looking up its ID=0 here
        // spuriously rejects valid local-player entities. Do not conflate
        // the GUID-only UI choice with the automatic stock recipient.
        const bool badChoice = databaseMode && (optionEntity != kNoEntity ||
            request.options[static_cast<std::size_t>(index)].cardGuid <= 0);
        const bool badTargetId = !ValidCompletionTargetId(request.mode, entity);
        const bool repeated = std::find(entities.begin(), entities.end(), entity) != entities.end();
        const bool missingSnapshot = !TargetBridge::StillEligible(
            request.effectEntityId, request.effectGroupId, entity);
        // A weighted GUID represents ONE physical discard/hand/deck entity.
        // Never accept a late UI choice when that exact pair differs from the
        // game's valid-target snapshot. In particular PickFromThree-like effects
        // need the discarded entity ID, not the definition GUID or a copy.
        const bool mismatchedLiveGuid = !databaseMode &&
            !TargetBridge::StillEligibleWithGuid(request.effectEntityId,
                request.effectGroupId, entity,
                request.options[static_cast<std::size_t>(index)].cardGuid);
        if (badChoice || badTargetId || repeated || missingSnapshot || mismatchedLiveGuid) {
            LOG_ERR("CustomComponents/Pick3/SUBMIT: REJECT_STALE_OR_DUPLICATE effect=%d group=%d target=%d "
                    "db=%d badChoice=%d badId=%d repeated=%d snapshotMissing=%d guidMismatch=%d",
                    request.effectEntityId, request.effectGroupId, entity, databaseMode ? 1 : 0,
                    badChoice ? 1 : 0, badTargetId ? 1 : 0,
                    repeated ? 1 : 0, missingSnapshot ? 1 : 0, mismatchedLiveGuid ? 1 : 0);
            return false;
        }
        if (databaseMode)
            CC_DIAG("CustomComponents/Pick3/DB5: RECIPIENT_SNAPSHOT_VALID effect=%d group=%d target=%d guid=%d",
                     request.effectEntityId, request.effectGroupId, entity,
                     request.options[static_cast<std::size_t>(index)].cardGuid);
        CC_SELECTION_DIAG("CustomComponents/Pick3/TARGET_TRACE: effect=%d group=%d selectionMode=%d "
                 "option=%d entityId=%d cardGuid=%d dbProxy=%d",
                 request.effectEntityId, request.effectGroupId, static_cast<int>(request.mode),
                 index, entity, request.options[static_cast<std::size_t>(index)].cardGuid,
                 databaseMode ? 1 : 0);
        entities.push_back(entity);
        selectedCards.push_back({databaseMode ? kNoEntity : entity,
                                 request.options[static_cast<std::size_t>(index)].cardGuid});
    }
    CC_SELECTION_DIAG("CustomComponents/Pick3/SUBMIT_CHECKPOINT: after-targets effect=%d group=%d selected=%zu local=%d", request.effectEntityId, request.effectGroupId, selectedCards.size(), Local() ? 1 : 0);
    Obj* svc = Target(serviceRoot);
    Obj* model = Target(modelRoot);
    if (!svc || !model) {
        LOG_ERR("CustomComponents/Pick3/SUBMIT: SERVICE_UNAVAILABLE effect=%d svc=%d model=%d",
                request.effectEntityId, svc ? 1 : 0, model ? 1 : 0);
        return false;
    }
    // A snapshot from popup-open is not proof that the local service still
    // has this blocking effect when the delayed user choice is dispatched.
    // Reject without ever entering Process() instead of generating failure 16.
    CC_SELECTION_DIAG("CustomComponents/Pick3/SUBMIT_CHECKPOINT: before-live-effect effect=%d group=%d svc=%p model=%p", request.effectEntityId, request.effectGroupId, static_cast<void*>(svc), static_cast<void*>(model));
    if (Local() && !LocalBlockingEffectMatches(request, svc)) {
        // EffectQueue's three concrete internal lists can disagree with the
        // stock input holder's CurrentBlockingEffect while a placeholder or
        // response is being promoted. The holder's post-Update current effect
        // is authoritative for *opening* and *attempting* selection; the
        // stock service still validates and can reject an invalid Play.
        const auto& owned=Systems::TargetingPresentationRefreshSystem::observedTargetingIdentity;
        const bool current=owned.valid && (owned.state==6 || owned.state==7) &&
            owned.effectEntityId==request.effectEntityId &&
            owned.effectGroupId==request.effectGroupId &&
            Target(boundEntityModelRoot)!=nullptr;
        if(!current) {
            NetworkTrace::MarkResult("PICK3_LOCAL_EFFECT_REJECT", request.effectEntityId, request.effectGroupId, 16);
            return false;
        }
        CC_SELECTION_DIAG("CustomComponents/Pick3/IDENTITY: STOCK_HOLDER_SUBMISSION effect=%d group=%d queueListMismatch=1 stockServiceMustValidate=1",request.effectEntityId,request.effectGroupId);
    }
    CC_SELECTION_DIAG("CustomComponents/Pick3/SUBMIT_CHECKPOINT: live-effect-passed effect=%d group=%d", request.effectEntityId, request.effectGroupId);
    const int playerId = localPlayerField[model].Get();
    // A reusable game-owned Play DTO builder is shared by PvE and PvP.
    // The selection rules, Picked query, local completion and retry semantics
    // remain in this PickFromThree adapter exactly as in the working baseline.
    SelectionTransport::TargetSelection action;
    action.playerId = playerId;
    action.entityIds = entities;
    if(request.mode!=SelectionMode::CardDatabase && request.additionalTargetId>=0)
        action.orderInTargetLocation=AdditionalSelection::Encode(request.additionalTargetId);
    // For DB5, Targets remains the real recipient entity, while CardId
    // carries the selected card DEFINITION GUID through the stock serializer.
    // A normal SelectTarget always retains CardId=-1.
    if (databaseMode) action.cardId = selectedCards[0].cardGuid;
    CC_SELECTION_DIAG("CustomComponents/Pick3/VALIDATION_AUDIT: phase=PREPARE_REQUEST effect=%d group=%d mode=%d owner=%d localPlayer=%d inputModel=%p service=%p targetCount=%zu first=%d second=%d third=%d cardId=%d offerCount=%zu rootCount=%zu",
        request.effectEntityId,request.effectGroupId,int(request.mode),request.ownerPlayerId,playerId,
        (void*)model,(void*)svc,entities.size(),entities.size()>0?entities[0]:-1,
        entities.size()>1?entities[1]:-1,entities.size()>2?entities[2]:-1,action.cardId,
        request.options.size(),SelectionCompletion::roots.size());
    for(std::size_t i=0;i<request.options.size() && i<3;++i)
        CC_SELECTION_DIAG("CustomComponents/Pick3/VALIDATION_AUDIT: phase=OFFER effect=%d group=%d index=%zu entity=%d guid=%d stillEligible=%d guidMatch=%d",
            request.effectEntityId,request.effectGroupId,i,request.options[i].entityId,request.options[i].cardGuid,
            int(TargetBridge::StillEligible(request.effectEntityId,request.effectGroupId,request.options[i].entityId)),
            int(TargetBridge::StillEligibleWithGuid(request.effectEntityId,request.effectGroupId,request.options[i].entityId,request.options[i].cardGuid)));
    SelectionTransport::PlayFields playApi{&playClass, &playTargets, &playType,
                                          &playCard, &playPlayer, &playOrder};
    SelectionTransport::Prepared prepared;
    NetworkTrace::MarkResult("PICK3_PREFLIGHT_OK", request.effectEntityId, request.effectGroupId, static_cast<int>(entities.size()));
    if (!SelectionTransport::Prepare(playApi, action, prepared)) {
        LOG_ERR("CustomComponents/Pick3/SUBMIT: PREPARE_FAILED effect=%d", request.effectEntityId);
        return false;
    }
    Obj* play = prepared.object;
    std::uint32_t playRoot = prepared.root;
    if (databaseMode && NetworkTrace::Mode())
        CC_SELECTION_DIAG("CustomComponents/Pick3/DB5_PVP: TX_ENCODE effect=%d group=%d guid=%d recipient=%d player=%d remote=%d",
                 request.effectEntityId,request.effectGroupId,action.cardId,entities[0],playerId,Remote()?1:0);
    NetworkTrace::MarkPrepared(play, request.effectEntityId, request.effectGroupId);
    if (!SelectionTransport::HasProcessMethod(svc)) { Drop(playRoot); return false; }
    // Store before stock Process(): downstream DrawCardFromSubset can evaluate
    // its GUID-domain query synchronously in this call. Failed submissions
    // remove the staged result. Never publish a global, unscoped last GUID.
    // Remote prediction may synchronously evaluate the same scoped custom query.
    // This local cache is NOT network replication; the other client/server must
    // still derive identical results from the stock target action.
    const bool publishForLocalProcess = Local() || (Remote() && allowPvp);
    const bool pickedPublished = publishForLocalProcess && CustomComponents::Pick3::Picked::Store(
        request.effectEntityId, request.effectGroupId, selectedCards, "TX_SUBMIT", &selectedTriggerIdentity.selectionSequence);
    if (pickedPublished) CustomComponents::Pick3::Picked::TraceLedger("TX_STAGED", request.effectEntityId, request.effectGroupId, selectedCards.empty() ? -1 : (selectedCards.front().entityId > 0 ? selectedCards.front().entityId : selectedCards.front().cardGuid));
    if (publishForLocalProcess && !pickedPublished) {
        LOG_ERR("CustomComponents/Pick3/PICKED_QUERY: STORE_FAILED effect=%d group=%d",
                request.effectEntityId, request.effectGroupId);
        Drop(playRoot);
        return false;
    }
    if (pickedPublished) {
        if (databaseMode)
            CC_DIAG("CustomComponents/Pick3/DB5: GUID_SELECTED effect=%d guid=%d stockProxy=%d",
                request.effectEntityId, selectedCards[0].cardGuid, proxy);
        CC_DIAG("CustomComponents/Pick3/PICKED_QUERY: STORE effect=%d group=%d count=%zu guid=%d,%d,%d",
                 request.effectEntityId, request.effectGroupId, selectedCards.size(),
                 selectedCards.size() > 0 ? selectedCards[0].cardGuid : -1,
                 selectedCards.size() > 1 ? selectedCards[1].cardGuid : -1,
                 selectedCards.size() > 2 ? selectedCards[2].cardGuid : -1);
    }
    // Only this exact pair passed StillEligibleWithGuid at the submission
    // boundary. Preserve its proof of validation across stock Process(),
    // which may retire or replace the offer snapshot before trigger dispatch.
    verifiedLiveChoice = {};
    if (!databaseMode && selectedCards.size() == 1 && request.requiredTargets == 1 &&
        selectedCards[0].entityId > 0 && selectedCards[0].cardGuid > 0) {
        verifiedLiveChoice = {request.effectEntityId, request.effectGroupId,
                              selectedCards[0].entityId, selectedCards[0].cardGuid, true};
        CC_SELECTION_DIAG("CustomComponents/Pick3/PICK3_LIVE_CAPTURE: effect=%d group=%d entity=%d guid=%d verifiedAtSubmit=1",
                 verifiedLiveChoice.effectId, verifiedLiveChoice.groupId,
                 verifiedLiveChoice.entityId, verifiedLiveChoice.cardGuid);
    }
    // Capture the live, group-matched Effect before the stock Process() can
    // retire it; never reconstruct one from recycled numeric IDs afterward.
    CaptureSelectionTriggerContext(request,svc);
    databaseEarlyArmed = request.mode == SelectionMode::CardDatabase && Local() &&
        selectedTriggerIdentity.valid && selectedTriggerEffectId == request.effectEntityId &&
        selectedTriggerGroupId == request.effectGroupId;
    databaseEarlyQueued = false;
    liveEarlyQueued = false;
    liveEarlyArmed = request.mode != SelectionMode::CardDatabase && Local() &&
        request.requiredTargets == 1 && verifiedLiveChoice.valid &&
        selectedTriggerIdentity.valid &&
        selectedTriggerIdentity.effectEntityId == request.effectEntityId &&
        selectedTriggerIdentity.effectGroupId == request.effectGroupId;
    if (liveEarlyArmed) {
        liveEarlyRequest = request;
        Obj* serviceFinder = localServiceFinder.IsValid()
            ? localServiceFinder[svc].Get() : nullptr;
        Obj* entityFinder = serviceFinder && TargetBridge::finderEntityFinder.IsValid()
            ? TargetBridge::finderEntityFinder[serviceFinder].Get() : nullptr;
        Obj* serviceModel = entityFinder && TargetBridge::entityFinderModel.IsValid()
            ? TargetBridge::entityFinderModel[entityFinder].Get() : nullptr;
        auto* producer = serviceModel
            ? CustomComponents::Systems::CustomTriggerSystem::FindStockTriggerContextForModel(serviceModel)
            : nullptr;
        if (producer && producer->triggeredEffects) {
            CustomComponents::Systems::CustomTriggerSystem::liveSelectionQueueScope = producer->triggeredEffects;
            CustomComponents::Systems::CustomTriggerSystem::liveSelectionQueueCallback = &OnLiveStockQueueBoundary;
        } else liveEarlyArmed = false;
        CC_SELECTION_DIAG("CustomComponents/Pick3/PICK3_LIVE_EARLY: phase=ARM effect=%d group=%d armed=%d", request.effectEntityId,request.effectGroupId,int(liveEarlyArmed));
    }
    if (databaseEarlyArmed) {
        databaseEarlyRequest = request;
        CreationProvenance::onVerifiedDatabaseDraw = &OnVerifiedDatabaseDraw;
    }
    // The live service collection is the exact object passed by stock to
    // EffectCollectionProcessor.ProcessEffects during this selection.
    if (request.mode == SelectionMode::CardDatabase && Local())
        CustomComponents::Systems::CustomTriggerSystem::WatchPick3ResumeEffects(
            svc ? *reinterpret_cast<Obj**>(reinterpret_cast<std::uint8_t*>(svc) + 0x30) : nullptr,
            request.effectEntityId, request.effectGroupId);
    // Lock before entering game-owned code: it may synchronously re-enter UI.
    lastEffect = request.effectEntityId; lastGroup = request.effectGroupId;

    CC_DIAG("CustomComponents/Pick3/SUBMIT: SEND mode=%s reason=%s effect=%d group=%d count=%zu targets=%d,%d,%d player=%d type=6",
        Remote() ? "PVP_REMOTE" : "PVE_LOCAL", timeout ? "TIMER" : "CLICK",
        request.effectEntityId, request.effectGroupId, entities.size(),
        entities.size() > 0 ? entities[0] : -1,
        entities.size() > 1 ? entities[1] : -1,
        entities.size() > 2 ? entities[2] : -1, playerId);
    // Observe completion before entering Process: local services may finish
    // synchronously; remote services can finish on a later frame.
    CC_SELECTION_DIAG("CustomComponents/Pick3/HANDOFF_TRACE: phase=BEFORE_BEGIN effect=%d group=%d service=%p play=%p player=%d targets=%zu currentCompletion=%d",
        request.effectEntityId, request.effectGroupId, (void*)svc, (void*)play, playerId,
        entities.size(), int(SelectionCompletion::Status()));
    if (!SelectionCompletion::Begin(svc,play)) {
        CustomComponents::Systems::CustomTriggerSystem::liveSelectionQueueScope = nullptr;
        CustomComponents::Systems::CustomTriggerSystem::liveSelectionQueueCallback = nullptr;
        liveEarlyArmed = false;
        liveEarlyQueued = false;
        databaseEarlyArmed = false;
        Drop(playRoot);
        if (pickedPublished) Picked::Erase(request.effectEntityId, request.effectGroupId);
        lastEffect = lastGroup = -1;
        Drop(selectedTriggerEffectRoot);
        selectedTriggerEffectId=selectedTriggerGroupId=-1;
        selectedTriggerIdentity = {};
        verifiedLiveChoice = {};
        NetworkTrace::MarkResult("PICK3_CALLBACK_SETUP_FAILED", request.effectEntityId, request.effectGroupId, -1);
        LOG_ERR("CustomComponents/Pick3/SUBMIT: CALLBACK_SETUP_FAILED (not dispatched)");
        return false;
    }
    CC_SELECTION_DIAG("CustomComponents/Pick3/HANDOFF_TRACE: phase=BEGIN_ATTACHED effect=%d group=%d service=%p ticket=%llu priorSuccessHandler=%d completion=%d",
        request.effectEntityId, request.effectGroupId, (void*)svc,
        (unsigned long long)SelectionCompletion::activeTicket,
        int(SelectionCompletion::hadSuccessHandler), int(SelectionCompletion::Status()));
    lastSubmitReachedService = true;
    // Visual transition starts on a verified single dispatch, not on success
    // or an ACK. It never owns or delays the stock selection service.
    if (onPick3PlayDispatch) onPick3PlayDispatch(request.effectEntityId,request.effectGroupId);
    NetworkTrace::MarkStage("PICK3_DISPATCH_ENTER", play);
    TransactionRuntime::ScopedSubmission selectionSubmission({request.effectEntityId,
        request.effectGroupId,selectedTriggerIdentity.selectionSequence,entities,selectedCards});
    const auto dispatch = SelectionTransport::Dispatch(svc, play);
    // The attached managed receiver now owns the request across late callbacks.
    Drop(playRoot);
    NetworkTrace::MarkStage("PICK3_DISPATCH_RETURN", play);
    CC_SELECTION_DIAG("CustomComponents/Pick3/HANDOFF_TRACE: phase=DISPATCH_RETURN effect=%d group=%d dispatch=%d completion=%d reason=%d",
        request.effectEntityId, request.effectGroupId, int(dispatch),
        int(SelectionCompletion::Status()), SelectionCompletion::completion.Reason());
    if (selectedTriggerIdentity.valid)
        LogPick3EffectLifetime("DISPATCH_RETURN", Target(selectedTriggerEffectRoot));
    CustomComponents::Systems::CustomTriggerSystem::liveSelectionQueueScope = nullptr;
    CustomComponents::Systems::CustomTriggerSystem::liveSelectionQueueCallback = nullptr;
    liveEarlyArmed = false;
    if (dispatch != SelectionTransport::DispatchResult::Returned)
        SelectionCompletion::MarkUncertain();
    CC_DIAG("CustomComponents/Pick3/SUBMIT: AWAIT_RESULT effect=%d dispatch=%d",
            request.effectEntityId, static_cast<int>(dispatch));
    return true; // Entered transaction, NOT confirmation of acceptance.
}
inline bool AwaitingResult() {
    return SelectionCompletion::Status() != SelectionCompletion::Result::Idle;
}
inline SelectionCompletion::Result PollResult() { return SelectionCompletion::Status(); }
// Experimental CardDatabase-only pre-return dispatch. The bound draw entity is
// available inside the stock CreateCard return hook. This runs only for an
// armed, exactly matching local selection; regular PickFromThree is untouched.
}
