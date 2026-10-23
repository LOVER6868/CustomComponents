#pragma once
#include "pick3/Pick3ReleaseSafety.hpp"
#include "diagnostics/DiagnosticLogging.hpp"
// Experimental stock selection submission for an explicitly marked PickFromThree effect.
// Captures GameModel from PlayCardCommand.Execute and SelectTargetCommand.Execute.
// Captures SelectTargetService from the existing Step 3 factory GetService tracer
// or the already-injected field on SelectTargetCommand.Execute.
// NEVER hook generated property setters (8-byte methods adjacent to other functions).
// No direct effect execution, no local-only PvP service, no modified target lists.
#include <cstdint>
#include <vector>
#include <random>
#include <algorithm>
#include <BNM/Class.hpp>
#include <BNM/Method.hpp>
#include <BNM/Field.hpp>
#include <BNM/BasicMonoStructures.hpp>
#include <BNM/Exceptions.hpp>
#include "core/SceneViewContext.hpp"
#include "presentation/TargetingPresentationRefreshSystem.hpp"
#include "pick3/PickFromThreeTargetBridge.hpp"
#include "pick3/PickedFromThreeSelection.hpp"
#include "gameplay/SelectionTransport.hpp"
#include "gameplay/SelectionCompletion.hpp"
#include "log.h"

