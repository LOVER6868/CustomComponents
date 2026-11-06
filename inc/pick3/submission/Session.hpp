#include "diagnostics/DiagnosticLogging.hpp"
#pragma once
// Private implementation module. Include PickFromThreeSubmission.hpp, not this file.
namespace CustomComponents::Pick3::Submission {
inline bool Local(); // Forward declaration: implementation follows below.
inline void CaptureSelectionTriggerContext(const PreviewRequest& request, Obj* service) {
    Drop(selectedTriggerEffectRoot);
    selectedTriggerEffectId=selectedTriggerGroupId=-1;
    selectedTriggerIdentity = {};
    // A remote service does not have the local service's queue layout.
    // Never read localServiceEffects[remoteService]: that yields an invalid
    // queue pointer and can segfault inside BNM::IsA during PvP submission.
    // This snapshot is used by the local early-effect handoff only.
    if (!service || !Local()) return;
    if (!localServiceEffects.IsValid() || !effectQueueConcrete.IsValid() ||
        !boundQueueEffects.IsValid() || !boundQueueResponses.IsValid() ||
        !boundQueueGroupResponses.IsValid() || !liveEffectEntityId.IsValid() ||
        !liveEffectGroupId.IsValid()) return;
    Obj* queue=localServiceEffects[service].Get();
    if (!queue || !BNM::IsA(queue,effectQueueConcrete)) return;
    auto capture=[&](BNM::Structures::Mono::List<Obj*>* entries) {
        if (!entries || entries->size<0 || entries->size>4096 ||
            (entries->size && !entries->items)) return false;
        for (int i=0;i<entries->size;i++) {
            Obj* effect=entries->items->m_Items[i];
            if (effect && liveEffectEntityId[effect].Get()==request.effectEntityId &&
                liveEffectGroupId[effect].Get()==request.effectGroupId) {
                CaptureRoot(selectedTriggerEffectRoot,effect);
                if (!selectedTriggerEffectRoot) return false;
                selectedTriggerIdentity = CustomComponents::Systems::CustomTriggerSystem::ReadPick3EffectIdentity(effect);
                // The snapshot must belong to this exact request, never a
                // stale pooled object or another selection transaction.
                if (selectedTriggerIdentity.effectEntityId != request.effectEntityId ||
                    selectedTriggerIdentity.effectGroupId != request.effectGroupId) {
                    Drop(selectedTriggerEffectRoot);
                    selectedTriggerIdentity = {};
                    return false;
                }
                LogPick3EffectLifetime("CAPTURE", effect);
                selectedTriggerEffectId=request.effectEntityId;
                selectedTriggerGroupId=request.effectGroupId;
                return selectedTriggerEffectRoot!=0;
            }
        }
        return false;
    };
    if (!capture(boundQueueEffects[queue].Get()) &&
        !capture(boundQueueResponses[queue].Get()))
        capture(boundQueueGroupResponses[queue].Get());
    CC_SELECTION_DIAG("CustomComponents/Pick3/TRIGGER_CONTEXT: effect=%d group=%d rooted=%d",
             request.effectEntityId,request.effectGroupId,selectedTriggerEffectRoot?1:0);
}
inline void ResetMatch() {
    SelectionCompletion::Detach();
    // A staged tap belongs to one match and must never poison the next one.
    ClearStockHandoff();
    overlayCapturesInput = false;
    observedLocalTargetSubTurn = false;
    observedVisualPhase = false;
    blockedStockClicks = 0;
    blockedBoardTaps = 0;
    blockedPlayCommands = 0;
    postSubmitInputBudget = 0;
    lifecycleTraceEvents = 0;
    stockTargetExecutes = stockTargetSuccesses = stockTargetFailures = stockTargetReleases = 0;
    lastObservedTargetCommand = 0;
    postSubmitEffect = postSubmitGroup = -1;
    dependencyPollFrame = 0;
    Drop(serviceRoot); Drop(modelRoot); Drop(stackRoot); Drop(binderRoot);
    Drop(selectedTriggerEffectRoot); selectedTriggerEffectId=selectedTriggerGroupId=-1;
    selectedTriggerIdentity = {};
    verifiedLiveChoice = {};
    Drop(updatedSignalRoot); Drop(deadlineTimerRoot);
    Drop(boundSignalRoot); Drop(boundEffectsRoot); Drop(boundEntityModelRoot);
    Drop(reviveLegalPlayModelRoot);
    deferredValidatedPlay={};
    deferredPrimaryCards={};stockSoleInitialPrimary={};
    pick3AcceptedCastPrimaries={};pick3VerifiedInitialCast={};
    pick3SynchronousVerifiedCastTarget=-1;
    pick3PrimaryCaptureModel=nullptr;
    pick3PrimaryCaptureEffect=pick3PrimaryCaptureCard=-1;
    for(auto& zone:deferredInitialZones)zone={};
    CreationProvenance::Reset();
    AdditionalSelection::Reset();
    pendingRefresh=false; pendingRefreshEffect=pendingRefreshGroup=-1;
    pendingRemoteRetirement=false;remoteRetirementEffect=remoteRetirementGroup=-1;
    remoteRetirementPolls=0;
    pendingHandHighlights=false;pendingHandHighlightAge=pendingHandHighlightStable=0;
    pendingStableFrames=pendingRefreshAge=0;
    signalDispatchCount = 0;
    TargetBridge::lastPlayedSourceEntity=-1;
    TargetBridge::lastPlayedCardGuid=-1;
    TransactionRuntime::Reset();
    CustomComponents::Pick3::Picked::ResetMatch();
    NetworkTrace::ResetMatch();
    lastEffect = lastGroup = -1;
    // Retired completion receivers retain their request through the stock service
    // graph; no process-global request roots or cross-match submission cap remain.
}
inline bool Remote() { Obj* svc = Target(serviceRoot); return svc && BNM::IsA(svc, remoteClass); }
inline bool Local() { Obj* svc = Target(serviceRoot); return svc && BNM::IsA(svc, localClass); }
// The stock SubTurn enum value 1 is LocalTargetPhase, but it cannot identify
// effect ownership across the UI's delayed animation and scene transitions.
// Use the actual Effect.PlayerId stored by the queueing target bridge instead.
inline int CapturedLocalPlayerId() {
    Obj* model = Target(modelRoot);
    return model && localPlayerField.IsValid() ? localPlayerField[model].Get() : -1;
}
inline bool PvpOwnerAllowed() {
    Obj* model = Target(modelRoot);
    if (!model || !opponentPlayerField.IsValid() || !localPlayerField.IsValid() ||
        !pvpOwnerGateReady) return false;
    const int local = localPlayerField[model].Get();
    const int opponent = opponentPlayerField[model].Get();
    const int owner = TargetBridge::ActiveOwnerPlayerId();
    // The actual Effect.PlayerId owns this request. A visual subturn snapshot
    // is diagnostic only: it may already have advanced during the UI delay.
    return local >= 0 && local != opponent && owner == local;
}
inline bool Allowed() {
    return enabled && ((Local() && allowPve) ||
                       (Remote() && allowPvp && PvpOwnerAllowed()));
}
// PvP crash containment: do not invoke TurnTimerComponent.GetTimeRemaining.
// The 2026-11-01 submission SIGSEGV reached this managed method through
// BNM::TryInvoke from the retained-timer deadline path. A catch wrapper cannot
// recover from a native SIGSEGV. Until the timer's IL2CPP object lifetime is
// proven, custom auto-pick/deadline decisions must be UNKNOWN rather than
// declaring a remote selection expired or crashing UnityMain.
inline long long TimerRemaining() {
    static unsigned suppressedReads = 0;
    if (++suppressedReads <= 8)
        CC_SELECTION_DIAG("CustomComponents/Pick3/PICK3_TIMER_SAFETY: phase=SKIP_MANAGED_REMAINING reason=prior-native-null-deref fallback=unknown");
    return -1;
}
inline bool DeadlineExpired() {
    if (!Remote()) return false;
    static unsigned suppressedDeadlines = 0;
    if (++suppressedDeadlines <= 8)
        CC_SELECTION_DIAG("CustomComponents/Pick3/PICK3_TIMER_SAFETY: phase=DEADLINE_UNKNOWN remote=1 no-unsafe-invoke=1");
    return false;
}
inline void BeginDeadline() { Drop(deadlineTimerRoot); }
inline bool ShouldAutoPick() {
    // Stock timer/timeout handling remains in charge. In particular, an
    // unreadable timer is NEVER interpreted as an expired deadline.
    return false;
}
inline int RandomIndex(int size) {
    if (size <= 1) return 0;
    std::uniform_int_distribution<int> dist(0, size - 1);
    return dist(rng);
}
inline bool Ready() {
    return SelectionCompletion::Status() == SelectionCompletion::Result::Idle && Allowed() && Target(serviceRoot) && Target(modelRoot) && localPlayerField.IsValid();
}
// All picks are shipped in ONE Play.Targets payload. One click is not one service call.
// Distinguishes safely retryable pre-dispatch validation failures from an
// attempt that entered the game's Process() (which must NEVER be replayed).
inline bool CanRetryLastSubmission() { return !lastSubmitReachedService; }
inline bool databaseEarlyQueued = false;
inline bool liveEarlyQueued = false;
inline bool liveEarlyArmed = false;
inline PreviewRequest liveEarlyRequest{};
inline bool OnLiveStockQueueBoundary(Obj*);
inline bool databaseEarlyArmed = false;
inline bool databaseEarlyBusy = false;
inline PreviewRequest databaseEarlyRequest{};
inline bool EmitSelectedTrigger(const PreviewRequest& request, bool success);
inline void OnVerifiedDatabaseDraw(int sourceEffect, int createdId);
}
