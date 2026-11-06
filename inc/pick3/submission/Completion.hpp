#include "diagnostics/DiagnosticLogging.hpp"
#pragma once
// Private implementation module. Include PickFromThreeSubmission.hpp, not this file.
namespace CustomComponents::Pick3::Submission {
inline void TracePickFromThreeQueueAtBoundary(const char* phase, const PreviewRequest& request) {
    if (request.mode != SelectionMode::UniqueGuidTargets ||
        !boundQueueEffects.IsValid() || !effectQueueConcrete.IsValid() ||
        !liveEffectEntityId.IsValid() || !liveEffectGroupId.IsValid()) return;
    Obj* queue = Target(boundEffectsRoot);
    if (!queue || !BNM::IsA(queue, effectQueueConcrete)) {
        CC_SELECTION_DIAG("CustomComponents/Pick3/PICK3_REVIVE_QUEUE_STAGE: boundary=%s effect=%d group=%d inspect=unavailable",
                 phase, request.effectEntityId, request.effectGroupId);
        return;
    }
    auto* list = boundQueueEffects[queue].Get();
    if (!list || list->size < 0 || list->size > 4096 ||
        (list->size && !list->items)) return;
    // These methods are resolved once, not on each queue entry. Absent fields
    // are logged as -1; they must never be guessed from a different class.
    static BNM::Field<int> choicePhase = BNM::Class(
        "PvZCards.Engine.Effects", "ChooseTargetBlockingEffect")
        .GetField("Phase").cast<int>();
    static BNM::Field<int> effectTarget = BNM::Class(
        "PvZCards.Engine", "Effect")
        .GetField("TargetId").cast<int>();
    static BNM::Field<int> effectSecondary = BNM::Class(
        "PvZCards.Engine", "Effect")
        .GetField("SecondaryTargetId").cast<int>();
    int matching = 0;
    for (int i = 0; i < list->size; ++i) {
        Obj* e = list->items->m_Items[i];
        if (!e) continue;
        const int id = liveEffectEntityId[e].Get();
        const int group = liveEffectGroupId[e].Get();
        if (id != request.effectEntityId || group != request.effectGroupId)
            continue;
        ++matching;
        const bool choose = chooseBlockingClass.IsValid() && BNM::IsA(e, chooseBlockingClass);
        const int stage = choose && choicePhase.IsValid() ? choicePhase[e].Get() : -1;
        const int primary = effectTarget.IsValid() ? effectTarget[e].Get() : -1;
        const int secondary = effectSecondary.IsValid() ? effectSecondary[e].Get() : -1;
        CC_SELECTION_DIAG("CustomComponents/Pick3/PICK3_REVIVE_QUEUE_STAGE: boundary=%s effect=%d group=%d index=%d count=%d choose=%d stage=%d primary=%d secondary=%d",
                 phase, id, group, i, list->size, int(choose), stage, primary, secondary);
    }
    CC_SELECTION_DIAG("CustomComponents/Pick3/PICK3_REVIVE_QUEUE_STAGE: boundary=%s effect=%d group=%d queueCount=%d matching=%d frontEffect=%d frontGroup=%d",
             phase, request.effectEntityId, request.effectGroupId, list->size,
             matching,
             list->size && list->items->m_Items[0] ? liveEffectEntityId[list->items->m_Items[0]].Get() : -1,
             list->size && list->items->m_Items[0] ? liveEffectGroupId[list->items->m_Items[0]].Get() : -1);
}

inline void FinishTransaction(const PreviewRequest& request, bool success) {
    if (stockHandoffState==StockHandoffState::Dispatched &&
        stockHandoffRequest.effectEntityId==request.effectEntityId &&
        stockHandoffRequest.effectGroupId==request.effectGroupId) {
        CC_SELECTION_DIAG("CustomComponents/Pick3/STOCK_HANDOFF: COMPLETION effect=%d group=%d stockTargetSuccesses=%u capturedCommand=%p",
                 request.effectEntityId,request.effectGroupId,stockTargetSuccesses,
                 (void*)Target(interceptedCommandRoot));
        ClearStockHandoff();
    }
    pendingHandHighlights=false; // superseded by a newer PickFromThree completion
    // The stock command is absent for the custom popup. Queue a once-only
    // match-scoped refresh on BOTH PvE and PvP; never dispatch in this callback.
    // The normal game state path remains responsible for retiring effects.
    const bool notify = success && !SelectionCompletion::hadSuccessHandler &&
                        !Target(interceptedCommandRoot) && syncLocalCompletion;
    const int reason = SelectionCompletion::completion.Reason();
    NetworkTrace::MarkResult("PICK3_FINISH_TRANSACTION", request.effectEntityId, request.effectGroupId, success ? 0 : reason);
    const int outcome = static_cast<int>(SelectionCompletion::Status());
    Picked::TraceLedger(success ? "CALLBACK_SUCCESS_NOT_COMMIT_PROOF" : "CALLBACK_FAILURE", request.effectEntityId, request.effectGroupId);
    Picked::ObserveCompletion(request.effectEntityId, request.effectGroupId, success);
    const bool uncertain = SelectionCompletion::Status() == SelectionCompletion::Result::Uncertain ||
                           SelectionCompletion::Status() == SelectionCompletion::Result::Pending;
    CC_SELECTION_DIAG("CustomComponents/Pick3/HANDOFF: BEFORE_CLEANUP effect=%d group=%d remote=%d "
             "success=%d queued=%d capture=%d targetSubTurn=%d visualPhase=%d "
             "hadStockSuccessHandler=%d completion=%d",
             request.effectEntityId, request.effectGroupId, Remote() ? 1 : 0,
             success ? 1 : 0, TargetBridge::HasQueuedChoice() ? 1 : 0,
             overlayCapturesInput ? 1 : 0, observedLocalTargetSubTurn ? 1 : 0,
             observedVisualPhase ? 1 : 0, SelectionCompletion::hadSuccessHandler ? 1 : 0, outcome);
    // The command identity and service must still agree at the completion
    // boundary. A mismatched stock command can never be used to refresh UI.
    const bool commandOwnerMatches = Target(interceptedCommandRoot) &&
        Target(interceptedCommandServiceRoot) == Target(serviceRoot);
    CC_DIAG("CustomComponents/Pick3/COMMAND_OWNER: finish effect=%d group=%d remote=%d success=%d cmd=%p match=%d",
            request.effectEntityId, request.effectGroupId, Remote() ? 1 : 0, success ? 1 : 0,
            static_cast<void*>(Target(interceptedCommandRoot)), commandOwnerMatches ? 1 : 0);
    CC_SELECTION_DIAG("CustomComponents/Pick3/RETIRE_BOUNDARY: BEFORE effect=%d group=%d remote=%d status=%d queued=%d capture=%d targetSubTurn=%d signalReady=%d commandMatch=%d",
             request.effectEntityId, request.effectGroupId, Remote() ? 1 : 0,
             outcome, TargetBridge::HasQueuedChoice() ? 1 : 0,
             overlayCapturesInput ? 1 : 0, observedLocalTargetSubTurn ? 1 : 0,
             (Target(interceptedTargetSignalRoot) || Target(updatedSignalRoot)) ? 1 : 0,
             commandOwnerMatches ? 1 : 0);
    TracePickFromThreeQueueAtBoundary("BEFORE_DETACH", request);
    SelectionCompletion::Detach();
    TracePickFromThreeQueueAtBoundary("AFTER_DETACH", request);
    // A successful local service dispatch does not establish that the
    // blocking effect has retired. Keep the same effect/group consumed on
    // both PvE and PvP; only a genuinely new identity can open another offer.
    // On a failed/uncertain dispatch do not silently reroll this effect either.
    const bool rearm = false;
    TargetBridge::CompleteLocalSelection(request.effectEntityId, request.effectGroupId, rearm);
    // The accepted casting snapshot was needed only to prepare this specific
    // effect's choice. Never expose it to another transaction after cleanup.
    if (pick3AcceptedCastPrimaries.effect == request.effectEntityId) {
        CC_SELECTION_DIAG("CustomComponents/Pick3/PICK3_REVIVE_OFFER_BRIDGE: phase=CLEAR effect=%d card=%d",
            request.effectEntityId, pick3AcceptedCastPrimaries.card);
        pick3AcceptedCastPrimaries = {};
    }
    TracePickFromThreeQueueAtBoundary("AFTER_COMPLETE", request);
    if (Remote() && success) {
        // Dispatch success can be prediction-only. Do not claim ACK_CONSUMED.
        pendingRemoteRetirement = true;
        remoteRetirementEffect = request.effectEntityId;
        remoteRetirementGroup = request.effectGroupId;
        remoteRetirementPolls = 0;
        CC_SELECTION_DIAG("CustomComponents/Pick3/PVP_RETIRE: PREDICTION_SUCCEEDED effect=%d group=%d waitingForQueueRetirement=1 notServerACK=1",
                 request.effectEntityId, request.effectGroupId);
    }
    if (!success || (request.mode == SelectionMode::CardDatabase
            ? !databaseEarlyQueued : !liveEarlyQueued))
        EmitSelectedTrigger(request, success);
    else CC_SELECTION_DIAG("CustomComponents/Pick3/PICK3_EARLY: phase=FINISH_SKIP effect=%d group=%d alreadyQueued=1", request.effectEntityId, request.effectGroupId);
    CustomComponents::Systems::CustomTriggerSystem::liveSelectionQueueScope = nullptr;
    CustomComponents::Systems::CustomTriggerSystem::liveSelectionQueueCallback = nullptr;
    liveEarlyArmed = false;
    liveEarlyQueued = false;
    databaseEarlyArmed = false;
    databaseEarlyQueued = false;
    Drop(selectedTriggerEffectRoot);
    selectedTriggerEffectId=selectedTriggerGroupId=-1;
    selectedTriggerIdentity = {};
    verifiedLiveChoice = {};
    if (!success) Picked::Erase(request.effectEntityId, request.effectGroupId);
    lastEffect = lastGroup = -1;
    overlayCapturesInput = false;
    postSubmitEffect = request.effectEntityId;
    postSubmitGroup = request.effectGroupId;
    postSubmitInputBudget = 24;
    CC_SELECTION_DIAG("CustomComponents/Pick3/HANDOFF: AFTER_CLEANUP effect=%d group=%d remote=%d "
             "queued=%d capture=%d rearm=%d notify=%d",
             request.effectEntityId, request.effectGroupId, Remote() ? 1 : 0,
             TargetBridge::HasQueuedChoice() ? 1 : 0, overlayCapturesInput ? 1 : 0,
             rearm ? 1 : 0, notify ? 1 : 0);
    TracePickFromThreeQueueAtBoundary("AFTER_MODAL_CLEANUP", request);
    if (notify) {
        pendingRefreshEffect=request.effectEntityId;
        pendingRefreshGroup=request.effectGroupId;
        pendingRefreshAge=pendingStableFrames=0;
        pendingStockSuccessBaseline=stockTargetSuccesses;
        pendingRefresh=true;
        // No synchronous service lookup here. Stock owns UI refresh; the
        // retired custom repair ticket must not invoke unready injection.
        CC_SELECTION_DIAG("CustomComponents/Pick3/CRASH_FIX_155: post-submit injector re-resolution skipped effect=%d group=%d", pendingRefreshEffect,pendingRefreshGroup);
        CC_DIAG("CustomComponents/Pick3/DEFAULT_REPAIR: REFRESH_PENDING effect=%d group=%d signal=%d queue=%d remote=%d",
                pendingRefreshEffect,pendingRefreshGroup,Target(boundSignalRoot)?1:0,
                Target(boundEffectsRoot)?1:0,Remote()?1:0);
    }
    // Rooted only for this modal transaction; prevent a future selection from
    // using a previous command's injected signal after re-entry or timeout.
    Drop(interceptedTargetSignalRoot);
    Drop(interceptedCommandRoot);
    Drop(interceptedCommandServiceRoot);
    CC_SELECTION_DIAG("CustomComponents/Pick3/RETIRE_BOUNDARY: AFTER effect=%d group=%d remote=%d queued=%d capture=%d targetSubTurn=%d rearm=%d notify=%d",
             request.effectEntityId, request.effectGroupId, Remote() ? 1 : 0,
             TargetBridge::HasQueuedChoice() ? 1 : 0, overlayCapturesInput ? 1 : 0,
             observedLocalTargetSubTurn ? 1 : 0, rearm ? 1 : 0, notify ? 1 : 0);
    CC_SELECTION_DIAG("CustomComponents/Pick3/SUBMIT: RESULT effect=%d group=%d success=%d reason=%d outcome=%d rearmed=%d",
             request.effectEntityId, request.effectGroupId, success ? 1 : 0, reason, outcome, rearm ? 1 : 0);
}
}