namespace CustomComponents::Pick3::Submission {
using Obj = BNM::IL2CPP::Il2CppObject;
using IntList = BNM::Structures::Mono::List<int>;
using Push = void(*)(Obj*, Obj*, const void*);
using Execute = void(*)(Obj*, const void*);
inline Execute originalPlayCardExecute = nullptr;
inline Execute originalBoardTapExecute = nullptr;
// Read-only stock command lifecycle probes; these NEVER synthesize completion.
inline Execute originalStockTargetSuccess = nullptr;
inline void (*originalStockTargetFailure)(Obj*, Obj*, const void*) = nullptr;
inline Execute originalHeroesCommandRelease = nullptr;
inline unsigned int lifecycleTraceEvents = 0;
inline unsigned int stockTargetExecutes = 0, stockTargetSuccesses = 0;
inline unsigned int stockTargetFailures = 0, stockTargetReleases = 0;
inline std::uintptr_t lastObservedTargetCommand = 0;
inline BNM::Field<Obj*> targetCommandStateSignal;
inline BNM::Field<Obj*> retainedCommandsTracker;

inline unsigned int blockedBoardTaps = 0;
inline unsigned int blockedPlayCommands = 0;
inline unsigned int dependencyPollFrame = 0;
inline bool resolveFromInjection = true;
// Step 4.10: restore the one stock GameStateUpdatedSignal notification after
// a local manual target transaction. Capture the already-injected signal from
// PlayCardCommand.Execute, avoiding the Step 4.6 arbitrary DI lookup.
inline bool syncLocalCompletion = true;
inline BNM::Class gameStateUpdatedSignalClass;
inline BNM::Field<Obj*> playCardStateUpdatedSignal;
inline BNM::Field<Obj*> serviceOnSuccess;
inline BNM::Method<void> dispatchGameStateUpdated;
inline std::uint32_t updatedSignalRoot = 0;
// Capture a genuine SelectTargetCommand signal only while our modal intercepts
// that command. This is a presentation fallback, NOT stock command completion.
inline std::uint32_t interceptedTargetSignalRoot = 0;
inline unsigned int interceptedStockCommands = 0;
// A rooted identity for the suppressed stock command, scoped to one popup.
// Never invoke Execute/Release on it: suppressing Execute means Retain may
// not have happened. This lets later traces distinguish matching service
// ownership from an unrelated target selection.
inline std::uint32_t interceptedCommandRoot = 0;
inline std::uint32_t interceptedCommandServiceRoot = 0;
inline unsigned int commandOwnershipMismatches = 0;
inline unsigned int signalDispatchCount = 0;
inline Push originalPush = nullptr;
inline Execute originalExecute = nullptr;
inline bool overlayCapturesInput = false;
inline unsigned int postSubmitInputBudget = 0;
inline int postSubmitEffect = -1, postSubmitGroup = -1;
inline unsigned int blockedStockClicks = 0;
inline BNM::Class localClass, remoteClass, playClass, commandClass, stackClass, timerClass;
inline BNM::Class serviceFactoryClass, playCardCommandClass;
inline BNM::Class gameInputHolderClass, gameCommandBinderClass;
inline BNM::Class selectServiceInterfaceClass;
inline BNM::Field<Obj*> stateCommandBinder, stateGameModel;
// Passive ownership check: inspect the already-injected model in the existing
// GameInputStateHolder observer. No EndSubTurn or tiny setter detours.
inline BNM::Field<Obj*> stateVisualTurnPhase;
inline BNM::Field<int> visualSubTurn, opponentPlayerField;
inline bool observedLocalTargetSubTurn = false;
inline bool observedVisualPhase = false;
inline bool pvpOwnerGateReady = false;
inline BNM::Method<Obj*> getInjectionBinder, getInjectedInstance;
inline BNM::Field<int> localPlayerField, playType, playPlayer, playCard, playOrder;
inline BNM::Field<IntList*> playTargets;
inline BNM::Field<Obj*> commandModel, commandService, playCardCommandModel;

struct TimerParameters { bool isForLocalPlayer; bool isForMulligans; };
inline BNM::Field<bool> timerRunning, timerPaused;
inline BNM::Field<TimerParameters> timerParameters;
inline bool enabled = false;
inline bool allowPvp = false;
inline bool allowPve = false;
inline int deadlineThresholdMs = 600;
inline std::uint32_t deadlineTimerRoot = 0;
inline std::uint32_t serviceRoot = 0, modelRoot = 0, stackRoot = 0, binderRoot = 0;
inline int lastEffect = -1, lastGroup = -1;
inline std::vector<std::uint32_t> submittedPlayRoots;
inline std::mt19937 rng{std::random_device{}()};

inline Obj* Target(std::uint32_t root) {
    return root && Core::SceneViewContext::getGCHandle ? Core::SceneViewContext::getGCHandle(root) : nullptr;
}
inline void Drop(std::uint32_t& root) {
    if (root && Core::SceneViewContext::freeGCHandle) Core::SceneViewContext::freeGCHandle(root);
    root = 0;
}
inline void CaptureRoot(std::uint32_t& root, Obj* obj) {
    if (!obj || !Core::SceneViewContext::newGCHandle) return;
    if (Target(root) == obj) return;
    Drop(root);
    root = Core::SceneViewContext::newGCHandle(obj, false);
}
inline void CaptureService(Obj* service, const char* source) {
    if (!enabled || !service || (!BNM::IsA(service, localClass) && !BNM::IsA(service, remoteClass))) return;
    CaptureRoot(serviceRoot, service);
    CC_DIAG("CustomComponents/Pick3/SUBMIT: SERVICE_CAPTURE source=%s type=%s remote=%d rooted=%d",
             source, service->klass && service->klass->name ? service->klass->name : "?",
             BNM::IsA(service, remoteClass) ? 1 : 0, serviceRoot ? 1 : 0);
}
// IMPORTANT: Do NOT hook TouchScript.TouchManagerInstance.Update.
// In this build it is a four-byte method at RVA 0x1AA0218, immediately
// followed by another method at 0x1AA021C. Inline hooking it can overwrite
// adjacent code, causing navigation-time SIGSEGV even with no popup open.
// Input safeguards below are limited to UGUI and stock game commands until
// an alternative TouchScript suppression method is verified.
// Avoid relying on SelectTargetCommand's short generated field setters.
// The regular per-frame game input holder already knows BOTH the active
// GameModel and the injected CommandBinder; snapshot them only after the
// game's own Update has returned. The real resolver is called from the UI tick
// rather than during the holder's state transition.
static void GameInputHolderUpdated(Obj* self) {
    if (!enabled || !self) return;
    if (stateVisualTurnPhase.IsValid() && visualSubTurn.IsValid()) {
        Obj* phase = stateVisualTurnPhase[self].Get();
        observedVisualPhase = phase != nullptr;
        observedLocalTargetSubTurn = phase && visualSubTurn[phase].Get() == 1;
    }
    if (stateGameModel.IsValid()) {
        Obj* model = stateGameModel[self].Get();
        if (model && Target(modelRoot) != model &&
                !(overlayCapturesInput || TargetBridge::HasQueuedChoice())) {
            CaptureRoot(modelRoot, model);
            CC_DIAG("CustomComponents/Pick3/SUBMIT: MODEL_CAPTURE source=GameInputStateHolder rooted=%d", modelRoot ? 1 : 0);
        }
    }
    if (stateCommandBinder.IsValid()) {
        Obj* binder = stateCommandBinder[self].Get();
        if (binder && Target(binderRoot) != binder) {
            CaptureRoot(binderRoot, binder);
            CC_DIAG("CustomComponents/Pick3/SUBMIT: BINDER_CAPTURE source=GameInputStateHolder rooted=%d", binderRoot ? 1 : 0);
        }
    }
}
// The matching game-context binder has already been injected into the game's
// CommandBinder. Query its existing SelectTargetService binding (not a new
// service instance we construct). Limit attempts and catch managed failures.
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
    LOG_INFO("CustomComponents/Pick3/INPUT_HANDOFF: command=%s blocked=%d remote=%d "
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
    LOG_INFO("CustomComponents/Pick3/STOCK_COMMAND: step=%s cmd=%p sameAsExecute=%d "
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
    if (originalPlayCardExecute) originalPlayCardExecute(self, method);
    if (trace)
        CC_DIAG("CustomComponents/Pick3/SUBMIT: PLAYCARD_EXEC_RETURN");
}
// Stock input can still reach SelectTargetCommand underneath a manually
// polled Canvas. Suppress that command only while OUR gameplay picker owns
// input; release ownership before sending the selected targets.
static void SelectTargetExecute(Obj* self, const void* method) {
    if (self) {
        ++stockTargetExecutes;
        lastObservedTargetCommand = reinterpret_cast<std::uintptr_t>(self);
        LogStockCommandLifecycle("EXECUTE_ENTER", self);
    }
    // Dependency injection has already populated these fields at Execute time.
    // Reading the fields is safe; intercepting adjacent 8-byte setters was not.
    if (enabled && self) {
        if (commandService.IsValid()) {
            Obj* service = commandService[self].Get();
            if (service && Target(serviceRoot) != service &&
                !(overlayCapturesInput || TargetBridge::HasQueuedChoice()))
                CaptureService(service, "SelectTargetExecute");
        }
        if (commandModel.IsValid()) {
            Obj* model = commandModel[self].Get();
            if (model && Target(modelRoot) != model) {
                CaptureRoot(modelRoot, model);
                CC_DIAG("CustomComponents/Pick3/SUBMIT: MODEL_CAPTURE source=SelectTargetExecute rooted=%d", modelRoot ? 1 : 0);
            }
        }
    }
    if (enabled && (overlayCapturesInput || TargetBridge::HasQueuedChoice())) {
        ++interceptedStockCommands;
        // The original Execute is NOT called here. Hence this command has not
        // necessarily Retained itself and may not be passed to Release().
        // Capture ownership only when the stock command is bound to the
        // same game-owned service that our transport will use. When it isn't,
        // do not borrow its signal or treat it as our completion command.
        Obj* ownerService = self && commandService.IsValid() ? commandService[self].Get() : nullptr;
        Obj* activeService = Target(serviceRoot);
        const bool matchedService = ownerService && activeService && ownerService == activeService;
        if (matchedService) {
            CaptureRoot(interceptedCommandRoot, self);
            CaptureRoot(interceptedCommandServiceRoot, ownerService);
        } else {
            ++commandOwnershipMismatches;
            Drop(interceptedCommandRoot);
            Drop(interceptedCommandServiceRoot);
            Drop(interceptedTargetSignalRoot);
        }
        CC_DIAG("CustomComponents/Pick3/COMMAND_OWNER: blocked cmd=%p service=%p active=%p matched=%d mismatchCount=%u effect=%d group=%d",
                static_cast<void*>(self), static_cast<void*>(ownerService), static_cast<void*>(activeService),
                matchedService ? 1 : 0, commandOwnershipMismatches, lastEffect, lastGroup);
        // Capture only its injected signal for the existing PvE UI refresh.
        if (matchedService && self && targetCommandStateSignal.IsValid() && gameStateUpdatedSignalClass.IsValid()) {
            Obj* stockSignal = targetCommandStateSignal[self].Get();
            if (stockSignal && BNM::IsA(stockSignal, gameStateUpdatedSignalClass) &&
                Target(interceptedTargetSignalRoot) != stockSignal)
                CaptureRoot(interceptedTargetSignalRoot, stockSignal);
            if (interceptedStockCommands <= 32)
                CC_DIAG("CustomComponents/Pick3/COMMAND_HANDOFF: suppressed=%u cmd=%p signal=%p captured=%d remote=%d",
                        interceptedStockCommands, static_cast<void*>(self), static_cast<void*>(stockSignal),
                        interceptedTargetSignalRoot ? 1 : 0,
                        Target(serviceRoot) && remoteClass.IsValid() && BNM::IsA(Target(serviceRoot), remoteClass) ? 1 : 0);
        }
        TracePostSubmitInput("SelectTargetCommand.Execute", true);
        if (++blockedStockClicks <= 24)
            CC_DIAG("CustomComponents/Pick3/SUBMIT: STOCK_INPUT_BLOCKED count=%u service=%d model=%d",
                     blockedStockClicks, Target(serviceRoot) ? 1 : 0,
                     Target(modelRoot) ? 1 : 0);
        return;
    }
    TracePostSubmitInput("SelectTargetCommand.Execute", false);
    if (originalExecute) originalExecute(self, method);
    LogStockCommandLifecycle("EXECUTE_RETURN", self);
}
// Board taps also propagate outside Unity's UGUI EventSystem. The stock
// HandleBoardTappedCommand.Execute() is 0x54 bytes on this APK (not an unsafe
// generated setter). Only silence it while our modal popup owns input.
static void BoardTapExecute(Obj* self, const void* method) {
    if (enabled && (overlayCapturesInput || TargetBridge::HasQueuedChoice())) {
        TracePostSubmitInput("HandleBoardTappedCommand.Execute", true);
        if (++blockedBoardTaps == 1)
            CC_DIAG("CustomComponents/Pick3/INPUT: BOARD_TAP_SUPPRESSED");
        return;
    }
    TracePostSubmitInput("HandleBoardTappedCommand.Execute", false);
    if (originalBoardTapExecute) originalBoardTapExecute(self,method);
}
inline void SetPopupInputCapture(bool capture) { overlayCapturesInput = capture && enabled; }
static void TimerPush(Obj* self, Obj* timer, const void* method) {
    if (originalPush) originalPush(self, timer, method);
    if (enabled && self && timer) CaptureRoot(stackRoot, self);
}
inline void ResetMatch() {
    SelectionCompletion::Detach();
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
    Drop(updatedSignalRoot); Drop(deadlineTimerRoot);
    signalDispatchCount = 0;
    TargetBridge::lastPlayedSourceEntity=-1;
    TargetBridge::lastPlayedCardGuid=-1;
    CustomComponents::Pick3::Picked::ResetMatch();
    lastEffect = lastGroup = -1;
    // Remote completion may outlive the scene. Keep the submitted Play rooted
    // until process exit instead of risking asynchronous access to freed data.
    CC_DIAG("CustomComponents/Pick3/SUBMIT: MATCH_RESET services/timer cleared retainedPlays=%zu", submittedPlayRoots.size());
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
inline long long TimerRemaining() {
    Obj* stack = Target(stackRoot);
    if (!stack) return -1;
    Obj* timer = nullptr;
    auto peek = stackClass.GetMethod("Peek", 0).cast<Obj*>();
    if (!peek.IsValid()) return -1;
    auto pe = BNM::TryInvoke([&]{ timer = peek[stack](); });
    if (pe.IsValid() || !timer || !BNM::IsA(timer, timerClass)) return -1;
    if (!timerParameters.IsValid()) return -1;
    const auto parameters = timerParameters[timer].Get();
    if (!parameters.isForLocalPlayer || parameters.isForMulligans) return -1;
    if (!Target(deadlineTimerRoot)) CaptureRoot(deadlineTimerRoot, timer);
    if (timerPaused.IsValid() && timerPaused[timer].Get()) return -1;
    if (timerRunning.IsValid() && !timerRunning[timer].Get()) return -1;
    auto getRemaining = timerClass.GetMethod("GetTimeRemaining", 0).cast<std::int64_t>();
    if (!getRemaining.IsValid()) return -1;
    std::int64_t remain = -1;
    auto ex = BNM::TryInvoke([&]{ remain = getRemaining[timer](); });
    return ex.IsValid() ? -1 : remain;
}
// Retain the actual local timer: Peek() can change when the timer completes.
inline bool DeadlineExpired() {
    if (!Remote()) return false;
    TimerRemaining(); // Capture a local, non-mulligan timer when available.
    Obj* timer = Target(deadlineTimerRoot);
    if (!timer || !BNM::IsA(timer, timerClass) || timerPaused[timer].Get()) return false;
    auto remaining = timerClass.GetMethod("GetTimeRemaining", 0).cast<std::int64_t>();
    std::int64_t value = -1;
    const auto ex = BNM::TryInvoke([&] { value = remaining[timer](); });
    return !ex.IsValid() && value <= 0;
}
inline void BeginDeadline() { Drop(deadlineTimerRoot); TimerRemaining(); }
inline bool ShouldAutoPick() {
    if (!Remote() || !allowPvp) return false;
    const long long remaining = TimerRemaining();
    return remaining >= 0 && remaining <= deadlineThresholdMs;
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
inline bool lastSubmitReachedService = false;
inline bool CanRetryLastSubmission() { return !lastSubmitReachedService; }
inline bool Submit(const PreviewRequest& request, const std::vector<int>& indices, bool timeout) {
    lastSubmitReachedService = false;
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
    if (databaseMode && !ReleaseSafety::CanUseDatabaseSelection(Remote(), request.requiredTargets)) {
        LOG_ERR("CustomComponents/Pick3/DB5: UNSUPPORTED_TRANSPORT effect=%d remote=%d required=%d; "
                "Play.Targets carries entity IDs, not card GUIDs; no network CardDatabase protocol exists",
                request.effectEntityId, Remote() ? 1 : 0, request.requiredTargets);
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
        // game's valid-target snapshot. In particular Unearth-like effects
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
        LOG_INFO("CustomComponents/Pick3/TARGET_TRACE: effect=%d group=%d selectionMode=%d "
                 "option=%d entityId=%d cardGuid=%d dbProxy=%d",
                 request.effectEntityId, request.effectGroupId, static_cast<int>(request.mode),
                 index, entity, request.options[static_cast<std::size_t>(index)].cardGuid,
                 databaseMode ? 1 : 0);
        entities.push_back(entity);
        selectedCards.push_back({databaseMode ? kNoEntity : entity,
                                 request.options[static_cast<std::size_t>(index)].cardGuid});
    }
    Obj* svc = Target(serviceRoot);
    Obj* model = Target(modelRoot);
    if (!svc || !model) {
        LOG_ERR("CustomComponents/Pick3/SUBMIT: SERVICE_UNAVAILABLE effect=%d svc=%d model=%d",
                request.effectEntityId, svc ? 1 : 0, model ? 1 : 0);
        return false;
    }
    if (submittedPlayRoots.size() >= 128) {
        LOG_ERR("CustomComponents/Pick3/SUBMIT: RETAINED_PLAY_LIMIT count=%zu effect=%d (selection not submitted)",
                submittedPlayRoots.size(), request.effectEntityId);
        return false;
    }
    const int playerId = localPlayerField[model].Get();
    // A reusable game-owned Play DTO builder is shared by PvE and PvP.
    // The selection rules, Picked query, local completion and retry semantics
    // remain in this PickFromThree adapter exactly as in the working baseline.
    SelectionTransport::TargetSelection action;
    action.playerId = playerId;
    action.entityIds = entities;
    SelectionTransport::PlayFields playApi{&playClass, &playTargets, &playType,
                                          &playCard, &playPlayer, &playOrder};
    SelectionTransport::Prepared prepared;
    if (!SelectionTransport::Prepare(playApi, action, prepared)) {
        LOG_ERR("CustomComponents/Pick3/SUBMIT: PREPARE_FAILED effect=%d", request.effectEntityId);
        return false;
    }
    Obj* play = prepared.object;
    std::uint32_t playRoot = prepared.root;
    if (!SelectionTransport::HasProcessMethod(svc)) { Drop(playRoot); return false; }
    // Store before stock Process(): downstream DrawCardFromSubset can evaluate
    // its GUID-domain query synchronously in this call. Failed submissions
    // remove the staged result. Never publish a global, unscoped last GUID.
    // Remote prediction may synchronously evaluate the same scoped custom query.
    // This local cache is NOT network replication; the other client/server must
    // still derive identical results from the stock target action.
    const bool publishForLocalProcess = Local() || (Remote() && allowPvp);
    const bool pickedPublished = publishForLocalProcess && CustomComponents::Pick3::Picked::Store(
        request.effectEntityId, request.effectGroupId, selectedCards);
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
    // Lock before entering game-owned code: it may synchronously re-enter UI.
    lastEffect = request.effectEntityId; lastGroup = request.effectGroupId;
    submittedPlayRoots.push_back(playRoot);
    CC_DIAG("CustomComponents/Pick3/SUBMIT: SEND mode=%s reason=%s effect=%d group=%d count=%zu targets=%d,%d,%d player=%d type=6",
        Remote() ? "PVP_REMOTE" : "PVE_LOCAL", timeout ? "TIMER" : "CLICK",
        request.effectEntityId, request.effectGroupId, entities.size(),
        entities.size() > 0 ? entities[0] : -1,
        entities.size() > 1 ? entities[1] : -1,
        entities.size() > 2 ? entities[2] : -1, playerId);
    // Observe completion before entering Process: local services may finish
    // synchronously; remote services can finish on a later frame.
    if (!SelectionCompletion::Begin(svc)) {
        submittedPlayRoots.pop_back(); Drop(playRoot);
        if (pickedPublished) Picked::Erase(request.effectEntityId, request.effectGroupId);
        lastEffect = lastGroup = -1;
        LOG_ERR("CustomComponents/Pick3/SUBMIT: CALLBACK_SETUP_FAILED (not dispatched)");
        return false;
    }
    lastSubmitReachedService = true;
    const auto dispatch = SelectionTransport::Dispatch(svc, play);
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
inline void FinishTransaction(const PreviewRequest& request, bool success) {
    // Preserve the working PvE fallback. The remote service must never
    // synthesize this signal: its authoritative network update owns completion.
    // Keep this policy in one place rather than distributing mode checks in UI.
    const bool notify = success && Local() &&
                        !SelectionCompletion::hadSuccessHandler && syncLocalCompletion;
    const int reason = SelectionCompletion::completion.Reason();
    const int outcome = static_cast<int>(SelectionCompletion::Status());
    const bool uncertain = SelectionCompletion::Status() == SelectionCompletion::Result::Uncertain ||
                           SelectionCompletion::Status() == SelectionCompletion::Result::Pending;
    LOG_INFO("CustomComponents/Pick3/HANDOFF: BEFORE_CLEANUP effect=%d group=%d remote=%d "
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
    LOG_INFO("CustomComponents/Pick3/RETIRE_BOUNDARY: BEFORE effect=%d group=%d remote=%d status=%d queued=%d capture=%d targetSubTurn=%d signalReady=%d commandMatch=%d",
             request.effectEntityId, request.effectGroupId, Remote() ? 1 : 0,
             outcome, TargetBridge::HasQueuedChoice() ? 1 : 0,
             overlayCapturesInput ? 1 : 0, observedLocalTargetSubTurn ? 1 : 0,
             (Target(interceptedTargetSignalRoot) || Target(updatedSignalRoot)) ? 1 : 0,
             commandOwnerMatches ? 1 : 0);
    SelectionCompletion::Detach();
    // PvE completes synchronously and retains its proven re-arm behavior.
    // PvP's OnSuccess acknowledges dispatch before the local replicated
    // ChooseTargetBlockingEffect necessarily disappears. Re-arming the same
    // effect/group here allows FindValidHook to publish a second modal request,
    // which can intercept unrelated card plays and subsequent Unearths.
    // Keep the offer consumed in PvP; a NEW effect/group can still be offered.
    const bool rearm = Local() && !uncertain;
    TargetBridge::CompleteLocalSelection(request.effectEntityId, request.effectGroupId, rearm);
    if (Remote() && success)
        CC_DIAG("CustomComponents/Pick3/PVP_LIFECYCLE: ACK_CONSUMED effect=%d group=%d rearm=0 awaitingAuthoritativeRetirement=1",
                request.effectEntityId, request.effectGroupId);
    if (!success) Picked::Erase(request.effectEntityId, request.effectGroupId);
    lastEffect = lastGroup = -1;
    overlayCapturesInput = false;
    postSubmitEffect = request.effectEntityId;
    postSubmitGroup = request.effectGroupId;
    postSubmitInputBudget = 24;
    LOG_INFO("CustomComponents/Pick3/HANDOFF: AFTER_CLEANUP effect=%d group=%d remote=%d "
             "queued=%d capture=%d rearm=%d notify=%d",
             request.effectEntityId, request.effectGroupId, Remote() ? 1 : 0,
             TargetBridge::HasQueuedChoice() ? 1 : 0, overlayCapturesInput ? 1 : 0,
             rearm ? 1 : 0, notify ? 1 : 0);
    if (notify) {
        // Prefer the signal injected into the intercepted target command over
        // one captured from an unrelated PlayCardCommand. Dispatch exactly
        // once; never dispatch manually on the network (PvP) path.
        Obj* signal = commandOwnerMatches ? Target(interceptedTargetSignalRoot) : nullptr;
        if (!signal) signal = Target(updatedSignalRoot);
        CC_DIAG("CustomComponents/Pick3/COMMAND_HANDOFF: PVE_REFRESH targetSignal=%d playSignal=%d source=%s",
                Target(interceptedTargetSignalRoot) ? 1 : 0,
                Target(updatedSignalRoot) ? 1 : 0,
                Target(interceptedTargetSignalRoot) ? "target-command" : "play-card");
        if (signal && dispatchGameStateUpdated.IsValid()) {
            const auto ex = BNM::TryInvoke([&] { dispatchGameStateUpdated[signal](); });
            if (ex.IsValid()) LOG_ERR("CustomComponents/Pick3/SUBMIT: NOTIFY_EXCEPTION %s", ex.Message().c_str());
        } else LOG_ERR("CustomComponents/Pick3/SUBMIT: NOTIFY_UNAVAILABLE");
    }
    // Rooted only for this modal transaction; prevent a future selection from
    // using a previous command's injected signal after re-entry or timeout.
    Drop(interceptedTargetSignalRoot);
    Drop(interceptedCommandRoot);
    Drop(interceptedCommandServiceRoot);
    LOG_INFO("CustomComponents/Pick3/RETIRE_BOUNDARY: AFTER effect=%d group=%d remote=%d queued=%d capture=%d targetSubTurn=%d rearm=%d notify=%d",
             request.effectEntityId, request.effectGroupId, Remote() ? 1 : 0,
             TargetBridge::HasQueuedChoice() ? 1 : 0, overlayCapturesInput ? 1 : 0,
             observedLocalTargetSubTurn ? 1 : 0, rearm ? 1 : 0, notify ? 1 : 0);
    LOG_INFO("CustomComponents/Pick3/SUBMIT: RESULT effect=%d group=%d success=%d reason=%d outcome=%d rearmed=%d",
             request.effectEntityId, request.effectGroupId, success ? 1 : 0, reason, outcome, rearm ? 1 : 0);
}

inline bool Initialize(bool request, bool pve, bool pvp, int thresholdMs,
                       bool useInjectionResolver = true, bool syncPveCompletion = true) {
    if (!request) return true;
    if (!Core::SceneViewContext::newGCHandle || !Core::SceneViewContext::getGCHandle || !Core::SceneViewContext::freeGCHandle) {
        LOG_ERR("CustomComponents/Pick3/SUBMIT: SETUP missing GC handle API");
        return false;
    }
    localClass = BNM::Class("PvZCards.Core.Services", "LocalSelectTargetService");
    remoteClass = BNM::Class("PvZCards.Multiplayer.Networking", "RemoteSelectTargetService");
    commandClass = BNM::Class("PvZCards.Game", "SelectTargetCommand");
    serviceFactoryClass = BNM::Class("PvZCards.Core.Utilities", "ServiceFactory");
    playCardCommandClass = BNM::Class("PvZCards.Game", "PlayCardCommand");
    gameStateUpdatedSignalClass = BNM::Class("PvZCards.Game", "GameStateUpdatedSignal");
    playCardStateUpdatedSignal = playCardCommandClass.GetField("<GameStateUpdatedSignal>k__BackingField");
    // Use the base-class Dispatch(), exactly like ARM64 bl 0x307C174 in
    // SelectTargetCommand.HandleServiceSuccess (0x1FBE424).
    dispatchGameStateUpdated = BNM::Class("strange.extensions.signal.impl", "Signal")
        .GetMethod("Dispatch", 0).cast<void>();
    serviceOnSuccess = BNM::Class("PvZCards.Engine", "BaseService")
        .GetField("<OnSuccess>k__BackingField").cast<Obj*>();
    stackClass = BNM::Class("PvZCards.Game", "TurnTimerStack");
    timerClass = BNM::Class("PvZCards.Game", "TurnTimerComponent");
    gameInputHolderClass = BNM::Class("PvZCards.Game", "GameInputStateHolder");
    gameCommandBinderClass = BNM::Class("strange.extensions.command.impl", "CommandBinder");
    selectServiceInterfaceClass = BNM::Class("PvZCards.Core.Services", "SelectTargetService");
    // Depending on Unity's metadata, the interface can be stored in the Core
    // namespace while its concrete instances live in Multiplayer.Networking.
    stateGameModel = gameInputHolderClass.GetField("<GameModel>k__BackingField");
    stateCommandBinder = gameInputHolderClass.GetField("<CommandBinder>k__BackingField");
    stateVisualTurnPhase = gameInputHolderClass.GetField("<VisualTurnPhaseModel>k__BackingField");
    visualSubTurn = BNM::Class("PvZCards.Game", "VisualTurnPhaseModel").GetField("SubTurn").cast<int>();
    getInjectionBinder = gameCommandBinderClass.GetMethod("get_injectionBinder", 0).cast<Obj*>();
    auto injectionBinder = BNM::Class("strange.extensions.injector.impl", "InjectionBinder");
    getInjectedInstance = injectionBinder.GetMethod("GetInstance", 2).cast<Obj*>();
    playClass = BNM::Class("PvZCards.Engine.Dtos", "Play");
    auto model = BNM::Class("PvZCards.Core.Utilities", "GameModel");
    localPlayerField = model.GetField("LocalPlayerId");
    opponentPlayerField = model.GetField("OpponentPlayerId");
    commandModel = commandClass.GetField("<GameModel>k__BackingField");
    commandService = commandClass.GetField("<SelectTargetService>k__BackingField");
    targetCommandStateSignal = commandClass.GetField("<GameStateUpdatedSignal>k__BackingField");
    retainedCommandsTracker = BNM::Class("PvZCards.Application", "HeroesCommand")
        .GetField("<RetainedCommandsTracker>k__BackingField");
    playCardCommandModel = playCardCommandClass.GetField("<GameModel>k__BackingField");
    playType = playClass.GetField("Type"); playPlayer = playClass.GetField("PlayerId");
    playCard = playClass.GetField("CardId"); playOrder = playClass.GetField("OrderInTargetLocation");
    playTargets = playClass.GetField("Targets");
    timerRunning = timerClass.GetField("IsRunning");
    timerPaused = timerClass.GetField("IsPaused");
    timerParameters = timerClass.GetField("Parameters");
    auto playCardExecute = playCardCommandClass.GetMethod("Execute", 0);
    auto inputUpdate = gameInputHolderClass.GetMethod("Update", 0);
    auto execute = commandClass.GetMethod("Execute", 0);
    auto boardTapExecute = BNM::Class("PvZCards.Game","HandleBoardTappedCommand").GetMethod("Execute",0);
    auto targetSuccess = commandClass.GetMethod("HandleServiceSuccess", 0);
    auto targetFailure = commandClass.GetMethod("HandleServiceFailure", 1);
    auto commandRelease = BNM::Class("PvZCards.Application", "HeroesCommand").GetMethod("Release", 0);
    auto push = stackClass.GetMethod("Push", 1);
    auto ctor = playClass.GetMethod(".ctor", 0);
    auto peek = stackClass.GetMethod("Peek", 0);
    auto getRemaining = timerClass.GetMethod("GetTimeRemaining", 0);
    const bool ok = localClass.IsValid() && remoteClass.IsValid() &&
        localPlayerField.IsValid() && commandModel.IsValid() && playType.IsValid() &&
        playPlayer.IsValid() && playCard.IsValid() && playOrder.IsValid() &&
        playTargets.IsValid() && commandService.IsValid() &&
        playCardExecute.IsValid() && playCardCommandModel.IsValid() &&
        execute.IsValid() && push.IsValid() && ctor.IsValid() &&
        inputUpdate.IsValid() && stateGameModel.IsValid() && stateCommandBinder.IsValid() &&
        peek.IsValid() && getRemaining.IsValid() && timerClass.IsValid() && timerParameters.IsValid() && timerRunning.IsValid() && timerPaused.IsValid();
    if (!ok || !SelectionCompletion::Initialize()) { LOG_ERR("CustomComponents/Pick3/SUBMIT: SETUP missing metadata"); return false; }
    allowPve = pve; allowPvp = pvp;
    pvpOwnerGateReady = localPlayerField.IsValid() && opponentPlayerField.IsValid() &&
                        TargetBridge::blockingPlayerId.IsValid();
    TargetBridge::resolveLocalPlayerId = &CapturedLocalPlayerId;
    TargetBridge::resolveRemoteSession = &Remote;
    if (allowPvp && !pvpOwnerGateReady) {
        LOG_ERR("CustomComponents/Pick3/PVP: OWNER_GATE_UNAVAILABLE remote selection disabled");
        allowPvp = false; // Do not compromise working PvE.
    }
    LOG_INFO("CustomComponents/Pick3/TRANSPORT: genericPlay=1 pve=%d pvpRequested=%d pvpEnabled=%d tracer=0 ownerGate=%d",
             allowPve ? 1 : 0, pvp ? 1 : 0, allowPvp ? 1 : 0, pvpOwnerGateReady ? 1 : 0);
    syncLocalCompletion = syncPveCompletion && (pve || pvp) &&
        gameStateUpdatedSignalClass.IsValid() && playCardStateUpdatedSignal.IsValid() &&
        dispatchGameStateUpdated.IsValid() && serviceOnSuccess.IsValid();
    CC_DIAG("CustomComponents/Pick3/SYNC410: SETUP requested=%d ready=%d signalType=%d field=%d dispatch=%d successField=%d",
        syncPveCompletion ? 1 : 0, syncLocalCompletion ? 1 : 0,
        gameStateUpdatedSignalClass.IsValid() ? 1 : 0,
        playCardStateUpdatedSignal.IsValid() ? 1 : 0,
        dispatchGameStateUpdated.IsValid() ? 1 : 0, serviceOnSuccess.IsValid() ? 1 : 0);
    resolveFromInjection = useInjectionResolver;
    TargetBridge::resolveMatchDatabase = useInjectionResolver
        ? &ResolveBoundCardDatabase : nullptr;
    CC_DIAG("CustomComponents/Pick3/SUBMIT: DI_META enabled=%d interface=%d getBinder=%d getService=%d",
        resolveFromInjection ? 1 : 0, selectServiceInterfaceClass.IsValid() ? 1 : 0,
        getInjectionBinder.IsValid() ? 1 : 0, getInjectedInstance.IsValid() ? 1 : 0);
    deadlineThresholdMs = std::clamp(thresholdMs, 100, 3000);
    // Enable before installing hooks: injection may occur on another thread.
    enabled = true;
    BNM::BasicHook(playCardExecute, PlayCardExecute, originalPlayCardExecute);
    BNM::BasicHook(execute, SelectTargetExecute, originalExecute);
    // Read-only hooks. Missing optional metadata never disables submission.
    if (targetSuccess.IsValid())
        BNM::BasicHook(targetSuccess, StockTargetSuccess, originalStockTargetSuccess);
    if (targetFailure.IsValid())
        BNM::BasicHook(targetFailure, StockTargetFailure, originalStockTargetFailure);
    if (commandRelease.IsValid())
        BNM::BasicHook(commandRelease, HeroesRelease, originalHeroesCommandRelease);
    LOG_INFO("CustomComponents/Pick3/STOCK_COMMAND: HOOKS success=%d failure=%d release=%d "
             "signalField=%d trackerField=%d",
             originalStockTargetSuccess ? 1 : 0, originalStockTargetFailure ? 1 : 0,
             originalHeroesCommandRelease ? 1 : 0,
             targetCommandStateSignal.IsValid() ? 1 : 0, retainedCommandsTracker.IsValid() ? 1 : 0);
    if (boardTapExecute.IsValid())
        BNM::BasicHook(boardTapExecute, BoardTapExecute, originalBoardTapExecute);
    BNM::BasicHook(push, TimerPush, originalPush);
    const bool observedGameInput =
        Systems::TargetingPresentationRefreshSystem::RegisterAfterUpdateObserver(GameInputHolderUpdated);
    enabled = originalPlayCardExecute && originalExecute && originalPush &&
        observedGameInput;
    CC_DIAG("CustomComponents/Pick3/SUBMIT: SETUP ready=%d pve=%d pvp=%d thresholdMs=%d hooks=playcard:%d execute:%d timer:%d gameInput:%d touchManager:0 setterHooks:0",
         enabled ? 1 : 0, pve ? 1 : 0, pvp ? 1 : 0, deadlineThresholdMs,
         originalPlayCardExecute ? 1 : 0, originalExecute ? 1 : 0,
         originalPush ? 1 : 0, observedGameInput ? 1 : 0);
    return enabled;
}
}
