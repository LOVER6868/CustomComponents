#include "diagnostics/DiagnosticLogging.hpp"
#pragma once
// Private implementation module. Include PickFromThreeSubmission.hpp, not this file.
namespace CustomComponents::Pick3::Submission {
inline void PumpDependencies() {
    if (!enabled || !resolveFromInjection || Target(serviceRoot)) return;
    if (++dependencyPollFrame % 60 != 1) return;
    Obj* binder = Target(binderRoot);
    if (!binder || !getInjectionBinder.IsValid() || !getInjectedInstance.IsValid() ||
        !selectServiceInterfaceClass.IsValid()) return;
    Obj* injection = nullptr;
    auto binderEx = BNM::TryInvoke([&]{ injection = getInjectionBinder[binder](); });
    if (binderEx.IsValid()) {
        if (dependencyPollFrame <= 300)
            LOG_ERR("CustomComponents/Pick3/SUBMIT: DI_BINDER_EXCEPTION class=%s message=%s",
                binderEx.ClassName().c_str(), binderEx.Message().c_str());
        return;
    }
    if (!injection) return;
    Obj* service = nullptr;
    auto serviceEx = BNM::TryInvoke([&] {
        service = getInjectedInstance[injection](selectServiceInterfaceClass.GetMonoType(), static_cast<Obj*>(nullptr));
    });
    if (serviceEx.IsValid()) {
        if (dependencyPollFrame <= 300)
            LOG_ERR("CustomComponents/Pick3/SUBMIT: DI_LOOKUP_EXCEPTION class=%s message=%s",
                serviceEx.ClassName().c_str(), serviceEx.Message().c_str());
        return;
    }
    if (service) CaptureService(service, "GameInputStateHolder.CommandBinder.DI");
    else if (dependencyPollFrame <= 300)
        CC_DIAG("CustomComponents/Pick3/SUBMIT: DI_LOOKUP_NULL binder=%p", binder);
}
// Uses the same GameInputStateHolder.CommandBinder DI already proven for
// SelectTargetService. Unlike CardDatabase.GetInstance(), this can return the
// existing game-bound database after content has been mapped. No new hook.
inline Obj* ResolveBoundCardDatabase() {
    Obj* binder = Target(binderRoot);
    if (!binder || !getInjectionBinder.IsValid() || !getInjectedInstance.IsValid() ||
        !TargetBridge::cardDatabaseClass.IsValid()) return nullptr;
    Obj* injection = nullptr;
    auto binderEx = BNM::TryInvoke([&] { injection = getInjectionBinder[binder](); });
    if (binderEx.IsValid() || !injection) return nullptr;
    Obj* database = nullptr;
    auto serviceEx = BNM::TryInvoke([&] {
        database = getInjectedInstance[injection](
            TargetBridge::cardDatabaseClass.GetMonoType(), static_cast<Obj*>(nullptr));
    });
    if (serviceEx.IsValid() || !database ||
        !BNM::IsA(database, TargetBridge::cardDatabaseClass)) return nullptr;
    return database;
}
// Passive input probe: distinguishes a suppressed command from a stock command
// that executes but fails later inside the game's input/target phase.
inline void TracePostSubmitInput(const char* command, bool blocked) {
    if (!postSubmitInputBudget) return;
    --postSubmitInputBudget;
    Obj* svc = Target(serviceRoot);
    CC_SELECTION_DIAG("CustomComponents/Pick3/INPUT_HANDOFF: command=%s blocked=%d remote=%d "
             "capture=%d queued=%d lastEffect=%d lastGroup=%d "
             "targetSubTurn=%d visualPhase=%d completion=%d",
             command, blocked ? 1 : 0, svc && BNM::IsA(svc, remoteClass) ? 1 : 0,
             overlayCapturesInput ? 1 : 0, TargetBridge::HasQueuedChoice() ? 1 : 0,
             postSubmitEffect, postSubmitGroup, observedLocalTargetSubTurn ? 1 : 0,
             observedVisualPhase ? 1 : 0, static_cast<int>(SelectionCompletion::Status()));
}
// Trace actual game command ownership without changing its lifecycle. Do not
// retain managed command objects here: integer identity is only for correlating
// log records within the same match; never dereference a saved address.
inline void LogStockCommandLifecycle(const char* step, Obj* self) {
    if (!enabled || !self || lifecycleTraceEvents >= 160) return;
    ++lifecycleTraceEvents;
    const bool isTarget = commandClass.IsValid() && BNM::IsA(self, commandClass);
    if (!isTarget) return;
    Obj* service = commandService.IsValid() ? commandService[self].Get() : nullptr;
    Obj* signal = targetCommandStateSignal.IsValid() ? targetCommandStateSignal[self].Get() : nullptr;
    Obj* tracker = retainedCommandsTracker.IsValid() ? retainedCommandsTracker[self].Get() : nullptr;
    CC_SELECTION_DIAG("CustomComponents/Pick3/STOCK_COMMAND: step=%s cmd=%p sameAsExecute=%d "
             "service=%p signal=%p tracker=%p remote=%d queued=%d capture=%d "
             "targetSubTurn=%d stockExecute=%u success=%u failure=%u release=%u",
             step, static_cast<void*>(self),
             reinterpret_cast<std::uintptr_t>(self) == lastObservedTargetCommand ? 1 : 0,
             static_cast<void*>(service), static_cast<void*>(signal), static_cast<void*>(tracker),
             service && remoteClass.IsValid() && BNM::IsA(service, remoteClass) ? 1 : 0,
             TargetBridge::HasQueuedChoice() ? 1 : 0, overlayCapturesInput ? 1 : 0,
             observedLocalTargetSubTurn ? 1 : 0, stockTargetExecutes,
             stockTargetSuccesses, stockTargetFailures, stockTargetReleases);
}
static void StockTargetSuccess(Obj* self, const void* method) {
    ++stockTargetSuccesses;
    LogStockCommandLifecycle("SUCCESS_ENTER", self);
    if (originalStockTargetSuccess) originalStockTargetSuccess(self, method);
    LogStockCommandLifecycle("SUCCESS_RETURN", self);
}
static void StockTargetFailure(Obj* self, Obj* failure, const void* method) {
    ++stockTargetFailures;
    LogStockCommandLifecycle("FAILURE_ENTER", self);
    if (originalStockTargetFailure) originalStockTargetFailure(self, failure, method);
    LogStockCommandLifecycle("FAILURE_RETURN", self);
}
static void HeroesRelease(Obj* self, const void* method) {
    const bool isTarget = self && commandClass.IsValid() && BNM::IsA(self, commandClass);
    if (isTarget) {
        ++stockTargetReleases;
        LogStockCommandLifecycle("RELEASE_ENTER", self);
    }
    if (originalHeroesCommandRelease) originalHeroesCommandRelease(self, method);
    if (isTarget) LogStockCommandLifecycle("RELEASE_RETURN", self);
}
inline bool PopupCanOpen() { return enabled; }
inline bool IsCapturingInput() { return enabled && overlayCapturesInput; }

// This real command method is much larger than the compiler-generated setter.
// Capture a previously injected GameModel before the normal card play.
// Always forward to the original: this hook must not change gameplay.
static void PlayCardExecute(Obj* self, const void* method) {
    if (PvpCrashTraceEnabled()) ++pvpCrashTraceSerial;
    PvpCrashMark("PLAY_COMMAND","ENTER",-1,self);
    const bool trace = false;
    if (trace)
        CC_DIAG("CustomComponents/Pick3/SUBMIT: PLAYCARD_EXEC_ENTER modelField=%d", playCardCommandModel.IsValid() ? 1 : 0);
    if (enabled && self && playCardCommandModel.IsValid()) {
        Obj* model = playCardCommandModel[self].Get();
        if (model && Target(modelRoot) != model) {
            CaptureRoot(modelRoot, model);
            CC_DIAG("CustomComponents/Pick3/SUBMIT: MODEL_CAPTURE source=PlayCardExecute rooted=%d", modelRoot ? 1 : 0);
        }
    }
    // Crash fix (2026-10-18): NEVER call EntityModel.GetEntityById on
    // PlayCardCommand.<GameModel>k__BackingField. The latter is
    // PvZCards.Core.Utilities.GameModel, NOT PvZCards.Engine.EntityModel.
    // GameModel.LocalPlayerId is stored at offset 0x18, precisely where
    // EntityModel.GetEntityById expects a List<Entity>*; the optional
    // source-provenance probe therefore caused an arm64 SIGSEGV when a
    // card was released, including on subsequent PickFromThree activations.
    // The primary effect's actual source GUID is already determined by
    // TargetBridge::FindValidHook from Effect.SourceId and its REAL
    // EntityModel, so removing this optional UI-only probe does not
    // change the chooser, its prefab localization, or submission.
    if (enabled && syncLocalCompletion && self && playCardStateUpdatedSignal.IsValid()) {
        Obj* signal = playCardStateUpdatedSignal[self].Get();
        if (signal && gameStateUpdatedSignalClass.IsValid() &&
            BNM::IsA(signal, gameStateUpdatedSignalClass) && Target(updatedSignalRoot) != signal) {
            CaptureRoot(updatedSignalRoot, signal);
            CC_DIAG("CustomComponents/Pick3/SYNC410: SIGNAL_CAPTURE source=PlayCardCommand.Execute rooted=%d dispatch=%d",
                updatedSignalRoot ? 1 : 0, dispatchGameStateUpdated.IsValid() ? 1 : 0);
        }
    }
    if (enabled && overlayCapturesInput) {
        TracePostSubmitInput("PlayCardCommand.Execute", true);
        if (++blockedPlayCommands <= 12)
            CC_DIAG("CustomComponents/Pick3/INPUT: STOCK_PLAYCARD_BLOCKED count=%u", blockedPlayCommands);
        return;
    }
    TracePostSubmitInput("PlayCardCommand.Execute", false);
    Obj* gesture = self && trickGestureField.IsValid() ? trickGestureField[self].Get() : nullptr;
    int src = -1, target = -1, order = -1, handled = -1;
    if (gesture) {
        const auto* bytes = reinterpret_cast<const std::uint8_t*>(gesture);
        src = *reinterpret_cast<const int*>(bytes + 0x10);
        target = *reinterpret_cast<const int*>(bytes + 0x14);
        handled = int(*(bytes + 0x18) != 0);
        order = *reinterpret_cast<const int*>(bytes + 0x1C);
    }
    // Narrow correction of the initial drop gesture, before stock Execute builds
    // its Play. No SelectTargetCommand, no picker/lane selection, no broad bypass.
    if (kDeferredPick3TrialEnabled && src>=0 &&
        deferredDropCard==src && deferredDropSeq==trickActiveDragSeq) {
        PF3_REPAIR_TRACE("CustomComponents/Pick3/REPAIR1712: COMMAND_PREFLIGHT card=%d target=%d handled=%d zone=%d validated=%d targetIsLane=%d",
            src,target,handled,deferredDropZone,
            int(IsRecentValidatedInitialPlay(src,deferredDropZone)),
            int(target!=src && IsReviveDropLaneOnCurrentModel(target)));
    }
    if(kDeferredPick3TrialEnabled && gesture && !handled &&
       trickActiveDragSeq && deferredDropSeq==trickActiveDragSeq &&
       src==deferredDropCard && deferredDropZone>=0 &&
       (PvpCrashMark("DEFERRED_DROP","PREDICATE_ENTER",src,self), true) &&
       IsRecentValidatedInitialPlay(src,deferredDropZone) &&
       FindDeferredInitialZone(src)==deferredDropZone &&
       target!=src && IsReviveDropLaneOnCurrentModel(target)) {
        // In the stock-sole case, the card's *only* eligible discarded
        // primary is a true stock target; transmit it as such. Multi-choice
        // plays retain the previous behavior until a deferred-choice protocol
        // can preserve the player's decision before server validation.
        // The snapshot is only candidate evidence, not an authoritative
        // server ACK. Recheck identity against the rooted generation model,
        // effect and current entity before substituting the initial target.
        const int candidateSole=deferredValidatedPlay.stockSolePrimary;
        Obj* proofModel=Target(reviveLegalPlayModelRoot);
        const auto firstNow=proofModel ? ResolvePick3InitialEffectOnModel(proofModel,src) : decltype(ResolvePick3InitialEffectOnModel(proofModel,src)){};
        const bool soleStillBound=candidateSole>=0 && proofModel &&
            firstNow.complete && firstNow.effectId==deferredValidatedPlay.effect &&
            deferredValidatedPlay.player>=0 &&
            deferredValidatedPlay.guid==TrickGenerationGuid(src) &&
            TrickGenerationGuid(candidateSole)>0 &&
            candidateSole!=src && candidateSole!=deferredDropZone;
        const int provenSole=soleStillBound ? candidateSole : -1;
        // 1.7.22 ownership checkpoint: a multi-choice Play still has NO
        // player-confirmed pre-Play physical selection. Never pretend the
        // existence of valid candidates is equivalent to a selection.
        // A future UI bridge must Stage() an ID-only choice BEFORE release,
        // then Consume() only after checking the live stock legal set.
        if (provenSole < 0 && PrePlayLifecycleTrace::enabled) {
            CC_SELECTION_DIAG("CustomComponents/Pick3/PREPLAY1722: stage=MULTICHOICE_UNRESOLVED "
                     "card=%d guid=%d player=%d effect=%d dropZone=%d "
                     "releaseSerial=%u insideRelease=%d pickerBound=0 "
                     "playRetained=0 gestureChangedBy1722=0",
                     src,deferredValidatedPlay.guid,deferredValidatedPlay.player,
                     deferredValidatedPlay.effect,deferredDropZone,
                     PrePlayLifecycleTrace::releaseSerial,
                     int(PrePlayLifecycleTrace::releaseDepth>0));
        }
        const int outboundTarget=provenSole>=0 ? provenSole : deferredDropZone;
        PF3_REPAIR_TRACE("CustomComponents/Pick3/REPAIR1717: SOLE_OUTBOUND_AUDIT card=%d guid=%d captured=%d verified=%d outgoing=%d zone=%d effect=%d nowEffect=%d drag=%u",
            src,TrickGenerationGuid(src),candidateSole,soleStillBound?1:0,outboundTarget,
            deferredDropZone,deferredValidatedPlay.effect,firstNow.effectId,trickActiveDragSeq);
        if (provenSole>=0)
            PF3_REPAIR_TRACE("CustomComponents/Pick3/REPAIR1716: SOLE_OUTBOUND card=%d guid=%d primary=%d zone=%d drag=%u stockTarget=1",
                src,TrickGenerationGuid(src),provenSole,deferredDropZone,trickActiveDragSeq);
        if(target!=outboundTarget) {
            const int oldTarget=target;
            *reinterpret_cast<int*>(reinterpret_cast<std::uint8_t*>(gesture)+0x14)=outboundTarget;
            target=outboundTarget;
            CC_SELECTION_DIAG("CustomComponents/Pick3/DEFERRED_DROP: phase=CORRECT card=%d guid=%d drag=%u old=%d target=%d zone=%d stockSole=%d",src,TrickGenerationGuid(src),trickActiveDragSeq,oldTarget,target,deferredDropZone,provenSole);
            PF3_REPAIR_TRACE("CustomComponents/Pick3/REPAIR1712: CAST_REWRITE source=%d oldTarget=%d initialTarget=%d effect=%d stockCommand=1",src,oldTarget,target,deferredValidatedPlay.effect);
        } else {
            CC_SELECTION_DIAG("CustomComponents/Pick3/DEFERRED_DROP: phase=ALREADY_ZONE card=%d drag=%u zone=%d",src,trickActiveDragSeq,target);
        }
        // Consume the single-use permission before invoking any nested hooks.
        deferredDropCard=-1; deferredDropZone=-1; deferredDropSeq=0;
    }
    if (TrickFocus(trickSecondaryFocusGuid)) {
        ++trickAcceptanceSeq;
        trickPendingCommand = self;
        trickPendingGuid = trickSecondaryFocusGuid;
        trickPendingSource = src;
        trickPendingTarget = target;
        CC_SELECTION_DIAG("CustomComponents/Pick3/TRICK_ACCEPT: phase=COMMAND_ENTER seq=%u guidHint=%d command=%p source=%d target=%d (guid-correlated-from-drag)", trickAcceptanceSeq, trickPendingGuid, self, src, target);
    }
    if (postSubmitInputBudget && src >= 0) {
        CC_SELECTION_DIAG("CustomComponents/Pick3/PATCH154_PLAY_HANDOFF: source=%d target=%d sourceEqualsTarget=%d capture=%d queued=%d normalPlaySubturn=%d lastEffect=%d lastGroup=%d pendingRefresh=%d stockUnmodified=1",
                 src, target, src==target?1:0, overlayCapturesInput?1:0,
                 TargetBridge::HasQueuedChoice()?1:0, observedLocalTargetSubTurn?0:1,
                 postSubmitEffect, postSubmitGroup, pendingRefresh?1:0);
    }
    CC_SELECTION_DIAG("CustomComponents/Pick3/TRICK_PLACE_DIAG: PLAYCARD_ENTER activeDrag=%u command=%p gesture=%p source=%d target=%d order=%d handled=%d stock=1",
             trickActiveDragSeq, self, gesture, src, target, order, handled);
    PrePlayLifecycleTrace::OnPlay(src,target,handled,false);
    PvpCrashMark("PLAY_COMMAND","STOCK_ENTER",src,self);
    if (originalPlayCardExecute) originalPlayCardExecute(self, method);
    PvpCrashMark("PLAY_COMMAND","STOCK_EXIT",src,self);
    PrePlayLifecycleTrace::OnPlay(src,target,handled,true);
    CC_SELECTION_DIAG("CustomComponents/Pick3/TRICK_PLACE_DIAG: PLAYCARD_EXIT command=%p stockReturned=1", self);
    if (trace)
        CC_DIAG("CustomComponents/Pick3/SUBMIT: PLAYCARD_EXEC_RETURN");
}
// The stock action trampoline is earlier than the transport implementation.
// Keep the staged request defined above this trampoline (see declaration near
// command ownership state); the engine's handler registration happens first.
inline bool Submit(const PreviewRequest&,const std::vector<int>&,bool);
inline bool Ready();
inline void BeginDeadline();
inline bool lastSubmitReachedService = false;
// Stock command handoff is staged by the presentation, never by an unrelated
// target/tap. It is consumed exactly once by SelectTargetCommand.SelectTarget.
// This prevents a custom Process() call from bypassing the stock command's
// HandleServiceSuccess / RemoveServiceHandlers / Release lifecycle.
inline int CapturedLocalPlayerId(); // Defined below; needed for staged-command ownership.
enum class StockHandoffState { Idle, Armed, Dispatched, Failed };
inline bool preferStockCommandHandoff = true;
inline StockHandoffState stockHandoffState = StockHandoffState::Idle;
inline bool HasArmedStockHandoff() { return stockHandoffState==StockHandoffState::Armed; }
inline PreviewRequest stockHandoffRequest{};
inline std::vector<int> stockHandoffIndices;
inline bool (*isStockChooseBoardTap)(Obj*) = nullptr;
inline void (*onPick3PlayDispatch)(int effect,int group) = nullptr;
inline void ClearStockHandoff() {
    stockHandoffRequest={}; stockHandoffIndices.clear();
    stockHandoffState=StockHandoffState::Idle;
}
inline int StockTapRecipient(const PreviewRequest& request, int option) {
    if (option<0 || static_cast<std::size_t>(option)>=request.options.size()) return kNoEntity;
    if (request.mode==SelectionMode::CardDatabase)
        return TargetBridge::SoleDatabaseRecipientAfterCardChoice(request.effectEntityId,request.effectGroupId);
    return request.options[static_cast<std::size_t>(option)].entityId;
}
inline bool StageStockHandoff(const PreviewRequest& request,const std::vector<int>& indices) {
    if (!preferStockCommandHandoff || stockHandoffState!=StockHandoffState::Idle || !Ready() ||
        indices.empty() || static_cast<int>(indices.size())!=request.requiredTargets ||
        StockTapRecipient(request,indices.back())<0) return false;
    std::vector<int> seen;
    for(const int index:indices) {
        if(index<0 || static_cast<std::size_t>(index)>=request.options.size() ||
           std::find(seen.begin(),seen.end(),index)!=seen.end()) return false;
        seen.push_back(index);
        const auto& option=request.options[index];
        if(option.cardGuid<=0 || (request.mode!=SelectionMode::CardDatabase && option.entityId<=0)) return false;
        CC_SELECTION_DIAG("CustomComponents/Pick3/PF3_CHOICE: effect=%d group=%d index=%d entity=%d guid=%d",
            request.effectEntityId,request.effectGroupId,index,option.entityId,option.cardGuid);
    }
    stockHandoffRequest=request; stockHandoffIndices=indices;
    stockHandoffState=StockHandoffState::Armed;
    CC_SELECTION_DIAG("CustomComponents/Pick3/STOCK_HANDOFF: STAGE effect=%d group=%d target=%d selected=%zu",
       request.effectEntityId,request.effectGroupId,StockTapRecipient(request,indices.back()),indices.size());
    return true;
}
// Automatic unique choices use the game's signal-to-command path too. Resolve
// and construct everything before staging; never call Process/Submit directly.
inline bool DispatchAutomaticTap(const PreviewRequest& request,const std::vector<int>& indices) {
    Obj* binder=Target(binderRoot);
    auto signalType=BNM::Class("PvZCards.Game","EntityTappedSignal");
    auto dataType=BNM::Class("PvZCards.Game","EntityGestureData");
    if(!binder || !signalType.IsValid() || !dataType.IsValid() ||
       !getInjectionBinder.IsValid() || !getInjectedInstance.IsValid())return false;
    Obj* signal=nullptr;Obj* data=nullptr;
    const auto prepared=BNM::TryInvoke([&] {
        Obj* injection=getInjectionBinder[binder]();
        if(injection) {
            auto binding=BNM::Class(injection).GetMethod("GetBinding",{std::string_view("key")}).cast<Obj*>();
            if(binding.IsValid() && binding[injection](signalType.GetMonoType()))
                signal=getInjectedInstance[injection](signalType.GetMonoType(),static_cast<Obj*>(nullptr));
        }
        data=dataType.CreateNewObjectParameters();
    });
    if(prepared.IsValid() || !signal || !data)return false;
    Core::ManagedRuntime::Root signalRoot(signal),dataRoot(data);
    auto dispatch=BNM::Class(signal).GetMethod("Dispatch",1).cast<void>();
    auto target=dataType.GetField("TargetEntity").cast<int>();
    auto order=dataType.GetField("OrderInTargetLocation").cast<int>();
    if(!signalRoot.Get() || !dataRoot.Get() || !dispatch.IsValid() || !target.IsValid() || !order.IsValid() || indices.empty())return false;
    const int recipient=StockTapRecipient(request,indices.back());if(recipient<0)return false;
    target[data].Set(recipient);order[data].Set(0);
    if(!StageStockHandoff(request,indices))return false;
    const auto sent=BNM::TryInvoke([&]{dispatch[signal](data);});
    if(sent.IsValid())LOG_ERR("CustomComponents/Pick3: automatic stock tap dispatch raised an exception; no replay");
    return true; // The signal was entered; replay is unsafe even if a listener threw.
}
// Stock input can still reach SelectTargetCommand underneath a manually
// polled Canvas. Suppress that command only while OUR gameplay picker owns
// input; release ownership before sending the selected targets.
// The original Execute owns Retain, service-handler registration, and normal
// command completion. Only SelectTarget() is replaced while our modal owns
// the same injected SelectTargetService; the custom picker submits one Play.
static void StockSelectTargetAction(Obj* self, const void* method) {
    Obj* owner = self && commandService.IsValid() ? commandService[self].Get() : nullptr;
    Obj* active = Target(serviceRoot);
    if (preferStockCommandHandoff && stockHandoffState==StockHandoffState::Armed) {
        const auto& h=Systems::TargetingPresentationRefreshSystem::observedTargetingIdentity;
        const bool valid = self && owner && active && owner==active &&
            Target(interceptedCommandRoot)==self &&
            Target(interceptedCommandServiceRoot)==owner &&
            h.valid && h.state==6 && h.phase==0 &&
            h.effectEntityId==stockHandoffRequest.effectEntityId &&
            h.effectGroupId==stockHandoffRequest.effectGroupId;
        if (valid) {
            CC_SELECTION_DIAG("CustomComponents/Pick3/STOCK_HANDOFF: COMMAND_MATCH cmd=%p service=%p effect=%d group=%d retained=1",
                     (void*)self,(void*)owner,h.effectEntityId,h.effectGroupId);
            // Input is no longer owned by the modal UI once the retained
            // stock command has accepted this exact effect/group.
            overlayCapturesInput = false;
            CC_SELECTION_DIAG("CustomComponents/Pick3/INPUT_HANDOFF: STOCK_ACCEPTED_CAPTURE_RELEASED effect=%d group=%d",h.effectEntityId,h.effectGroupId);
            // Begin the visual exit before Submit can synchronously invoke
            // stock success and retire the targeting holder. The visual clock
            // is independent of ACK or command release.
            if (onPick3PlayDispatch)
                onPick3PlayDispatch(h.effectEntityId,h.effectGroupId);
            BeginDeadline();
            const bool submitted=Submit(stockHandoffRequest,stockHandoffIndices,false);
            stockHandoffState=submitted ? StockHandoffState::Dispatched : StockHandoffState::Failed;
            CC_SELECTION_DIAG("CustomComponents/Pick3/STOCK_HANDOFF: COMMAND_RESULT cmd=%p dispatch=%d reachedService=%d status=%d",
                     (void*)self,int(submitted),int(lastSubmitReachedService),int(SelectionCompletion::Status()));
            // Stock Execute registered success/failure handlers. Our single
            // custom SelectTarget Play went through that service once.
            return;
        }
        LOG_ERR("CustomComponents/Pick3/STOCK_HANDOFF: COMMAND_MISMATCH cmd=%p owner=%p active=%p captured=%p holder=%d/%d requested=%d/%d; stock action preserved",
                (void*)self,(void*)owner,(void*)active,(void*)Target(interceptedCommandRoot),
                h.effectEntityId,h.effectGroupId,stockHandoffRequest.effectEntityId,stockHandoffRequest.effectGroupId);
#if CUSTOMCOMPONENTS_PF3_REPAIR_TRACE
        static unsigned actionMisses=0;
        if (++actionMisses<=48)
            PF3_REPAIR_TRACE("CustomComponents/Pick3/REPAIR175: ACTION_IDENTITY miss=%u cmd=%p cmdModel=%p retainedService=%p holderState=%d holderPhase=%d dispatch=%d",
                actionMisses,(void*)self,(void*)(self&&commandModel.IsValid()?commandModel[self].Get():nullptr),
                (void*)Target(interceptedCommandServiceRoot),h.state,h.phase,int(stockHandoffState));
#endif
        // A foreign stock command is not ours to suppress or mutate.
    }
    const bool modal = enabled && (overlayCapturesInput || TargetBridge::HasQueuedChoice());
    // The command retained at Execute is the most precise identity available.
    // A DI-resolved SelectTargetService may have been recaptured in the
    // meantime, so pointer equality with serviceRoot alone is insufficient.
    const bool retained = self && Target(interceptedCommandRoot) == self;
    const bool retainedOwner = retained && owner &&
        owner == Target(interceptedCommandServiceRoot);
    const bool serviceMatches = self && owner && active && owner == active;
    const bool ours = modal && (retainedOwner || serviceMatches);
    if (modal && (ours || !serviceMatches)) {
        CC_DIAG("CustomComponents/Pick3/HANDOFF_GUARD: ACTION cmd=%p owner=%p active=%p retained=%d retainedOwner=%d serviceMatches=%d defer=%d capture=%d queued=%d",
                static_cast<void*>(self), static_cast<void*>(owner),
                static_cast<void*>(active), retained ? 1 : 0,
                retainedOwner ? 1 : 0, serviceMatches ? 1 : 0,
                ours ? 1 : 0, overlayCapturesInput ? 1 : 0,
                TargetBridge::HasQueuedChoice() ? 1 : 0);
    }
    if (ours) {
        ++suppressedStockSelectionActions;
        CC_DIAG("CustomComponents/Pick3/COMMAND_LIFECYCLE: STOCK_ACTION_DEFERRED cmd=%p service=%p total=%u",
                static_cast<void*>(self), static_cast<void*>(owner), suppressedStockSelectionActions);
        return;
    }
    // Preserve all non-owned commands, including out-of-phase effects.
    if (originalSelectTargetAction) originalSelectTargetAction(self, method);
}
static void SelectTargetExecute(Obj* self, const void* method) {
    if (self) {
        ++stockTargetExecutes;
        lastObservedTargetCommand = reinterpret_cast<std::uintptr_t>(self);
        LogStockCommandLifecycle("EXECUTE_ENTER", self);
    }
    Obj* owner = self && commandService.IsValid() ? commandService[self].Get() : nullptr;
    Obj* active = Target(serviceRoot);
    const bool modal = enabled && (overlayCapturesInput || TargetBridge::HasQueuedChoice());
    // A remote SelectTargetService is per-command in this game's PvP path.
    // The cached serviceRoot may still point at the previous command. Only
    // rebind it when the executing stock command proves it belongs to the
    // exact staged local choice AND the same retained GameModel. Do not
    // refresh based on pointer inequality alone (that can steal a rival turn).
    const auto& stagedHolder=Systems::TargetingPresentationRefreshSystem::observedTargetingIdentity;
    const bool exactStage=preferStockCommandHandoff &&
        stockHandoffState==StockHandoffState::Armed && self && owner &&
        stagedHolder.valid && stagedHolder.state==6 && stagedHolder.phase==0 &&
        stagedHolder.effectEntityId==stockHandoffRequest.effectEntityId &&
        stagedHolder.effectGroupId==stockHandoffRequest.effectGroupId;
    Obj* executingModel=(self && commandModel.IsValid()) ? commandModel[self].Get() : nullptr;
    Obj* retainedModel=Target(modelRoot);
    const bool modelMatches=executingModel && retainedModel && executingModel==retainedModel;
    const bool ownerIsKnownService=owner && ((localClass.IsValid() && BNM::IsA(owner,localClass)) ||
        (remoteClass.IsValid() && BNM::IsA(owner,remoteClass)));
    const bool isRemoteCommand=remoteClass.IsValid() && owner && BNM::IsA(owner,remoteClass);
    const bool localChoiceOwner=!isRemoteCommand ||
        (stockHandoffRequest.ownerPlayerId>=0 &&
         stockHandoffRequest.ownerPlayerId==CapturedLocalPlayerId() &&
         stockHandoffRequest.ownerPlayerId==TargetBridge::ActiveOwnerPlayerId());
    if (exactStage && modelMatches && ownerIsKnownService && localChoiceOwner && owner!=active) {
        PF3_REPAIR_TRACE("CustomComponents/Pick3/REPAIR176: COMMAND_SERVICE_REBIND cmd=%p model=%p old=%p new=%p effect=%d group=%d owner=%d local=%d",
            (void*)self,(void*)executingModel,(void*)active,(void*)owner,
            stockHandoffRequest.effectEntityId,stockHandoffRequest.effectGroupId,
            stockHandoffRequest.ownerPlayerId,CapturedLocalPlayerId());
        CaptureService(owner,"SelectTargetExecute.StagedCommand");
        active=Target(serviceRoot);
    } else if (exactStage && owner!=active) {
        PF3_REPAIR_TRACE("CustomComponents/Pick3/REPAIR176: COMMAND_SERVICE_REBIND_REJECT cmd=%p modelMatch=%d knownService=%d localOwner=%d owner=%p active=%p effect=%d group=%d",
            (void*)self,int(modelMatches),int(ownerIsKnownService),int(localChoiceOwner),
            (void*)owner,(void*)active,stockHandoffRequest.effectEntityId,stockHandoffRequest.effectGroupId);
    }
    // A Choose tap may arrive after the popup has released its board-input
    // capture, so `modal` alone is not a valid command-ownership test.
    // Bind the exact staged effect/group to the stock command BEFORE its
    // original Execute runs (and calls SelectTarget synchronously).
    const auto& holder=Systems::TargetingPresentationRefreshSystem::observedTargetingIdentity;
    const bool stagedHere=preferStockCommandHandoff &&
        stockHandoffState==StockHandoffState::Armed && self && owner && active &&
        owner==active && holder.valid && holder.state==6 && holder.phase==0 &&
        holder.effectEntityId==stockHandoffRequest.effectEntityId &&
        holder.effectGroupId==stockHandoffRequest.effectGroupId;
    const bool ours = (modal || stagedHere) && self && owner && active && owner == active;
    // The GameModel of a SelectTargetCommand is authoritative command context,
    // but is NOT an EntityModel. Record its identity; never reinterpret it as
    // the finder snapshot or use it to bypass the exact holder effect/group.
#if CUSTOMCOMPONENTS_PF3_REPAIR_TRACE
    if (self && stockHandoffState==StockHandoffState::Armed) {
        static unsigned ownershipChecks=0;
        if (++ownershipChecks<=96) {
            Obj* commandGameModel=commandModel.IsValid()?commandModel[self].Get():nullptr;
            PF3_REPAIR_TRACE("CustomComponents/Pick3/REPAIR175: COMMAND_CONTEXT seq=%u cmd=%p cmdModel=%p retainedModel=%p cmdService=%p activeService=%p captured=%p holder=%d/%d state=%d phase=%d staged=%d/%d stagedHere=%d modal=%d",
                ownershipChecks,(void*)self,(void*)commandGameModel,(void*)Target(modelRoot),
                (void*)owner,(void*)active,(void*)Target(interceptedCommandRoot),
                holder.effectEntityId,holder.effectGroupId,holder.state,holder.phase,
                stockHandoffRequest.effectEntityId,stockHandoffRequest.effectGroupId,
                int(stagedHere),int(modal));
        }
    }
#endif
    if (stagedHere)
        CC_SELECTION_DIAG("CustomComponents/Pick3/STOCK_HANDOFF: EXECUTE_STAGE_CAPTURE cmd=%p service=%p effect=%d group=%d modal=%d",
                 (void*)self,(void*)owner,holder.effectEntityId,holder.effectGroupId,int(modal));
    if (enabled && self && !modal && !stagedHere) {
        if (owner && owner != active) CaptureService(owner, "SelectTargetExecute");
        if (commandModel.IsValid()) {
            Obj* model = commandModel[self].Get();
            if (model && model != Target(modelRoot)) CaptureRoot(modelRoot, model);
        }
    }
    if (ours) {
        // Execute must run once to attach the real handlers and retain the
        // command. Do not synthesize a release or a success callback ourselves.
        if (Target(interceptedCommandRoot) == self) {
            CC_DIAG("CustomComponents/Pick3/COMMAND_LIFECYCLE: REPEATED_EXECUTE_SKIPPED cmd=%p", static_cast<void*>(self));
            return;
        }
        CaptureRoot(interceptedCommandRoot, self);
        CaptureRoot(interceptedCommandServiceRoot, owner);
        if (targetCommandStateSignal.IsValid() && gameStateUpdatedSignalClass.IsValid()) {
            Obj* signal = targetCommandStateSignal[self].Get();
            if (signal && BNM::IsA(signal, gameStateUpdatedSignalClass))
                CaptureRoot(interceptedTargetSignalRoot, signal);
        }
        CC_DIAG("CustomComponents/Pick3/COMMAND_LIFECYCLE: STOCK_EXECUTE_WITH_ACTION_DEFERRED cmd=%p service=%p",
                static_cast<void*>(self), static_cast<void*>(owner));
    } else if (modal && owner && active && owner != active) {
        ++commandOwnershipMismatches;
        CC_DIAG("CustomComponents/Pick3/COMMAND_LIFECYCLE: FOREIGN_STOCK_EXECUTE cmd=%p", static_cast<void*>(self));
    }
    TracePostSubmitInput("SelectTargetCommand.Execute", false);
    if (originalExecute) originalExecute(self, method);
    LogStockCommandLifecycle("EXECUTE_RETURN", self);
}
// Board taps also propagate outside Unity's UGUI EventSystem. The stock
// HandleBoardTappedCommand.Execute() is 0x54 bytes on this APK (not an unsafe
// generated setter). Only silence it while our modal popup owns input.
static void BoardTapExecute(Obj* self, const void* method) {
    const bool modal=enabled && (overlayCapturesInput || TargetBridge::HasQueuedChoice());
    const bool stockChoice=modal && preferStockCommandHandoff && isStockChooseBoardTap &&
                           isStockChooseBoardTap(self);
    // Only the active state-6 PF3 selection may forward an input tap.
    if (modal && !stockChoice) {
        TracePostSubmitInput("HandleBoardTappedCommand.Execute", true);
        if (++blockedBoardTaps == 1)
            CC_DIAG("CustomComponents/Pick3/INPUT: BOARD_TAP_SUPPRESSED");
        return;
    }
    TracePostSubmitInput("HandleBoardTappedCommand.Execute", false);
    if (stockChoice) CC_DIAG("CustomComponents/Pick3/STOCK_HANDOFF: BOARD_TAP_CHOOSE_FORWARDED");
    if (originalBoardTapExecute) originalBoardTapExecute(self,method);
}
inline void SetPopupInputCapture(bool capture) { overlayCapturesInput = capture && enabled; }
static void TimerPush(Obj* self, Obj* timer, const void* method) {
    if (originalPush) originalPush(self, timer, method);
    if (enabled && self && timer) CaptureRoot(stackRoot, self);
}
}
