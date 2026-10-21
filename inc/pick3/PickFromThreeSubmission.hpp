#pragma once
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
#include "log.h"

namespace CustomComponents::Pick3::Submission {
using Obj = BNM::IL2CPP::Il2CppObject;
using IntList = BNM::Structures::Mono::List<int>;
using Push = void(*)(Obj*, Obj*, const void*);
using Execute = void(*)(Obj*, const void*);
inline Execute originalPlayCardExecute = nullptr;
inline Execute originalBoardTapExecute = nullptr;
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
inline unsigned int signalDispatchCount = 0;
inline Push originalPush = nullptr;
inline Execute originalExecute = nullptr;
inline bool overlayCapturesInput = false;
inline unsigned int blockedStockClicks = 0;
inline BNM::Class localClass, remoteClass, playClass, commandClass, stackClass, timerClass;
inline BNM::Class serviceFactoryClass, playCardCommandClass;
inline BNM::Class gameInputHolderClass, gameCommandBinderClass;
inline BNM::Class selectServiceInterfaceClass;
inline BNM::Field<Obj*> stateCommandBinder, stateGameModel;
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
    if (stateGameModel.IsValid()) {
        Obj* model = stateGameModel[self].Get();
        if (model && Target(modelRoot) != model) {
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
        if (++blockedPlayCommands <= 12)
            CC_DIAG("CustomComponents/Pick3/INPUT: STOCK_PLAYCARD_BLOCKED count=%u", blockedPlayCommands);
        return;
    }
    if (originalPlayCardExecute) originalPlayCardExecute(self, method);
    if (trace)
        CC_DIAG("CustomComponents/Pick3/SUBMIT: PLAYCARD_EXEC_RETURN");
}
// Stock input can still reach SelectTargetCommand underneath a manually
// polled Canvas. Suppress that command only while OUR gameplay picker owns
// input; release ownership before sending the selected targets.
static void SelectTargetExecute(Obj* self, const void* method) {
    // Dependency injection has already populated these fields at Execute time.
    // Reading the fields is safe; intercepting adjacent 8-byte setters was not.
    if (enabled && self) {
        if (commandService.IsValid()) {
            Obj* service = commandService[self].Get();
            if (service && Target(serviceRoot) != service)
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
        if (++blockedStockClicks <= 24)
            CC_DIAG("CustomComponents/Pick3/SUBMIT: STOCK_INPUT_BLOCKED count=%u service=%d model=%d",
                     blockedStockClicks, Target(serviceRoot) ? 1 : 0,
                     Target(modelRoot) ? 1 : 0);
        return;
    }
    if (originalExecute) originalExecute(self, method);
}
// Board taps also propagate outside Unity's UGUI EventSystem. The stock
// HandleBoardTappedCommand.Execute() is 0x54 bytes on this APK (not an unsafe
// generated setter). Only silence it while our modal popup owns input.
static void BoardTapExecute(Obj* self, const void* method) {
    if (enabled && (overlayCapturesInput || TargetBridge::HasQueuedChoice())) {
        if (++blockedBoardTaps == 1)
            CC_DIAG("CustomComponents/Pick3/INPUT: BOARD_TAP_SUPPRESSED");
        return;
    }
    if (originalBoardTapExecute) originalBoardTapExecute(self,method);
}
inline void SetPopupInputCapture(bool capture) { overlayCapturesInput = capture && enabled; }
static void TimerPush(Obj* self, Obj* timer, const void* method) {
    if (originalPush) originalPush(self, timer, method);
    if (enabled && self && timer) CaptureRoot(stackRoot, self);
}
inline void ResetMatch() {
    overlayCapturesInput = false;
    blockedStockClicks = 0;
    blockedBoardTaps = 0;
    blockedPlayCommands = 0;
    dependencyPollFrame = 0;
    Drop(serviceRoot); Drop(modelRoot); Drop(stackRoot); Drop(binderRoot);
    Drop(updatedSignalRoot);
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
inline bool Allowed() { return enabled && ((Remote() && allowPvp) || (Local() && allowPve)); }
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
    if (timerPaused.IsValid() && timerPaused[timer].Get()) return -1;
    if (timerRunning.IsValid() && !timerRunning[timer].Get()) return -1;
    auto getRemaining = timerClass.GetMethod("GetTimeRemaining", 0).cast<std::int64_t>();
    if (!getRemaining.IsValid()) return -1;
    std::int64_t remain = -1;
    auto ex = BNM::TryInvoke([&]{ remain = getRemaining[timer](); });
    return ex.IsValid() ? -1 : remain;
}
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
    return Allowed() && Target(serviceRoot) && Target(modelRoot) && localPlayerField.IsValid();
}
// All picks are shipped in ONE Play.Targets payload. One click is not one service call.
// Distinguishes safely retryable pre-dispatch validation failures from an
// attempt that entered the game's Process() (which must NEVER be replayed).
inline bool lastSubmitReachedService = false;
inline bool CanRetryLastSubmission() { return !lastSubmitReachedService; }
inline bool Submit(const PreviewRequest& request, const std::vector<int>& indices, bool timeout) {
    lastSubmitReachedService = false;
    if (!Ready()) {
        LOG_ERR("CustomComponents/Pick3/SUBMIT: REJECT_NOT_READY svc=%d model=%d allowed=%d",
            Target(serviceRoot) ? 1 : 0, Target(modelRoot) ? 1 : 0, Allowed() ? 1 : 0);
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
    const bool databaseMode = request.mode == SelectionMode::CardDatabase;
    if (databaseMode && (!Local() || request.requiredTargets != 1)) {
        LOG_ERR("CustomComponents/Pick3/DB5: PVE_ONE_PICK_ONLY effect=%d", request.effectEntityId);
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
        if (badChoice || badTargetId || repeated || missingSnapshot) {
            LOG_ERR("CustomComponents/Pick3/SUBMIT: REJECT_STALE_OR_DUPLICATE effect=%d group=%d target=%d "
                    "db=%d badChoice=%d badId=%d repeated=%d snapshotMissing=%d",
                    request.effectEntityId, request.effectGroupId, entity, databaseMode ? 1 : 0,
                    badChoice ? 1 : 0, badTargetId ? 1 : 0,
                    repeated ? 1 : 0, missingSnapshot ? 1 : 0);
            return false;
        }
        if (databaseMode)
            CC_DIAG("CustomComponents/Pick3/DB5: RECIPIENT_SNAPSHOT_VALID effect=%d group=%d target=%d guid=%d",
                     request.effectEntityId, request.effectGroupId, entity,
                     request.options[static_cast<std::size_t>(index)].cardGuid);
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
    Obj* play = playClass.CreateNewInstance();
    if (!play) return false;
    std::uint32_t playRoot = Core::SceneViewContext::newGCHandle(play, false);
    if (!playRoot) return false;
    auto ctor = playClass.GetMethod(".ctor", 0).cast<void>();
    auto process = BNM::Class(svc).GetMethod("Process", 1).cast<void>();
    if (!ctor.IsValid() || !process.IsValid()) { Drop(playRoot); return false; }
    const auto initEx = BNM::TryInvoke([&]{ ctor[play](); });
    if (initEx.IsValid()) {
        LOG_ERR("CustomComponents/Pick3/SUBMIT: CTOR_EXCEPTION class=%s message=%s",
                initEx.ClassName().c_str(), initEx.Message().c_str());
        Drop(playRoot); return false;
    }
    IntList* ids = playTargets[play].Get();
    if (!ids) { Drop(playRoot); return false; }
    auto add = BNM::Class(static_cast<Obj*>(ids)).GetMethod("Add", 1).cast<void>();
    if (!add.IsValid()) { Drop(playRoot); return false; }
    for (int entity : entities) {
        auto addEx = BNM::TryInvoke([&]{ add[ids](entity); });
        if (addEx.IsValid()) {
            LOG_ERR("CustomComponents/Pick3/SUBMIT: TARGET_ADD_EXCEPTION class=%s message=%s",
                    addEx.ClassName().c_str(), addEx.Message().c_str());
            Drop(playRoot); return false;
        }
    }
    playType[play].Set(6);
    playCard[play].Set(-1);
    playPlayer[play].Set(playerId);
    playOrder[play].Set(0);
    // Store before stock Process(): downstream DrawCardFromSubset can evaluate
    // its GUID-domain query synchronously in this call. Failed submissions
    // remove the staged result. Never publish a global, unscoped last GUID.
    const bool pickedPublished = Local() && CustomComponents::Pick3::Picked::Store(
        request.effectEntityId, request.effectGroupId, selectedCards);
    if (Local() && !pickedPublished) {
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
    // Read only a stock BaseService field. The success handler, if installed,
    // will itself dispatch GameStateUpdatedSignal during Process().
    const bool hadStockSuccessHandler = Local() && serviceOnSuccess.IsValid() &&
        serviceOnSuccess[svc].Get() != nullptr;
    CC_DIAG("CustomComponents/Pick3/SYNC410: PRE_SUBMIT signal=%d stockHandler=%d local=%d",
        Target(updatedSignalRoot) ? 1 : 0, hadStockSuccessHandler ? 1 : 0, Local() ? 1 : 0);
    // From this line forward we cannot safely retry without proving the
    // service performed no work. A caught exception is still an attempt.
    lastSubmitReachedService = true;
    auto sendEx = BNM::TryInvoke([&]{ process[svc](play); });
    if (sendEx.IsValid()) {
        if (pickedPublished) CustomComponents::Pick3::Picked::Erase(
            request.effectEntityId, request.effectGroupId);
        // Step DB5: an exception must not leave the runtime target filter
        // redirected to AdditionalTargetQuery for the rest of the match.
        if (Local()) TargetBridge::CompleteLocalSelection(
            request.effectEntityId, request.effectGroupId);
        LOG_ERR("CustomComponents/Pick3/SUBMIT: SERVICE_EXCEPTION effect=%d class=%s message=%s (no retry)",
                request.effectEntityId, sendEx.ClassName().c_str(), sendEx.Message().c_str());
        return false;
    }
    CC_DIAG("CustomComponents/Pick3/SUBMIT: DISPATCH_RETURN effect=%d (acceptance unconfirmed)", request.effectEntityId);
    // Restore the original PickFromThree marker now that the synchronous
    // local selection service has finished reading this runtime filter.
    // Otherwise a pooled/shared PrimaryTargetFilter remains Manual=1 and the
    // next activation bypasses our custom candidate bridge entirely.
    const bool completedLocalService = Local();
    if (completedLocalService)
        TargetBridge::CompleteLocalSelection(request.effectEntityId, request.effectGroupId);
    // The original SelectTargetCommand.HandleServiceSuccess removes its
    // service handlers, calls Signal.Dispatch on injected field +0x50, and
    // finally completes the command. Our popup bypasses that stock command.
    // Without its notification, ProcessHistoryCommand may not run and the
    // frontend's ChooseTargetPhaseEndRecord remains unprocessed.
    // Restore *only* that event; do not mutate input/subturn/history state.
    // Service OnSuccess is checked before Process above, so a preexisting
    // stock command handler is not notified twice.
    if (Local() && syncLocalCompletion) {
        Obj* signal = Target(updatedSignalRoot);
        if (hadStockSuccessHandler) {
            CC_DIAG("CustomComponents/Pick3/SYNC410: NOTIFY_SKIPPED_STOCK_HANDLER effect=%d", request.effectEntityId);
        } else if (!signal || !dispatchGameStateUpdated.IsValid()) {
            LOG_ERR("CustomComponents/Pick3/SYNC410: NOTIFY_UNAVAILABLE effect=%d signal=%d dispatch=%d",
                request.effectEntityId, signal ? 1 : 0, dispatchGameStateUpdated.IsValid() ? 1 : 0);
        } else {
            CC_DIAG("CustomComponents/Pick3/SYNC410: NOTIFY_ENTER effect=%d count=%u", request.effectEntityId, signalDispatchCount + 1);
            const auto notifyEx = BNM::TryInvoke([&] { dispatchGameStateUpdated[signal](); });
            if (notifyEx.IsValid()) {
                LOG_ERR("CustomComponents/Pick3/SYNC410: NOTIFY_EXCEPTION effect=%d class=%s message=%s",
                    request.effectEntityId, notifyEx.ClassName().c_str(), notifyEx.Message().c_str());
            } else {
                ++signalDispatchCount;
                CC_DIAG("CustomComponents/Pick3/SYNC410: NOTIFY_RETURN effect=%d count=%u",
                    request.effectEntityId, signalDispatchCount);
            }
        }
    }
    // Deduplication is transaction-scoped, not match-scoped: the same card or
    // recycled effect ID can legitimately be used later this match. The UI
    // itself remains sealed until Close(), and the lock stayed held across
    // both Process() and the stock completion notification above.
    if (completedLocalService) {
        lastEffect = -1;
        lastGroup = -1;
        CC_DIAG("CustomComponents/Pick3/STEP411: SUBMISSION_REARMED effect=%d group=%d",
                 request.effectEntityId, request.effectGroupId);
    }
    return true;
}
inline bool Initialize(bool request, bool pve, bool pvp, int thresholdMs,
                       bool useInjectionResolver = true, bool syncPveCompletion = true) {
    if (!request) return true;
    if (!Core::SceneViewContext::newGCHandle || !Core::SceneViewContext::getGCHandle || !Core::SceneViewContext::freeGCHandle)
        return false;
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
    getInjectionBinder = gameCommandBinderClass.GetMethod("get_injectionBinder", 0).cast<Obj*>();
    auto injectionBinder = BNM::Class("strange.extensions.injector.impl", "InjectionBinder");
    getInjectedInstance = injectionBinder.GetMethod("GetInstance", 2).cast<Obj*>();
    playClass = BNM::Class("PvZCards.Engine.Dtos", "Play");
    auto model = BNM::Class("PvZCards.Core.Utilities", "GameModel");
    localPlayerField = model.GetField("LocalPlayerId");
    commandModel = commandClass.GetField("<GameModel>k__BackingField");
    commandService = commandClass.GetField("<SelectTargetService>k__BackingField");
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
    if (!ok) { LOG_ERR("CustomComponents/Pick3/SUBMIT: SETUP missing metadata"); return false; }
    allowPve = pve; allowPvp = pvp;
    syncLocalCompletion = syncPveCompletion && pve &&
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
