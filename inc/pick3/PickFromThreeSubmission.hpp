#pragma once
#include "pick3/SelectionResolutionPolicy.hpp"
#include "pick3/Pick3ReleaseSafety.hpp"
#include "pick3/RevivePairContract1723.hpp"
#include "diagnostics/Pf3RepairTrace.hpp"
#include "pick3/TargetButtonTrace.hpp"
#include "pick3/PhaseRecordTrace.hpp"
#include "diagnostics/DiagnosticLogging.hpp"
// Experimental stock selection submission for an explicitly marked PickFromThree effect.
// Captures GameModel from PlayCardCommand.Execute and SelectTargetCommand.Execute.
// Captures SelectTargetService from the existing Step 3 factory GetService tracer
// or the already-injected field on SelectTargetCommand.Execute.
// NEVER hook generated property setters (8-byte methods adjacent to other functions).
// No direct effect execution, no local-only PvP service, no modified target lists.
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
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
#include "pick3/Pick3CreationProvenanceTrace.hpp"
#include "gameplay/SelectionTransport.hpp"
#include "gameplay/SelectionCompletion.hpp"
#include "triggers/CustomTriggerSystem.hpp"
#include "pick3/Pick3NetworkTrace.hpp"
#include "pick3/Patch2QueryCallerObservation.hpp"
#include "pick3/Patch2ExactRouting.hpp"
#include "log.h"

namespace CustomComponents::Pick3::Submission {
using Obj = BNM::IL2CPP::Il2CppObject;
using IntList = BNM::Structures::Mono::List<int>;
using Push = void(*)(Obj*, Obj*, const void*);
using Execute = void(*)(Obj*, const void*);
inline Execute originalPlayCardExecute = nullptr;
// Passive trick placement diagnostics. These hooks call stock unchanged.
inline Execute originalTrickStopDrag = nullptr;
// Trick-only play acceptance diagnostics: no mutations, every stock call forwarded.
using AcceptFailureFn = void(*)(Obj*, Obj*, const void*);
inline Execute originalTrickCommandSuccess = nullptr;
inline AcceptFailureFn originalTrickCommandFailure = nullptr;
using AcceptLocalProcessFn = void(*)(Obj*, Obj*, const void*);
inline AcceptLocalProcessFn originalTrickLocalProcess = nullptr;
// Service-boundary trace uses actual Play.CardId, not the last drag hint.
using TrickDetermineFn = bool(*)(Obj*, Obj*, const void*);
inline TrickDetermineFn originalTrickDetermine = nullptr;
inline BNM::Field<int> trickPlayCardIdField;
inline BNM::Field<int> trickPlayPlayerIdField;
inline BNM::Field<int> trickPlayTypeField;
inline BNM::Field<int> trickPlayOrderField;
inline BNM::Field<IntList*> trickPlayTargetsField;
inline BNM::Field<Obj*> trickHelperFailModelField;
inline BNM::Field<Obj*> trickFailureReasonsField;
inline thread_local Obj* trickServiceActivePlay = nullptr;
inline thread_local Obj* trickDetermineActivePlay = nullptr;
// Active candidate play is scoped to Validate invoked from GetValidPlaysForCard.
inline thread_local Obj* trickCandidateActivePlay = nullptr;
// Native game validator's injected simulation model, scoped to the
// synchronous OneTimeEffectValidator.Validate call. No rooted or pooled
// objects are cached beyond this call; never substitute the presentation model.
inline BNM::Field<Obj*> pick3OneTimeValidatorModelField;
inline thread_local Obj* pick3CurrentValidationModel = nullptr;
inline thread_local int pick3CurrentValidationCard = -1;

inline thread_local unsigned trickServiceTraceSeq = 0;
inline unsigned trickServiceTraceTotal = 0;
// Validated candidate snapshot: IDs only, never retain pooled managed Play/List.
struct DeferredValidatedPlay {
    int card=-1, zone=-1, player=-1, guid=-1, effect=-1;
    // 1.7.16: a sole PRIMARY has an independently stock-validated target.
    // Preserve the casting zone for drag identity, but submit this primary
    // in the initial Play only when the source validator proved it unique.
    int stockSolePrimary=-1;
    std::chrono::steady_clock::time_point provenAt{};
};
inline thread_local DeferredValidatedPlay deferredValidatedPlay{};
// Forward declarations: root helpers are implemented after the drag observers.
extern std::uint32_t boundEntityModelRoot;
extern std::uint32_t reviveLegalPlayModelRoot;
inline Obj* Target(std::uint32_t root);
inline void CaptureRoot(std::uint32_t& root, Obj* obj);
inline thread_local bool reviveDragLookupActive = false;
struct ScopedReviveDragLookup {
    bool previous;
    explicit ScopedReviveDragLookup(bool active) : previous(reviveDragLookupActive) { reviveDragLookupActive = active; }
    ~ScopedReviveDragLookup() { reviveDragLookupActive = previous; }
};
inline int TrickGenerationGuid(int id);
// A stock-generated legal Play is the authority for the initial casting target.
// It is never proof that a later card/lane pair is legal or that PvP accepted it.
inline bool IsRecentValidatedInitialPlay(int card, int zone) {
    const auto& p = deferredValidatedPlay;
    if (card < 0 || zone < 0 || p.card != card || p.zone != zone ||
        p.effect < 0 || !Target(reviveLegalPlayModelRoot) ||
        p.guid != TrickGenerationGuid(card)) return false;
    const auto age = std::chrono::steady_clock::now() - p.provenAt;
    return age >= std::chrono::steady_clock::duration::zero() &&
        age <= std::chrono::seconds(8);
}
inline thread_local int deferredDropCard=-1;
inline thread_local int deferredDropZone=-1;
inline thread_local unsigned deferredDropSeq=0;

// Explicit opt-in. OFF means all gameplay paths remain stock.
// This trial requires initial play generation and validator comparison to agree
// on the EXACT same generated effect-zone entity; no board-overlap fallback.
inline constexpr bool kDeferredPick3TrialEnabled = true;
// Only populated during the actual LocalPlayCardService validator call.
// IDs copied from managed PoolableList via get_Item; no managed references held.
struct DeferredPrimaryCards { int card=-1; int guid=-1; std::vector<int> ids; };
inline thread_local DeferredPrimaryCards deferredPrimaryCards{};
// Captured only from a successful stock OneTimeEffectValidator invocation
// while GetValidPlaysForCard is generating the candidate, not from UI offers.
// A single returned Play does NOT imply a single eligible discarded card.
struct StockSoleInitialPrimary {
    int card=-1, effect=-1, target=-1;
    Obj* model=nullptr; // synchronous comparison only; never dereference later
    bool ambiguous=false;
};
inline thread_local StockSoleInitialPrimary stockSoleInitialPrimary{};
// One successful PlayCardService validation is followed by asynchronous stock
// effect creation and PickFromThree target discovery. The synchronous primary
// list is gone by then. Retain ONLY copied entity IDs, bound to the exact
// source card, effect and simulation model; never retain pooled collections.
struct Pick3AcceptedCastPrimaries {
    Obj* model = nullptr; // identity only; never dereference this retained pointer
    int card = -1;
    int effect = -1;
    std::vector<int> ids;
    std::chrono::steady_clock::time_point captured{};
};
inline thread_local Pick3AcceptedCastPrimaries pick3AcceptedCastPrimaries{};

// 1.7.10: a *verified initial Play* target is not necessarily the hand entity.
// Preserve only its numeric identity after the ORIGINAL service validator
// succeeds, for the exact source/effect/model and a short bounded lifetime.
// This is not proof of an arbitrary remote Play's target.
struct Pick3VerifiedInitialCast {
    Obj* model = nullptr; // identity only, never dereferenced after capture
    int source = -1;
    int effect = -1;
    int target = -1;
    std::chrono::steady_clock::time_point captured{};
};
inline thread_local Pick3VerifiedInitialCast pick3VerifiedInitialCast{};
inline thread_local int pick3SynchronousVerifiedCastTarget = -1;
inline int Pick3VerifiedCastTargetFor(Obj* model, int source, int effect) {
    const auto& r = pick3VerifiedInitialCast;
    if (!model || model != r.model || source != r.source || effect != r.effect || r.target < 0)
        return -1;
    const auto age = std::chrono::steady_clock::now() - r.captured;
    return age >= std::chrono::steady_clock::duration::zero() &&
        age <= std::chrono::seconds(30) ? r.target : -1;
}

// The copy is valid only for the current synchronous stock-validation scope.
inline thread_local Obj* pick3PrimaryCaptureModel = nullptr;
inline thread_local int pick3PrimaryCaptureEffect = -1;
inline thread_local int pick3PrimaryCaptureCard = -1;

// Registered by the custom discard-zone system after its field bindings are
// initialized. Avoid importing its header into submission (cyclic dependency).
using Pick3PhysicalPairCheck = bool(*)(Obj*, int, int, int);
inline Pick3PhysicalPairCheck pick3PhysicalPairCheck = nullptr;
inline void RegisterPick3PhysicalPairCheck(Pick3PhysicalPairCheck fn) {
    pick3PhysicalPairCheck = fn;
}

// Optional effect-aware secondary query evaluator. This is a *filter* decision,
// not physical placement permission. The caller always runs the shared
// discard-to-lane legality check after this returns Allow. Implementations must
// evaluate the actual secondary filter/query on `model`, for THIS primary and
// lane; they must fail closed if they cannot establish the correct context.
// Never retain the query/filter objects or pooled contexts outside the call.
enum class Pick3SecondaryDecision : std::uint8_t { Unsupported, Reject, Allow };
using Pick3SecondaryFilterEvaluator = Pick3SecondaryDecision(*)(
    Obj* model, int effectId, Obj* secondaryFilter, Obj* secondaryQuery,
    int primaryCardId, int laneId, int playerId);
inline Pick3SecondaryFilterEvaluator pick3SecondaryFilterEvaluator = nullptr;
inline void RegisterPick3SecondaryFilterEvaluator(Pick3SecondaryFilterEvaluator fn) {
    pick3SecondaryFilterEvaluator = fn;
}

// Prefer the accepted service Play’s model/effect-bound copied primary IDs.
// The synchronous validation cache remains available inside the validator.

// Generic proof for a selection-dependent trigger during the actual stock
// event. A cached list of legal choices alone is NOT proof of the choice:
// require the actual event target to appear exactly once in the stock-accepted
// primary list for this same source/effect/model within its bounded lifetime.
inline bool ProveInFlightSelectedTarget(Obj* model, int sourceCard,
                                       int effectId, int targetId) {
    const auto& proof = pick3AcceptedCastPrimaries;
    if (!kDeferredPick3TrialEnabled || !model || sourceCard <= 0 ||
        effectId <= 0 || targetId <= 0 || proof.model != model ||
        proof.card != sourceCard || proof.effect != effectId ||
        proof.ids.empty() || proof.ids.size() > 128 ||
        std::count(proof.ids.begin(), proof.ids.end(), targetId) != 1)
        return false;
    const auto age = std::chrono::steady_clock::now() - proof.captured;
    return age >= std::chrono::steady_clock::duration::zero() &&
           age <= std::chrono::seconds(30);
}

inline std::vector<int> ValidatedInitialCardTargets(Obj* model, int card, int effectId) {
    if (kDeferredPick3TrialEnabled && model &&
        pick3AcceptedCastPrimaries.model == model &&
        pick3AcceptedCastPrimaries.card == card &&
        pick3AcceptedCastPrimaries.effect == effectId &&
        !pick3AcceptedCastPrimaries.ids.empty()) {
        const auto age = std::chrono::steady_clock::now() - pick3AcceptedCastPrimaries.captured;
        if (age >= std::chrono::steady_clock::duration::zero() &&
            age <= std::chrono::seconds(30)) {
            LOG_INFO("CustomComponents/Pick3/PICK3_REVIVE_OFFER_BRIDGE: phase=READ card=%d effect=%d count=%zu modelMatch=1 ageMs=%lld",
                card, pick3AcceptedCastPrimaries.effect, pick3AcceptedCastPrimaries.ids.size(),
                static_cast<long long>(std::chrono::duration_cast<std::chrono::milliseconds>(age).count()));
            return pick3AcceptedCastPrimaries.ids;
        }
        LOG_INFO("CustomComponents/Pick3/PICK3_REVIVE_OFFER_BRIDGE: phase=EXPIRE card=%d", card);
        pick3AcceptedCastPrimaries = {};
    }
    if (!kDeferredPick3TrialEnabled || !pick3CurrentValidationModel ||
        model != pick3CurrentValidationModel ||
        deferredPrimaryCards.card!=card ||
        pick3PrimaryCaptureModel != pick3CurrentValidationModel ||
        pick3PrimaryCaptureCard != card) {
        static thread_local unsigned misses = 0;
        if (kDeferredPick3TrialEnabled && ++misses <= 32)
            LOG_INFO("CustomComponents/Pick3/PICK3_PAIR_CONTEXT: phase=PRIMARY_CACHE_MISS card=%d cachedCard=%d producer=missing copiedIds=0", card, deferredPrimaryCards.card);
        return {};
    }
    static thread_local unsigned hits = 0;
    if (++hits <= 32)
        LOG_INFO("CustomComponents/Pick3/PICK3_PAIR_CONTEXT: phase=PRIMARY_CACHE_READ card=%d ids=%zu producer=stock-effect-targets decision=observation-only", card, deferredPrimaryCards.ids.size());
    return deferredPrimaryCards.ids;
}

struct DeferredInitialZoneRecord { int card=-1; int zone=-1; };
inline DeferredInitialZoneRecord deferredInitialZones[32]{};
inline void RememberDeferredInitialZone(int card, int zone) {
    if(card < 0 || zone < 0) return;
    deferredInitialZones[static_cast<unsigned>(card)%32] = {card,zone};
}
inline int FindDeferredInitialZone(int card) {
    if(card < 0) return -1;
    auto& record=deferredInitialZones[static_cast<unsigned>(card)%32];
    return record.card==card?record.zone:-1;
}

inline bool TrickFocus(int guid);
inline int TrickGenerationGuid(int id);
inline bool IsGenericPick3ReviveBurst(int cardId);
// PvP crash boundary diagnostic: bounded, per-thread, log-only breadcrumbs.
inline thread_local unsigned pvpCrashTraceSerial = 0;
inline thread_local unsigned pvpCrashTraceDepth = 0;
inline constexpr bool kPvpCrashVerboseTraceEnabled = false;
inline bool PvpCrashTraceEnabled() { return kPvpCrashVerboseTraceEnabled && pvpCrashTraceSerial < 1200; }
inline void PvpCrashMark(const char* tag, const char* phase, int card, const void* ptr=nullptr) {
    if (!PvpCrashTraceEnabled()) return;
    LOG_INFO("CustomComponents/Pick3/PVP_CRASH_TRACE: serial=%u depth=%u tag=%s phase=%s card=%d ptr=%p",
             pvpCrashTraceSerial, pvpCrashTraceDepth, tag, phase, card, ptr);
}
inline int TrickServicePlayGuid(Obj* play) {
    if (!play || !trickPlayCardIdField.IsValid()) return -1;
    const int cardId = trickPlayCardIdField[play].Get();
    return cardId >= 0 ? TrickGenerationGuid(cardId) : -1;
}
inline void TrickServiceDumpPlay(const char* phase, Obj* play, unsigned seq) {
    if (!play) return;
    const int card=trickPlayCardIdField.IsValid()?trickPlayCardIdField[play].Get():-1;
    const int guid=TrickServicePlayGuid(play);
    const int player=trickPlayPlayerIdField.IsValid()?trickPlayPlayerIdField[play].Get():-1;
    const int type=trickPlayTypeField.IsValid()?trickPlayTypeField[play].Get():-1;
    const int order=trickPlayOrderField.IsValid()?trickPlayOrderField[play].Get():-1;
    IntList* targets=trickPlayTargetsField.IsValid()?trickPlayTargetsField[play].Get():nullptr;
    int count=-1; int ids[8]{};
    if(targets && targets->size>=0 && targets->size<=32 && (!targets->size || targets->items)) {
        count=targets->size;
        for(int i=0;i<count && i<8;++i) ids[i]=targets->items->m_Items[i];
    }
    LOG_INFO("CustomComponents/Pick3/TRICK_SERVICE: phase=%s seq=%u play=%p guid=%d card=%d player=%d type=%d order=%d count=%d shown=%d targets=[%d,%d,%d,%d,%d,%d,%d,%d]",phase,seq,play,guid,card,player,type,order,count,count<0?0:(count<8?count:8),ids[0],ids[1],ids[2],ids[3],ids[4],ids[5],ids[6],ids[7]);
}
inline bool TrickDetermineTrace(Obj* self, Obj* play, const void* method) {
    PvpCrashMark("TRICK_DETERMINE","ENTER",-1,play);
    const int guid=TrickServicePlayGuid(play);
    const bool focus=TrickFocus(guid) && trickServiceTraceTotal<100;
    const Obj* prior=trickDetermineActivePlay;
    unsigned seq=0;
    if(focus) {
        seq=++trickServiceTraceTotal;
        trickDetermineActivePlay=play;
        TrickServiceDumpPlay("DETERMINE_ENTER",play,seq);
        LOG_INFO("CustomComponents/Pick3/TRICK_SERVICE: phase=DETERMINE_CONTEXT seq=%u helper=%p sameServicePlay=%d",seq,self,play==trickServiceActivePlay?1:0);
    }
    const bool result=originalTrickDetermine?originalTrickDetermine(self,play,method):false;
    if(focus) {
        Obj* failModel=trickHelperFailModelField.IsValid()?trickHelperFailModelField[self].Get():nullptr;
        Obj* failures=failModel&&trickFailureReasonsField.IsValid()?trickFailureReasonsField[failModel].Get():nullptr;
        int count=-2;
        if(failures) {
            auto* list=reinterpret_cast<BNM::Structures::Mono::List<int>*>(failures);
            if(list->size>=0&&list->size<=128) count=list->size;
        }
        LOG_INFO("CustomComponents/Pick3/TRICK_SERVICE: phase=DETERMINE_EXIT seq=%u play=%p guid=%d result=%d failModel=%p errorCount=%d",seq,play,guid,result?1:0,failModel,count);
        trickDetermineActivePlay=const_cast<Obj*>(prior);
    }
    return result;
}
// Patch 1.7.14: diagnostic only. Local validator acceptance of a custom
// revive pair MUST NOT be mistaken for authoritative PvP acceptance.
// Never store a Play pointer or suppress stock errors here.
inline thread_local bool auth1714LocalPairAccepted = false;
inline thread_local int auth1714LocalPairCard = -1;
inline thread_local int auth1714LocalPairTarget = -1;
inline unsigned trickAcceptanceSeq = 0;
inline Obj* trickPendingCommand = nullptr;
inline int trickPendingGuid = -1;
inline int trickPendingSource = -1;
inline int trickPendingTarget = -1;
inline bool TrickFocus(int guid) { return guid == 699 || guid == 712 || guid == 713 || guid == 27; }
inline void TrickPlaySuccess(Obj* self, const void* method) {
    if (self && self == trickPendingCommand && TrickFocus(trickPendingGuid))
        LOG_INFO("CustomComponents/Pick3/TRICK_ACCEPT: phase=COMMAND_SUCCESS seq=%u guid=%d command=%p source=%d target=%d", trickAcceptanceSeq, trickPendingGuid, self, trickPendingSource, trickPendingTarget);
    if (originalTrickCommandSuccess) originalTrickCommandSuccess(self, method);
}
inline void TrickPlayFailure(Obj* self, Obj* failure, const void* method) {
    if (self && self == trickPendingCommand && TrickFocus(trickPendingGuid))
        LOG_INFO("CustomComponents/Pick3/TRICK_ACCEPT: phase=COMMAND_FAILURE seq=%u guid=%d command=%p failure=%p source=%d target=%d", trickAcceptanceSeq, trickPendingGuid, self, failure, trickPendingSource, trickPendingTarget);
    if (originalTrickCommandFailure) originalTrickCommandFailure(self, failure, method);
}
inline void TrickLocalPlayProcess(Obj* self, Obj* play, const void* method) {
    const int guid=TrickServicePlayGuid(play);
    const bool focus=TrickFocus(guid);
    Obj* prior=trickServiceActivePlay;
    const bool previousPairAccepted = auth1714LocalPairAccepted;
    const int previousPairCard = auth1714LocalPairCard;
    const int previousPairTarget = auth1714LocalPairTarget;
    auth1714LocalPairAccepted=false;
    auth1714LocalPairCard=-1;
    auth1714LocalPairTarget=-1;
    unsigned seq=0;
    // PlayServiceProcessor can invoke the receiving LocalPlayCardService without
    // PlayCardHelper.DetermineIfPlayIsValid on the same stack. Keep an exact
    // service identity for ALL plays, including nested Type-6 selections.
    // The validator below accepts the cast exception ONLY for the exact
    // Type-3 Play + Pick3 revive signature on its own simulation model.
    // Saving the inner Play also prevents reusing an outer Type-3 context.
    if(play) trickServiceActivePlay=play;
    if(focus) {
        seq=++trickServiceTraceSeq;
        TrickServiceDumpPlay("PROCESS_ENTER",play,seq);
    }
    if (originalTrickLocalProcess) originalTrickLocalProcess(self, play, method);
    if(focus) {
        PF3_REPAIR_TRACE("CustomComponents/Pick3/REPAIR1714: LOCAL_PROCESS_CONTRACT seq=%u guid=%d card=%d pairBypass=%d pairCard=%d initialTarget=%d localOnly=1 serverAck=0",
            seq,guid,trickPlayCardIdField.IsValid()?trickPlayCardIdField[play].Get():-1,
            int(auth1714LocalPairAccepted),auth1714LocalPairCard,auth1714LocalPairTarget);
        LOG_INFO("CustomComponents/Pick3/TRICK_SERVICE: phase=PROCESS_EXIT seq=%u play=%p guid=%d",seq,play,guid);
    }
    if(play) trickServiceActivePlay=prior;
    auth1714LocalPairAccepted=previousPairAccepted;
    auth1714LocalPairCard=previousPairCard;
    auth1714LocalPairTarget=previousPairTarget;
}

using TrickTargetsFn = Obj* (*)(Obj*, int, int, Obj*, const void*);
inline TrickTargetsFn originalTrickValidTargets = nullptr;
inline unsigned trickDragObservations = 0;
inline unsigned trickTargetObservations = 0;
inline unsigned trickActiveDragSeq = 0;
inline BNM::Field<Obj*> trickGestureField;
inline BNM::Field<Obj*> trickDragModelField;
inline BNM::Field<int> trickDraggedCardField;
inline BNM::Field<bool> trickInHandField;
inline BNM::Field<IntList*> trickTargetIdsField;
inline BNM::Field<Obj*> trickOverlapFinderField;


inline Execute originalTrickStartDrag = nullptr;
inline Execute originalTrickUpdateDrag = nullptr;
using TrickTopDraggedFn = int(*)(Obj*, Obj*, int, const void*);
inline TrickTopDraggedFn originalTrickTopDragged = nullptr;
inline unsigned trickResolveObservations = 0;
// Read-only PlayCache -> overlap-resolution correlation. The overlap argument is
// IEnumerable<int>, NOT necessarily a List<int>; never inspect its layout.
using TrickCachedPlaysFn = Obj*(*)(Obj*, int, const void*);
using TrickFirstValidFn = int(*)(Obj*, Obj*, const void*); // static method
inline TrickCachedPlaysFn originalTrickCachedPlays = nullptr;
inline TrickFirstValidFn originalTrickFirstValid = nullptr;
inline thread_local unsigned trickCacheResolveDepth = 0;
inline thread_local unsigned trickCacheProbeSeq = 0;
inline thread_local int trickCacheProbeCard = -1;
inline thread_local int trickCacheResultCount = -2;
inline thread_local int trickCacheFirstResult = -2;
inline unsigned trickCacheResolveSamples = 0;
inline int TrickSafeManagedPlayCount(Obj* list) {
    if (!list) return -1;
    auto* plays = reinterpret_cast<BNM::Structures::Mono::List<Obj*>*>(list);
    return plays->size >= 0 && plays->size <= 2048 &&
           (plays->size == 0 || plays->items) ? plays->size : -2;
}
inline Obj* TrickCachedPlaysTrace(Obj* self, int cardId, const void* method) {
    Obj* result = originalTrickCachedPlays ? originalTrickCachedPlays(self,cardId,method) : nullptr;
    if (trickCacheResolveDepth && cardId == trickCacheProbeCard) {
        trickCacheResultCount = TrickSafeManagedPlayCount(result);
        LOG_INFO("CustomComponents/Pick3/TRICK_CACHE: phase=LOOKUP seq=%u card=%d cache=%p list=%p count=%d", trickCacheProbeSeq,cardId,self,result,trickCacheResultCount);
    }
    return result;
}
inline int TrickFirstValidTrace(Obj* overlapped, Obj* plays, const void* method) {
    const int result = originalTrickFirstValid ? originalTrickFirstValid(overlapped,plays,method) : -1;
    if (trickCacheResolveDepth) {
        trickCacheFirstResult = result;
        LOG_INFO("CustomComponents/Pick3/TRICK_CACHE: phase=FIRST_VALID seq=%u card=%d plays=%p count=%d overlaps=%p resolved=%d", trickCacheProbeSeq,trickCacheProbeCard,plays,TrickSafeManagedPlayCount(plays),overlapped,result);
    }
    return result;
}

inline unsigned trickStartObservations = 0;
inline unsigned trickUpdateObservations = 0;
// Read-only trick-specific stock secondary-target boundary diagnostics.
// 699=return-to-hand control; 712/713=return-to-lane cases; 27=stock move trick.
// No pooled return values are enumerated, released, or modified.
inline int trickSecondaryFocusGuid = -1;
inline unsigned trickSecondaryTraceCount = 0;
using BurstTargetsTraceFn = void(*)(Obj*, Obj*, Obj*, const void*);
using FindSecondaryTraceFn = Obj*(*)(Obj*, Obj*, Obj*, const void*);
using CreateSecondaryTraceFn = Obj*(*)(Obj*, Obj*, Obj*, Obj*, const void*);
inline BurstTargetsTraceFn originalBurstTargetsTrace = nullptr;
inline FindSecondaryTraceFn originalFindSecondaryTrace = nullptr;
inline CreateSecondaryTraceFn originalCreateSecondaryTrace = nullptr;
// Effect targeting investigation: correlate on the calling thread, not the last drag.
// GameStateUtils burst receives Entity*, whose Id backing field is at +0x18.
inline thread_local unsigned trickEffectBurstDepth = 0;
inline thread_local int trickEffectBurstGuid = -1;
inline thread_local int trickEffectBurstEntity = -1;
inline unsigned trickEffectBurstSamples = 0;
using EffectTargetedFn = bool(*)(Obj*, int, const void*);
using EffectValidTargetsFn = Obj*(*)(Obj*, int, const void*);
inline EffectTargetedFn originalEffectIsTargeted = nullptr;
inline EffectValidTargetsFn originalEffectValidTargets = nullptr;
extern thread_local unsigned trickValidationDepth;
extern thread_local int trickValidationGuid;
extern thread_local int trickValidationCard;
inline bool IsGenericPick3ReviveBurst(int cardId);
inline bool IsGenericPick3ReviveEffect(int effectId);
inline void TrickEffectLogList(const char* phase, int guid, int source, Obj* raw) {
    const auto* list = reinterpret_cast<const IntList*>(raw);
    if (!list || list->size < 0 || list->size > 4096 || (list->size && !list->items)) {
        LOG_INFO("CustomComponents/Pick3/TRICK_EFFECT_PATH: phase=%s guid=%d cardEntity=%d list=%p opaque=1", phase, guid, source, raw);
        return;
    }
    char ids[180] = {};
    size_t n = 0;
    for (int i=0; i<std::min(list->size, 12); ++i) {
        const int z = snprintf(ids+n, sizeof(ids)-n, "%s%d", i?",":"", list->items->m_Items[i]);
        if (z <= 0 || size_t(z) >= sizeof(ids)-n) break;
        n += size_t(z);
    }
    LOG_INFO("CustomComponents/Pick3/TRICK_EFFECT_PATH: phase=%s guid=%d cardEntity=%d count=%d ids=[%s]%s", phase, guid, source, list->size, ids, list->size>12?" truncated=1":"");
}
// Experimental *initial Burst target generation* adapter for deferred Pick3->lane tricks.
// Reversible: set this constant to false to restore completely stock classification.
// This does NOT change effect components, the secondary lane filter, other cards,
// or global targeting decisions made outside GetPossiblePlayTargetsForBurst.
inline constexpr bool kDeferredPick3InitialZoneExperiment = kDeferredPick3TrialEnabled;
inline unsigned trickInitialZoneOverrideCount = 0;
inline bool TrickEffectIsTargeted(Obj* self, int effectId, const void* method) {
    const bool stock = originalEffectIsTargeted ? originalEffectIsTargeted(self, effectId, method) : false;
    // Only the first, structurally identified PickFromThree revive effect may
    // use the initial casting-zone adapter. Do not suppress targetedness for
    // later PlayTriggers or other effects belonging to the same card.
    const bool scope = stock && kDeferredPick3InitialZoneExperiment && trickEffectBurstDepth != 0 &&
                       IsGenericPick3ReviveBurst(trickEffectBurstEntity) &&
                       IsGenericPick3ReviveEffect(effectId);
    const bool overrideToZone = kDeferredPick3InitialZoneExperiment && scope && stock;
    const bool result = overrideToZone ? false : stock;
    if (trickEffectBurstDepth && TrickFocus(trickEffectBurstGuid)) {
        LOG_INFO("CustomComponents/Pick3/TRICK_EFFECT_PATH: phase=IS_TARGETED guid=%d cardEntity=%d effectId=%d stock=%d returned=%d zoneExperiment=%d scoped=%d",
                 trickEffectBurstGuid, trickEffectBurstEntity, effectId, stock?1:0,
                 result?1:0, overrideToZone?1:0, scope?1:0);
        if (overrideToZone && ++trickInitialZoneOverrideCount <= 24)
            LOG_INFO("CustomComponents/Pick3/TRICK_INITIAL_ZONE: phase=OVERRIDE guid=%d cardEntity=%d effectId=%d count=%u initialBurstOnly=1 secondaryFilterUntouched=1",
                     trickEffectBurstGuid, trickEffectBurstEntity, effectId, trickInitialZoneOverrideCount);
    }
    return result;
}
inline Obj* TrickEffectValidTargets(Obj* self, int effectId, const void* method) {
    Obj* result = originalEffectValidTargets ? originalEffectValidTargets(self, effectId, method) : nullptr;
    if (trickEffectBurstDepth && TrickFocus(trickEffectBurstGuid)) {
        LOG_INFO("CustomComponents/Pick3/TRICK_EFFECT_PATH: phase=EFFECT_TARGET_REQUEST guid=%d cardEntity=%d effectId=%d", trickEffectBurstGuid, trickEffectBurstEntity, effectId);
        TrickEffectLogList("EFFECT_TARGETS", trickEffectBurstGuid, trickEffectBurstEntity, result);
    }
    // OneTimeEffectValidator owns the returned PoolableList<int> until its
    // synchronous IsTargetingValid comparison. Copy IDs *before returning*
    // to stock; never retain or release the list or its backing array.
    // A wrong result type or incomplete ownership context is a hard skip.
    static thread_local unsigned pick3ValidationTargetSamples = 0;
    if (pick3CurrentValidationModel && pick3PrimaryCaptureCard >= 0 &&
        pick3PrimaryCaptureEffect == effectId &&
        pick3CurrentValidationCard == pick3PrimaryCaptureCard) {
        const char* reason = "NO_RESULT";
        int count = -1;
        int copied = 0;
        deferredPrimaryCards = {};
        if (result) {
            auto* klass = BNM::Class(result).GetClass();
            const char* name = klass ? klass->name : nullptr;
            if (!name || (std::strcmp(name, "PoolableList`1") &&
                          std::strcmp(name, "PoolableList<int>"))) {
                reason = "WRONG_CONCRETE_TYPE";
            } else {
                // PoolableList<T> derives from List<T>; these are the
                // BNM-provided concrete List<int> fields, not ICollection
                // virtual calls. Never cast an arbitrary interface object.
                const auto* list = reinterpret_cast<const IntList*>(result);
                count = list->size;
                if (count < 0 || count > 128 ||
                    (count && (!list->items ||
                    list->items->capacity < static_cast<size_t>(count)))) {
                    reason = "INVALID_LIST_BOUNDS";
                } else {
                    reason = "COPIED_CONCRETE_LIST";
                    deferredPrimaryCards.card = pick3PrimaryCaptureCard;
                    deferredPrimaryCards.guid = TrickGenerationGuid(pick3PrimaryCaptureCard);
                    deferredPrimaryCards.ids.reserve(static_cast<size_t>(count));
                    for (int i = 0; i < count; ++i) {
                        const int id = list->items->m_Items[i];
                        if (id >= 0) deferredPrimaryCards.ids.push_back(id);
                    }
                    copied = static_cast<int>(deferredPrimaryCards.ids.size());
                }
            }
        }
        // Sampling limits diagnostics, never the production of the synchronous
        // validated-primary snapshot. Otherwise long matches silently lose it.
        if (++pick3ValidationTargetSamples <= 128) {
            const int firstId = copied > 0 ? deferredPrimaryCards.ids[0] : -1;
            const int secondId = copied > 1 ? deferredPrimaryCards.ids[1] : -1;
            LOG_INFO("CustomComponents/Pick3/PICK3_STOCK_PRIMARY_CAPTURE: card=%d guid=%d effect=%d model=%p pool=%p count=%d copied=%d firstId=%d secondId=%d reason=%s secondaryPairVerified=0 decision=stock-unchanged",
                     pick3PrimaryCaptureCard, TrickGenerationGuid(pick3PrimaryCaptureCard), effectId,
                     pick3CurrentValidationModel, result, count, copied,
                     firstId, secondId, reason);
        }
    }
    return result;
}
inline int TrickGenerationGuid(int id);
inline bool IsGenericPick3ReviveBurst(int cardId);
inline void TrickBurstTargetsTrace(Obj* self, Obj* card, Obj* targetIds, const void* method) {
    const int id = card ? *reinterpret_cast<const int*>(reinterpret_cast<const std::uint8_t*>(card)+0x18) : -1;
    const int guid = id >= 0 ? TrickGenerationGuid(id) : -1;
    const bool genericScope=IsGenericPick3ReviveBurst(id);
    // Gameplay scope must never depend on the diagnostic sample budget.
    // In particular the original targetedness callback uses this TLS scope.
    const bool relevant = TrickFocus(guid) || genericScope;
    const bool trace = relevant && ++trickEffectBurstSamples <= 160;
    const unsigned oldDepth = trickEffectBurstDepth;
    const int oldGuid = trickEffectBurstGuid;
    const int oldEntity = trickEffectBurstEntity;
    if (relevant) {
        ++trickEffectBurstDepth;
        trickEffectBurstGuid = guid;
        trickEffectBurstEntity = id;
        if (trace) TrickEffectLogList("BURST_BEFORE", guid, id, targetIds);
    }
    if (originalBurstTargetsTrace) originalBurstTargetsTrace(self, card, targetIds, method);
    if (kDeferredPick3TrialEnabled && relevant && genericScope) {
        const auto* list=reinterpret_cast<const IntList*>(targetIds);
        // Only record the engine-generated single zone ID, never synthesize one.
        if(list && list->size==1 && list->items) {
            const int zone=list->items->m_Items[0];
            RememberDeferredInitialZone(id,zone);
            if (trace) LOG_INFO("CustomComponents/Pick3/DEFERRED_TRIAL: phase=ZONE_RECORDED guid=%d card=%d zone=%d",guid,id,zone);
        }
    }
    if (trace) TrickEffectLogList("BURST_AFTER", guid, id, targetIds);
    trickEffectBurstDepth = oldDepth;
    trickEffectBurstGuid = oldGuid;
    trickEffectBurstEntity = oldEntity;
}
// Read-only target propagation audit. Hooks are scoped to 712/713 and retain stock flow.
using PrimaryPreselectedAuditFn = Obj*(*)(Obj*, Obj*, Obj*, Obj*, const void*);
using PrimaryTargetListAuditFn = Obj*(*)(Obj*, Obj*, Obj*, Obj*, const void*);
using SecondaryContextAuditFn = void(*)(Obj*, Obj*, Obj*, const void*);
inline PrimaryPreselectedAuditFn originalPrimaryPreselectedAudit = nullptr;
inline PrimaryTargetListAuditFn originalPrimaryTargetListAudit = nullptr;
inline SecondaryContextAuditFn originalSecondaryContextAudit = nullptr;
inline thread_local unsigned unearthTargetAuditDepth = 0;
inline unsigned unearthTargetAuditSamples = 0;
inline BNM::Field<int> unearthAuditEffectPrimary;
inline BNM::Field<int> unearthAuditEffectSecondary;
inline BNM::Field<int> unearthAuditReturnOrder;
inline constexpr int kUnearthReturnMarker = 0x6F000003;

inline void UnearthAuditIds(const char* stage, Obj* list) {
    if (!list) { LOG_INFO("CustomComponents/Pick3/UNEARCH_TARGET_FLOW: %s ids=null",stage); return; }
    // Both primary and secondary creator methods take System.Collections.Generic.List<int>.
    auto* ids=reinterpret_cast<IntList*>(list);
    const int count=ids->size;
    if(count<0 || count>128 || (count && !ids->items)) {
        LOG_INFO("CustomComponents/Pick3/UNEARCH_TARGET_FLOW: %s ids=%p count=unreadable",stage,list);
        return;
    }
    LOG_INFO("CustomComponents/Pick3/UNEARCH_TARGET_FLOW: %s count=%d first=%d second=%d third=%d",stage,count,
             count>0?ids->items->m_Items[0]:-1,count>1?ids->items->m_Items[1]:-1,count>2?ids->items->m_Items[2]:-1);
}
// Read-only ownership inventory. Component identity is read from the actual
// effect entity; the shared focus GUID is only a sampling hint, never proof.
inline unsigned unearthOwnerSamples=0;
// Read-only observation of stock-owned context at a synchronous effect-creation
// boundary. No managed invokes, no pooled-list inspection, no retained pointers.
// Offsets match dump.cs: EntityQueryContext(Model 0x10, EffectContext 0x18,
// SelfId 0x20); EffectContext(Target 0x10, Source 0x14, EffectEntity 0x18,
// Springboard 0x1C, EffectData 0x20).
inline unsigned pick3StockContextAuditSamples=0;
inline void AuditPick3StockQueryContext(const char* stage, Obj* effectEntity, Obj* context) {
    if (!context || pick3StockContextAuditSamples >= 96) return;
    ++pick3StockContextAuditSamples;
    const auto* contextBytes = reinterpret_cast<const std::uint8_t*>(context);
    Obj* model = *reinterpret_cast<Obj* const*>(contextBytes + 0x10);
    Obj* effectContext = *reinterpret_cast<Obj* const*>(contextBytes + 0x18);
    const int selfId = *reinterpret_cast<const int*>(contextBytes + 0x20);
    if (!effectContext) {
        LOG_INFO("CustomComponents/Pick3/PICK3_STOCK_QUERY_CONTEXT: phase=%s focusGuid=%d effect=%p ctx=%p model=%p self=%d effectContext=null", stage, trickSecondaryFocusGuid, effectEntity, context, model, selfId);
        return;
    }
    const auto* effectBytes = reinterpret_cast<const std::uint8_t*>(effectContext);
    const int targetId = *reinterpret_cast<const int*>(effectBytes + 0x10);
    const int sourceId = *reinterpret_cast<const int*>(effectBytes + 0x14);
    const int effectEntityId = *reinterpret_cast<const int*>(effectBytes + 0x18);
    const int springboardId = *reinterpret_cast<const int*>(effectBytes + 0x1C);
    Obj* effectData = *reinterpret_cast<Obj* const*>(effectBytes + 0x20);
    LOG_INFO("CustomComponents/Pick3/PICK3_STOCK_QUERY_CONTEXT: phase=%s focusGuid=%d effect=%p ctx=%p model=%p self=%d source=%d target=%d effectEntityId=%d springboard=%d effectData=%p validationModel=%p validationCard=%d modelMatch=%d", stage, trickSecondaryFocusGuid, effectEntity, context, model, selfId, sourceId, targetId, effectEntityId, springboardId, effectData, pick3CurrentValidationModel, pick3CurrentValidationCard, pick3CurrentValidationModel ? (model == pick3CurrentValidationModel ? 1 : 0) : -1);
}
inline void UnearthOwnerAudit(const char* stage, Obj* effect, Obj* context, Obj* result=nullptr) {
    if(!effect || ++unearthOwnerSamples>220) return;
    const auto entityClass=BNM::Class(effect);
    auto componentsField=entityClass.GetField("Components").cast<BNM::Structures::Mono::List<Obj*>*>();
    auto idField=entityClass.GetField("<Id>k__BackingField").cast<int>();
    auto* components=componentsField.IsValid()?componentsField[effect].Get():nullptr;
    const int entityId=idField.IsValid()?idField[effect].Get():-1;
    bool primary=false, secondary=false, revive=false, move=false;
    int componentCount=-1;
    if(components && components->size>=0 && components->size<=128 &&
       (components->size==0 || components->items)) {
        componentCount=components->size;
        for(int i=0;i<components->size;++i) {
            Obj* component=components->items->m_Items[i];
            if(!component) continue;
            auto* cls=BNM::Class(component).GetClass();
            const char* name=(cls && cls->name)?cls->name:"?";
            if(!std::strcmp(name,"PrimaryTargetFilter")) primary=true;
            if(!std::strcmp(name,"SecondaryTargetFilter")) secondary=true;
            if(!std::strcmp(name,"ReturnFromDiscardPileToLaneEffectDescriptor")) revive=true;
            if(!std::strcmp(name,"MoveCardToLanesEffectDescriptor")) move=true;
            if(unearthOwnerSamples<=65)
                LOG_INFO("CustomComponents/Pick3/UNEARCH_OWNER: COMPONENT stage=%s entity=%d idx=%d type=%s",stage,entityId,i,name);
        }
    }
    // Unknown collection layouts are reported, never cast to List<> blindly.
    int resultCount=-1;
    if(result) {
        auto count=BNM::Class(result).GetMethod("get_Count",0).cast<int>();
        if(count.IsValid()) {
            auto ex=BNM::TryInvoke([&]{ resultCount=count[result](); });
            if(ex.IsValid() || resultCount<0 || resultCount>4096) resultCount=-2;
        }
    }
    LOG_INFO("CustomComponents/Pick3/UNEARCH_OWNER: %s entity=%d ptr=%p ctx=%p focusHint=%d components=%d primaryFilter=%d secondaryFilter=%d revive=%d move=%d result=%p resultCount=%d",
             stage,entityId,effect,context,trickSecondaryFocusGuid,componentCount,
             primary?1:0,secondary?1:0,revive?1:0,move?1:0,result,resultCount);
}
// EffectEntityConverter uses an IList<int> (not necessarily a List<int>).
using UnearthConverterFn=Obj*(*)(Obj*,Obj*,Obj*,Obj*,const void*);
inline UnearthConverterFn originalUnearthConverter=nullptr;
inline Obj* UnearthConverterAudit(Obj* self,Obj* effect,Obj* targets,Obj* context,const void* method) {
    const bool scoped=trickSecondaryFocusGuid==712 || trickSecondaryFocusGuid==713 || unearthTargetAuditDepth!=0;
    if(scoped) { AuditPick3StockQueryContext("CONVERTER_ENTER", effect, context); UnearthOwnerAudit("CONVERTER_ENTER",effect,context); }
    Obj* result=originalUnearthConverter?originalUnearthConverter(self,effect,targets,context,method):nullptr;
    if(scoped) { AuditPick3StockQueryContext("CONVERTER_EXIT", effect, context); UnearthOwnerAudit("CONVERTER_EXIT",effect,context,result); }
    return result;
}
inline void UnearthAuditEffect(const char* stage, Obj* effect) {
    if (!effect) { LOG_INFO("CustomComponents/Pick3/UNEARCH_TARGET_FLOW: %s effect=null",stage); return; }
    LOG_INFO("CustomComponents/Pick3/UNEARCH_TARGET_FLOW: %s effect=%p primary=%d secondary=%d",stage,effect,
             unearthAuditEffectPrimary.IsValid()?unearthAuditEffectPrimary[effect].Get():-999,
             unearthAuditEffectSecondary.IsValid()?unearthAuditEffectSecondary[effect].Get():-999);
}
// Uncapped, effect-identity-scoped stock primary creator audit. Read-only:
// it does not change the query context, lists, filter, or return value.
inline bool Pick3RevivePrimaryCreatorScope(Obj* effect, int& entityId) {
    entityId = -1;
    if (!effect) return false;
    static auto idField = BNM::Class("PvZCards.Engine", "Entity")
        .GetField("<Id>k__BackingField").cast<int>();
    if (!idField.IsValid()) return false;
    entityId = idField[effect].Get();
    return entityId >= 0 && IsGenericPick3ReviveEffect(entityId);
}
inline void Pick3RevivePrimaryCreatorContext(const char* stage, Obj* effect,
                                               Obj* context, int effectId) {
    if (!context) {
        LOG_INFO("CustomComponents/Pick3/PICK3_REVIVE_PRIMARY_CREATOR: phase=%s entity=%d effect=%p context=null", stage, effectId, effect);
        return;
    }
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(context);
    Obj* model = *reinterpret_cast<Obj* const*>(bytes + 0x10);
    Obj* effectContext = *reinterpret_cast<Obj* const*>(bytes + 0x18);
    const int self = *reinterpret_cast<const int*>(bytes + 0x20);
    if (!effectContext) {
        LOG_INFO("CustomComponents/Pick3/PICK3_REVIVE_PRIMARY_CREATOR: phase=%s entity=%d effect=%p model=%p self=%d effectContext=null",stage,effectId,effect,model,self);
        return;
    }
    const auto* ec = reinterpret_cast<const std::uint8_t*>(effectContext);
    LOG_INFO("CustomComponents/Pick3/PICK3_REVIVE_PRIMARY_CREATOR: phase=%s entity=%d effect=%p model=%p self=%d source=%d target=%d contextEffect=%d springboard=%d effectData=%p", stage,effectId,effect,model,self,
             *reinterpret_cast<const int*>(ec+0x14), *reinterpret_cast<const int*>(ec+0x10),
             *reinterpret_cast<const int*>(ec+0x18), *reinterpret_cast<const int*>(ec+0x1c),
             *reinterpret_cast<Obj* const*>(ec+0x20));
}
inline void Pick3RevivePrimaryCreatorIds(const char* stage, Obj* targets, int effectId) {
    if (!targets) {
        LOG_INFO("CustomComponents/Pick3/PICK3_REVIVE_PRIMARY_CREATOR: phase=%s entity=%d targets=null",stage,effectId);
        return;
    }
    // This parameter is stock List<int>; read its existing layout, never retain it.
    const auto* ids = reinterpret_cast<const IntList*>(targets);
    const int count=ids->size;
    if (count<0 || count>4096 || (count>0 && !ids->items)) {
        LOG_INFO("CustomComponents/Pick3/PICK3_REVIVE_PRIMARY_CREATOR: phase=%s entity=%d targets=%p count=unreadable",stage,effectId,targets);
        return;
    }
    LOG_INFO("CustomComponents/Pick3/PICK3_REVIVE_PRIMARY_CREATOR: phase=%s entity=%d targets=%p count=%d first=%d second=%d third=%d",stage,effectId,targets,count,
             count>0?ids->items->m_Items[0]:-1,count>1?ids->items->m_Items[1]:-1,count>2?ids->items->m_Items[2]:-1);
}
inline void CaptureStockAutomaticPrimary(Obj* effect,Obj* context,IntList* ids,Obj* result) {
    if(effect && context && ids && result) {
        static auto id=BNM::Class("PvZCards.Engine","Entity").GetField("<Id>k__BackingField").cast<int>();
        Obj* model=TransactionRuntime::ObjectAt(context,0x10);
        const int effectId=id.IsValid()?id[effect].Get():-1;
        const int mode=model?TargetBridge::ReadActiveSelectionMode(effectId,model):-1;
        if(EnumModes::IsLivePickFromThree(mode) && ids && ids->items && ids->size==1 && ids->items->capacity>=1) {
            const int target=ids->items->m_Items[0];
            Obj* card=target>0?TargetBridge::getEntityById[model](target):nullptr;
            const int guid=CustomComponents::Queries::InDeckCardGuidOf(card);
            auto* effects=reinterpret_cast<BNM::Structures::Mono::List<Obj*>*>(result);
            const int source=effects && effects->items && effects->size>0?TransactionRuntime::IntAt(effects->items->m_Items[0],0x24):-1;
            TransactionRuntime::AttachAutomaticEffects(model,effectId,source,target,guid,
                TransactionRuntime::ReadContext(TransactionRuntime::ObjectAt(context,0x18)),result);
        }
    }
 }
inline Obj* UnearthPrimaryPreselectedAudit(Obj* self, Obj* effect, Obj* context, Obj* targets, const void* method) {
    const bool scoped=(trickSecondaryFocusGuid==712 || trickSecondaryFocusGuid==713) && ++unearthTargetAuditSamples<=160;
    int reviveId=-1;
    const bool revive=Pick3RevivePrimaryCreatorScope(effect,reviveId);
    if (revive) { Pick3RevivePrimaryCreatorContext("PRESELECT_ENTER",effect,context,reviveId); Pick3RevivePrimaryCreatorIds("PRESELECT_TARGETS",targets,reviveId); }
    if(scoped) UnearthOwnerAudit("PRESELECT_ENTER",effect,context);
    if(scoped) LOG_INFO("CustomComponents/Pick3/UNEARCH_TARGET_FLOW: PRIMARY_PRESELECT_ENTER effect=%p context=%p primaryTargets=%p",effect,context,targets);
    if(scoped) ++unearthTargetAuditDepth;
    Obj* result=originalPrimaryPreselectedAudit?originalPrimaryPreselectedAudit(self,effect,context,targets,method):nullptr;
    static auto autoIdsField=BNM::Class("PvZCards.Engine.Components","Targets").GetField("TargetEntityIds").cast<IntList*>();
    CaptureStockAutomaticPrimary(effect,context,targets && autoIdsField.IsValid()?autoIdsField[targets].Get():nullptr,result);
    if(revive) LOG_INFO("CustomComponents/Pick3/PICK3_REVIVE_PRIMARY_CREATOR: phase=PRESELECT_EXIT entity=%d result=%p",reviveId,result);
    if(scoped) UnearthOwnerAudit("PRESELECT_EXIT",effect,context,result);
    if(scoped) { --unearthTargetAuditDepth; LOG_INFO("CustomComponents/Pick3/UNEARCH_TARGET_FLOW: PRIMARY_PRESELECT_EXIT effects=%p",result); }
    return result;
}
// Read-only audit: never mutate the preselected primary list. The previous
// substitution produced primary == secondary == selected discard entity, and
// the return-to-lane effect ran before any lane selection.
inline Obj* UnearthPrimaryListAudit(Obj* self, Obj* targets, Obj* effect, Obj* context, const void* method) {
    const bool scoped=(trickSecondaryFocusGuid==712 || trickSecondaryFocusGuid==713 || unearthTargetAuditDepth!=0) && ++unearthTargetAuditSamples<=160;
    int reviveId=-1;
    const bool revive=Pick3RevivePrimaryCreatorScope(effect,reviveId);
    if(revive) { Pick3RevivePrimaryCreatorContext("PRIMARY_LIST_ENTER",effect,context,reviveId); Pick3RevivePrimaryCreatorIds("PRIMARY_LIST_TARGETS",targets,reviveId); }
    if(scoped) UnearthOwnerAudit("PRIMARY_LIST_ENTER",effect,context);
    if(scoped) { LOG_INFO("CustomComponents/Pick3/UNEARCH_TARGET_FLOW: PRIMARY_LIST_ENTER effect=%p context=%p",effect,context); UnearthAuditIds("PRIMARY_LIST_IDS",targets); ++unearthTargetAuditDepth; }
    Obj* result=originalPrimaryTargetListAudit?originalPrimaryTargetListAudit(self,targets,effect,context,method):nullptr;
    CaptureStockAutomaticPrimary(effect,context,reinterpret_cast<IntList*>(targets),result);
    if(revive) LOG_INFO("CustomComponents/Pick3/PICK3_REVIVE_PRIMARY_CREATOR: phase=PRIMARY_LIST_EXIT entity=%d result=%p",reviveId,result);
    if(scoped) UnearthOwnerAudit("PRIMARY_LIST_EXIT",effect,context,result);
    if(scoped) { --unearthTargetAuditDepth; LOG_INFO("CustomComponents/Pick3/UNEARCH_TARGET_FLOW: PRIMARY_LIST_EXIT effects=%p",result); }
    return result;
}
// Trace the stock primary creator's *choice boundary*, including whether the
// Burst preselected path ever enters the normal primary selection creator.
using PrimaryNormalAuditFn = Obj*(*)(Obj*, Obj*, Obj*, const void*);
using PrimaryMethodAuditFn = Obj*(*)(Obj*, Obj*, Obj*, Obj*, Obj*, const void*);
inline PrimaryNormalAuditFn originalPrimaryNormalAudit=nullptr;
inline PrimaryMethodAuditFn originalPrimaryMethodAudit=nullptr;
inline Obj* UnearthPrimaryNormalAudit(Obj* self, Obj* effect, Obj* context, const void* method) {
    const bool scoped=(trickSecondaryFocusGuid==712 || trickSecondaryFocusGuid==713 || unearthTargetAuditDepth!=0) && ++unearthTargetAuditSamples<=160;
    int reviveId=-1;
    const bool revive=Pick3RevivePrimaryCreatorScope(effect,reviveId);
    if(revive) Pick3RevivePrimaryCreatorContext("NORMAL_ENTER",effect,context,reviveId);
    if(scoped) UnearthOwnerAudit("NORMAL_ENTER",effect,context);
    if(scoped) LOG_INFO("CustomComponents/Pick3/UNEARCH_PRIMARY_BRANCH: NORMAL_ENTER effect=%p context=%p",effect,context);
    Obj* result=originalPrimaryNormalAudit?originalPrimaryNormalAudit(self,effect,context,method):nullptr;
    if(revive) LOG_INFO("CustomComponents/Pick3/PICK3_REVIVE_PRIMARY_CREATOR: phase=NORMAL_EXIT entity=%d result=%p",reviveId,result);
    if(scoped) UnearthOwnerAudit("NORMAL_EXIT",effect,context,result);
    if(scoped) LOG_INFO("CustomComponents/Pick3/UNEARCH_PRIMARY_BRANCH: NORMAL_EXIT effects=%p",result);
    return result;
}
inline Obj* UnearthPrimaryMethodAudit(Obj* self, Obj* effect, Obj* context, Obj* targets, Obj* filter, const void* method) {
    const bool scoped=(trickSecondaryFocusGuid==712 || trickSecondaryFocusGuid==713 || unearthTargetAuditDepth!=0) && ++unearthTargetAuditSamples<=160;
    int reviveId=-1;
    const bool revive=Pick3RevivePrimaryCreatorScope(effect,reviveId);
    if(revive) {
        Pick3RevivePrimaryCreatorContext("METHOD_ENTER",effect,context,reviveId);
        LOG_INFO("CustomComponents/Pick3/PICK3_REVIVE_PRIMARY_CREATOR: phase=METHOD_INPUT entity=%d targets=%p filter=%p",reviveId,targets,filter);
    }
    if(scoped) UnearthOwnerAudit("METHOD_ENTER",effect,context);
    if(scoped) LOG_INFO("CustomComponents/Pick3/UNEARCH_PRIMARY_BRANCH: METHOD_ENTER effect=%p context=%p targets=%p filter=%p",effect,context,targets,filter);
    Obj* result=originalPrimaryMethodAudit?originalPrimaryMethodAudit(self,effect,context,targets,filter,method):nullptr;
    if(revive) LOG_INFO("CustomComponents/Pick3/PICK3_REVIVE_PRIMARY_CREATOR: phase=METHOD_EXIT entity=%d result=%p",reviveId,result);
    if(scoped) UnearthOwnerAudit("METHOD_EXIT",effect,context,result);
    if(scoped) LOG_INFO("CustomComponents/Pick3/UNEARCH_PRIMARY_BRANCH: METHOD_EXIT effects=%p",result);
    return result;
}
// Bind only a uniquely matching, still-unbound stock secondary choice.
// Called after the real selection service processes a validated primary.
inline void BindCommittedPick3RevivePrimary(Obj* service, const TargetBridge::ScopedValidationRecipient& recipient);
// Defined below, after the stock queue field bindings are available.
inline void CorrectPick3ReviveRolesBeforeStockSelect(Obj* service, int effectId,
    int groupId, int sourceCard, int selectedCard, int ownerPlayer, bool verified);


// Native ARM64 ABI: SecondaryTargetingData is a 0x20-byte value type passed
// indirectly in x1. x2 is ICollection<Effect>. Do not treat the data as an
// Il2CppObject or assume its IEnumerable<int> is always a List<int>.
inline int Pick3CastingHandOnModel(Obj* model, int player);
struct Pick3SecondaryTargetingData {
    Obj* effectEntity;
    std::int32_t primaryId;
    std::int32_t padding;
    Obj* secondaryIds;
    std::int32_t context;
    std::int32_t tailPadding;
};
static_assert(sizeof(Pick3SecondaryTargetingData) == 0x20);
using Pick3AddEffectsWithSecondaryFn = void(*)(Obj*, Pick3SecondaryTargetingData*, Obj*, const void*);
inline Pick3AddEffectsWithSecondaryFn originalPick3AddEffectsWithSecondary = nullptr;
struct Pick3SecondaryCommitScope {
    int effect = -1, group = -1, source = -1, chosen = -1;
    Obj* model = nullptr;
    int player = -1;
};
inline thread_local Pick3SecondaryCommitScope pick3SecondaryCommitScope{};
struct Pick3SecondaryCommitGuard {
    Pick3SecondaryCommitScope old;
    explicit Pick3SecondaryCommitGuard(const Pick3SecondaryCommitScope& next)
        : old(pick3SecondaryCommitScope) { pick3SecondaryCommitScope = next; }
    ~Pick3SecondaryCommitGuard() { pick3SecondaryCommitScope = old; }
};
// The stock effect creator copies data.primaryId directly into Effect.TargetId
// (libil2cpp RVA 0x2216F78). Correct only when a single exact selection is
// verified AND the incoming secondary IDs are demonstrably lanes, not cards.
// If stock is still supplying the discarded card as a secondary target, do
// not produce a self-targeted revive; log the missing lane handoff instead.
inline void Pick3AddEffectsWithSecondary(Obj* self, Pick3SecondaryTargetingData* data,
                                         Obj* effects, const void* method) {
    const auto scope = pick3SecondaryCommitScope;
    // 1.7.24: observe the existing hook *before* any commit-scope filtering.
    // Do not dereference the effect entity without the exact scope and a valid
    // metadata field. No new detour, command ownership, or gameplay changes.
#if CUSTOMCOMPONENTS_PF3_REPAIR_TRACE
    static thread_local unsigned creatorSamples1724 = 0;
    const bool traceCreator1724 = creatorSamples1724 < 64;
    if (traceCreator1724) {
        ++creatorSamples1724;
        LOG_INFO("CustomComponents/Pick3/PREPLAY1724: stage=HOOK_ENTER scopeEffect=%d scopeGroup=%d scopeSource=%d chosen=%d dataPresent=%d entityPresent=%d originalPrimary=%d secondaryPresent=%d",
            scope.effect, scope.group, scope.source, scope.chosen,
            int(data != nullptr), int(data && data->effectEntity != nullptr),
            data ? data->primaryId : -1, int(data && data->secondaryIds != nullptr));
    }
    const char* creatorReason1724 = "NO_COMMIT_SCOPE";
    int creatorDataEffect1724 = -1;
    if (scope.effect > 0)
        creatorReason1724 = (!data || !data->effectEntity) ? "NO_EFFECT_DATA" : "EFFECT_UNAVAILABLE";
#endif
    if (scope.effect > 0 && data && data->effectEntity) {
        static auto entityIdField = BNM::Class("PvZCards.Engine", "Entity")
            .GetField("<Id>k__BackingField").cast<int>();
        const int dataEffect = entityIdField.IsValid() ? entityIdField[data->effectEntity].Get() : -1;
#if CUSTOMCOMPONENTS_PF3_REPAIR_TRACE
        creatorDataEffect1724 = dataEffect;
        creatorReason1724 = !entityIdField.IsValid() ? "EFFECT_ID_UNAVAILABLE" :
            (dataEffect != scope.effect ? "EFFECT_MISMATCH" :
            (!IsGenericPick3ReviveEffect(dataEffect) ? "NON_REVIVE_EFFECT" : "MATCHED"));
#endif
        if (dataEffect == scope.effect && IsGenericPick3ReviveEffect(dataEffect)) {
            int selected = -1, guid = -1;
            const bool verified = Picked::TryGetSingleLiveEntity(scope.effect, scope.group,
                selected, guid) && selected == scope.chosen;
            int listCount = -1;
            bool laneIds = false;
            bool listsChosenCard = false;
            // IEnumerable<int> can be an iterator: inspect only a confirmed
            // concrete List<int>. Never reinterpret arbitrary enumerable data.
            if (data->secondaryIds) {
                const auto* klass = BNM::Class(data->secondaryIds).GetClass();
                if (klass && klass->name && !std::strcmp(klass->name, "List`1")) {
                    auto* ids = reinterpret_cast<const IntList*>(data->secondaryIds);
                    if (ids->size >= 0 && ids->size <= 64 &&
                        (ids->size == 0 || ids->items)) {
                        listCount = ids->size;
                        laneIds = listCount > 0 && scope.model &&
                            TargetBridge::getEntityById.IsValid() && TargetBridge::getComponent.IsValid();
                        static auto laneClass = BNM::Class("PvZCards.Engine.Components", "Lane");
                        for (int i = 0; i < listCount; ++i) {
                            const int id = ids->items->m_Items[i];
                            if (id == scope.chosen) listsChosenCard = true;
                            Obj* entity = nullptr;
                            Obj* lane = nullptr;
                            if (laneIds) {
                                auto err = BNM::TryInvoke([&]{
                                    entity = TargetBridge::getEntityById[scope.model](id);
                                });
                                if (!err.IsValid() && entity && laneClass.IsValid()) {
                                    err = BNM::TryInvoke([&]{
                                        lane = TargetBridge::getComponent[entity](laneClass.GetMonoType());
                                    });
                                }
                                if (err.IsValid() || !lane) laneIds = false;
                            }
                        }
                    }
                }
            }
            const int oldPrimary = data->primaryId;
            const int initialZone = Pick3VerifiedCastTargetFor(scope.model,scope.source,scope.effect);
            const bool zoneVerified = initialZone >= 0 && oldPrimary == initialZone;
            const auto provenance = RevivePairContract1723::Classify(
                oldPrimary, selected, initialZone);
            // Read-only provenance: scoped to the already existing stock creator
            // detour. No extra hooks, no pooled-object retention, no ID rewrite.
            // Never interpret an ALREADY_CHOSEN primary as a casting zone.
#if CUSTOMCOMPONENTS_PF3_REPAIR_TRACE
            static thread_local unsigned provenanceSamples = 0;
            if (provenanceSamples < 48) {
                ++provenanceSamples;
                LOG_INFO("CustomComponents/Pick3/PREPLAY1723: stage=SECONDARY_PRIMARY_PROVENANCE effect=%d group=%d source=%d player=%d original=%d chosen=%d verifiedCast=%d origin=%s selectedVerified=%d secondaryCount=%d lanesVerified=%d includesChosen=%d model=%p",
                    scope.effect, scope.group, scope.source, scope.player,
                    oldPrimary, scope.chosen, initialZone,
                    RevivePairContract1723::Name(provenance), int(verified),
                    listCount, int(laneIds), int(listsChosenCard), (void*)scope.model);
            }
#endif
            PF3_REPAIR_TRACE("CustomComponents/Pick3/REPAIR1710: SECONDARY_CREATOR_CONTEXT effect=%d group=%d source=%d player=%d model=%p validatedCast=%d original=%d chosen=%d verified=%d lanes=%d",
                scope.effect,scope.group,scope.source,scope.player,(void*)scope.model,initialZone,oldPrimary,selected,int(verified),int(laneIds));
            const bool canCorrect = verified && zoneVerified && laneIds &&
                !listsChosenCard && oldPrimary != selected;
            if (canCorrect) data->primaryId = selected;
            LOG_INFO("CustomComponents/Pick3/PICK3_REVIVE_SECONDARY_CREATOR: action=%s effect=%d group=%d source=%d chosen=%d dataPrimaryBefore=%d dataPrimaryAfter=%d zone=%d verified=%d laneList=%d secondaryCount=%d includesChosen=%d collection=%p",
                canCorrect ? "CORRECTED" : "SKIP", dataEffect, scope.group,
                scope.source, scope.chosen, oldPrimary, data->primaryId, initialZone,
                int(verified), int(laneIds), listCount, int(listsChosenCard), effects);
        }
    }
#if CUSTOMCOMPONENTS_PF3_REPAIR_TRACE
    if (traceCreator1724)
        LOG_INFO("CustomComponents/Pick3/PREPLAY1724: stage=HOOK_CLASSIFY reason=%s scopeEffect=%d dataEffect=%d scopeGroup=%d beforeStockPrimary=%d",
            creatorReason1724, scope.effect, creatorDataEffect1724, scope.group,
            data ? data->primaryId : -1);
#endif
    if (originalPick3AddEffectsWithSecondary)
        originalPick3AddEffectsWithSecondary(self, data, effects, method);
#if CUSTOMCOMPONENTS_PF3_REPAIR_TRACE
    if (traceCreator1724)
        LOG_INFO("CustomComponents/Pick3/PREPLAY1724: stage=CREATOR_RETURN reason=%s scopeEffect=%d dataEffect=%d primaryAfter=%d stockCalled=%d",
            creatorReason1724, scope.effect, creatorDataEffect1724,
            data ? data->primaryId : -1, int(originalPick3AddEffectsWithSecondary != nullptr));
#endif
}

inline void UnearthSecondaryContextAudit(Obj* self, Obj* data, Obj* effect, const void* method) {
    const bool scoped=(trickSecondaryFocusGuid==712 || trickSecondaryFocusGuid==713 || unearthTargetAuditDepth!=0) && ++unearthTargetAuditSamples<=160;
    if(scoped) { LOG_INFO("CustomComponents/Pick3/UNEARCH_TARGET_FLOW: CONTEXT_SET_ENTER data=%p",data); UnearthAuditEffect("CONTEXT_BEFORE",effect); }
    if(originalSecondaryContextAudit) originalSecondaryContextAudit(self,data,effect,method);
    // Read-only at this boundary. The target-role correction now happens
    // before the stock selection service is allowed to advance its queue.
    // Mutating here was unreliable: the selection can have been consumed.
    if(scoped) UnearthAuditEffect("CONTEXT_AFTER",effect);
}

// Stock secondary-choice decision audit for the 712/713 Unearth handoff.
// The two ShouldAdd overloads have different ABIs. No game state is changed.
using SecondaryChooseDecision3Fn = bool(*)(Obj*, int, Obj*, Obj*, const void*);
using SecondaryChooseDecision2Fn = bool(*)(Obj*, Obj*, Obj*, const void*);
using AddSecondaryChoiceFn = void(*)(Obj*, Obj*, Obj*, Obj*, const void*);
using CreateTargetChoiceFn = Obj*(*)(Obj*, Obj*, int, Obj*, const void*);
inline SecondaryChooseDecision3Fn originalSecondaryChooseDecision3 = nullptr;
inline SecondaryChooseDecision2Fn originalSecondaryChooseDecision2 = nullptr;
inline AddSecondaryChoiceFn originalAddSecondaryChoice = nullptr;
inline CreateTargetChoiceFn originalCreateTargetChoice = nullptr;
inline thread_local unsigned unearthSecondaryDecisionDepth = 0;
inline unsigned unearthSecondaryDecisionSamples = 0;
inline bool UnearthChoiceAuditEnabled() {
    return trickSecondaryFocusGuid == 712 || trickSecondaryFocusGuid == 713 || unearthTargetAuditDepth != 0;
}
inline int UnearthAuditListCount(Obj* list) {
    if (!list) return -1;
    const auto* ids = reinterpret_cast<const IntList*>(list);
    return (ids->size >= 0 && ids->size <= 4096) ? ids->size : -2;
}
inline bool UnearthChooseDecision3(Obj* self, int primaryId, Obj* filter, Obj* targets, const void* method) {
    const bool scoped = UnearthChoiceAuditEnabled() && ++unearthSecondaryDecisionSamples <= 240;
    if(scoped) LOG_INFO("CustomComponents/Pick3/UNEARCH_CHOOSE_DECISION: ENTER overload=3 primary=%d filter=%p secondaryCount=%d", primaryId, filter, UnearthAuditListCount(targets));
    if(scoped) ++unearthSecondaryDecisionDepth;
    const bool result=originalSecondaryChooseDecision3 ? originalSecondaryChooseDecision3(self,primaryId,filter,targets,method) : false;
    if(scoped) { --unearthSecondaryDecisionDepth; LOG_INFO("CustomComponents/Pick3/UNEARCH_CHOOSE_DECISION: EXIT overload=3 primary=%d secondaryCount=%d choose=%d",primaryId,UnearthAuditListCount(targets),result?1:0); }
    return result;
}
inline bool UnearthChooseDecision2(Obj* self, Obj* filter, Obj* targets, const void* method) {
    const bool scoped = (UnearthChoiceAuditEnabled() || unearthSecondaryDecisionDepth!=0) && ++unearthSecondaryDecisionSamples <= 240;
    if(scoped) LOG_INFO("CustomComponents/Pick3/UNEARCH_CHOOSE_DECISION: ENTER overload=2 filter=%p targetCount=%d",filter,UnearthAuditListCount(targets));
    const bool result=originalSecondaryChooseDecision2 ? originalSecondaryChooseDecision2(self,filter,targets,method) : false;
    if(scoped) LOG_INFO("CustomComponents/Pick3/UNEARCH_CHOOSE_DECISION: EXIT overload=2 targetCount=%d choose=%d",UnearthAuditListCount(targets),result?1:0);
    return result;
}
// Unbounded, structurally scoped secondary-choice boundary audit.
// Deliberately do NOT defer stock effects here: there is no verified resume
// operation for the original queue or guaranteed matching target context yet.
// This diagnostic never retains objects or dereferences pooled lists.
inline bool Pick3IsReviveChoiceEffect(Obj* effectEntity) {
    if (!effectEntity) return false;
    auto id = BNM::Class("PvZCards.Engine", "Entity")
        .GetField("<Id>k__BackingField").cast<int>();
    if (!id.IsValid()) return false;
    const int effectId = id[effectEntity].Get();
    return effectId >= 0 && IsGenericPick3ReviveEffect(effectId);
}
inline thread_local unsigned pick3SecondaryChoiceDepth = 0;
inline void UnearthAddSecondaryChoice(Obj* self, Obj* effectEntity, Obj* allEffects, Obj* context, const void* method) {
    const bool scoped = Pick3IsReviveChoiceEffect(effectEntity);
    if (scoped) {
        LOG_INFO("CustomComponents/Pick3/PICK3_REVIVE_CHOICE_BOUNDARY: phase=ADD_ENTER effect=%p context=%p collection=%p primaryCommit=unknown continuation=stock", effectEntity, context, allEffects);
        ++pick3SecondaryChoiceDepth;
    }
    if (originalAddSecondaryChoice) originalAddSecondaryChoice(self,effectEntity,allEffects,context,method);
    if (scoped) {
        --pick3SecondaryChoiceDepth;
        LOG_INFO("CustomComponents/Pick3/PICK3_REVIVE_CHOICE_BOUNDARY: phase=ADD_EXIT effect=%p context=%p collection=%p continuation=stock", effectEntity,context,allEffects);
    }
}
inline Obj* UnearthCreateTargetChoice(Obj* self, Obj* effectEntity, int phase, Obj* context, const void* method) {
    const bool scoped = pick3SecondaryChoiceDepth != 0 || Pick3IsReviveChoiceEffect(effectEntity);
    if(scoped) LOG_INFO("CustomComponents/Pick3/PICK3_REVIVE_CHOICE_BOUNDARY: phase=CREATE_ENTER effect=%p choicePhase=%d context=%p nested=%u",effectEntity,phase,context,pick3SecondaryChoiceDepth);
    Obj* result=originalCreateTargetChoice ? originalCreateTargetChoice(self,effectEntity,phase,context,method) : nullptr;
    if(scoped) LOG_INFO("CustomComponents/Pick3/PICK3_REVIVE_CHOICE_BOUNDARY: phase=CREATE_EXIT effect=%p choicePhase=%d result=%p",effectEntity,phase,result);
    return result;
}

inline Obj* TrickFindSecondaryTrace(Obj* self, Obj* effect, Obj* context, const void* method) {
    const bool logging = trickSecondaryFocusGuid == 699 || trickSecondaryFocusGuid == 712 || trickSecondaryFocusGuid == 713 || trickSecondaryFocusGuid == 27;
    if (logging) LOG_INFO("CustomComponents/Pick3/TRICK_SECONDARY: boundary=FIND_ENTER focusGuid=%d effect=%p context=%p stock=1", trickSecondaryFocusGuid, effect, context);
    Obj* result = originalFindSecondaryTrace ? originalFindSecondaryTrace(self, effect, context, method) : nullptr;
    if (logging) LOG_INFO("CustomComponents/Pick3/TRICK_SECONDARY: boundary=FIND_EXIT focusGuid=%d effect=%p list=%p count=opaque stockReturned=1", trickSecondaryFocusGuid, effect, result);
    return result;
}
inline Obj* TrickCreateSecondaryTrace(Obj* self, Obj* targets, Obj* effect, Obj* context, const void* method) {
    const bool logging = trickSecondaryFocusGuid == 699 || trickSecondaryFocusGuid == 712 || trickSecondaryFocusGuid == 713 || trickSecondaryFocusGuid == 27;
    if(logging) { AuditPick3StockQueryContext("SECONDARY_ENTER", effect, context); UnearthOwnerAudit("SECONDARY_ENTER",effect,context); }
    if (logging) { LOG_INFO("CustomComponents/Pick3/TRICK_SECONDARY: boundary=CREATE_ENTER focusGuid=%d targets=%p effect=%p context=%p stock=1", trickSecondaryFocusGuid, targets, effect, context); UnearthAuditIds("SECONDARY_CREATOR_PRIMARY_IDS",targets); }
    Obj* result = originalCreateSecondaryTrace ? originalCreateSecondaryTrace(self, targets, effect, context, method) : nullptr;
    if(logging) { AuditPick3StockQueryContext("SECONDARY_EXIT", effect, context); UnearthOwnerAudit("SECONDARY_EXIT",effect,context,result); }
    if (logging) LOG_INFO("CustomComponents/Pick3/TRICK_SECONDARY: boundary=CREATE_EXIT focusGuid=%d effects=%p count=opaque stockReturned=1", trickSecondaryFocusGuid, result);
    return result;
}

inline void TrickLogIdentity(const char* stage, unsigned seq, int id);
inline void TrickLogTargetsIdentity(const char* stage, unsigned seq, IntList* list);
inline void TrickLogIntList(const char* stage, unsigned seq, int cardId, IntList* list) {
    if (!list) {
        LOG_INFO("CustomComponents/Pick3/TRICK_TARGET_IDS: phase=%s seq=%u card=%d list=null", stage, seq, cardId);
        return;
    }
    const int count = list->size;
    if (count < 0 || count > 4096 || (count && !list->items)) {
        LOG_INFO("CustomComponents/Pick3/TRICK_TARGET_IDS: phase=%s seq=%u card=%d invalidListCount=%d", stage, seq, cardId, count);
        return;
    }
    char ids[240] = {};
    size_t length = 0;
    const int maxPrint = std::min(count, 24);
    for (int i=0; i<maxPrint; ++i) {
        int added = snprintf(ids+length, sizeof(ids)-length, "%s%d", i ? "," : "", list->items->m_Items[i]);
        if (added <= 0 || size_t(added) >= sizeof(ids)-length) break;
        length += size_t(added);
    }
    LOG_INFO("CustomComponents/Pick3/TRICK_TARGET_IDS: phase=%s seq=%u card=%d count=%d ids=[%s]%s", stage, seq, cardId, count, ids, count>maxPrint?" truncated=1":"");
}
inline void TrickStartDragTrace(Obj* self, const void* method) {
    const unsigned seq = ++trickStartObservations;
    if (seq <= 100) LOG_INFO("CustomComponents/Pick3/TRICK_DRAG_LIFE: phase=START_ENTER seq=%u card=%d", seq, self ? *reinterpret_cast<const int*>(reinterpret_cast<const std::uint8_t*>(self)+0x48) : -1);
    if (originalTrickStartDrag) originalTrickStartDrag(self, method);
    if (seq <= 100) LOG_INFO("CustomComponents/Pick3/TRICK_DRAG_LIFE: phase=START_EXIT seq=%u", seq);
}
inline void TrickUpdateDragTrace(Obj* self, const void* method) {
    if (originalTrickUpdateDrag) originalTrickUpdateDrag(self, method);
    const unsigned seq=++trickUpdateObservations;
    if (seq <= 15 || seq%80==0) {
        Obj* model = self ? *reinterpret_cast<Obj* const*>(reinterpret_cast<const std::uint8_t*>(self)+0x50) : nullptr;
        const int cardId = model && trickDraggedCardField.IsValid() ? trickDraggedCardField[model].Get() : -1;
        LOG_INFO("CustomComponents/Pick3/TRICK_DRAG_LIFE: phase=UPDATE seq=%u card=%d", seq, cardId);
    }
}
inline int TrickTopDraggedTrace(Obj* self, Obj* overlapped, int cardId, const void* method) {
    const unsigned seq = ++trickResolveObservations;
    const unsigned oldDepth = trickCacheResolveDepth;
    const unsigned oldSeq = trickCacheProbeSeq;
    const int oldCard = trickCacheProbeCard;
    const int oldCount = trickCacheResultCount;
    const int oldFirst = trickCacheFirstResult;
    const bool focus = seq <= 180 && ++trickCacheResolveSamples <= 180;
    if (focus) {
        ++trickCacheResolveDepth;
        trickCacheProbeSeq = seq;
        trickCacheProbeCard = cardId;
        trickCacheResultCount = -2;
        trickCacheFirstResult = -2;
    }
    const int result = originalTrickTopDragged ? originalTrickTopDragged(self, overlapped, cardId, method) : -1;
    if (focus) {
        const char* path = trickCacheFirstResult != -2 ? "first-valid" :
                           trickCacheResultCount == -1 ? "null-cache-fallback" :
                           trickCacheResultCount == -2 ? "cache-lookup-not-observed" :
                           "fallback-or-unobserved-helper";
        LOG_INFO("CustomComponents/Pick3/TRICK_CACHE: phase=RESOLVE seq=%u dragSeq=%u card=%d cacheCount=%d firstValid=%d final=%d path=%s", seq,trickActiveDragSeq,cardId,trickCacheResultCount,trickCacheFirstResult,result,path);
    }
    trickCacheResolveDepth = oldDepth;
    trickCacheProbeSeq = oldSeq;
    trickCacheProbeCard = oldCard;
    trickCacheResultCount = oldCount;
    trickCacheFirstResult = oldFirst;
    if (seq <= 180) {
        LOG_INFO("CustomComponents/Pick3/TRICK_OVERLAP: seq=%u activeDrag=%u card=%d resolved=%d", seq, trickActiveDragSeq, cardId, result);
        LOG_INFO("CustomComponents/Pick3/TRICK_OVERLAP: seq=%u enumerable=%p note=not-assumed-List", seq, overlapped);
        TrickLogIdentity("OVERLAP_RESOLVED", trickActiveDragSeq, result);
    }
    return result;
}

// These are read-only native field snapshots from the matching IL2CPP dump:
// StopCardDragCommand.ReleasePosition at 0x40 and EntityGestureData at 0x10.
// The pooled stock target list remains opaque and is neither retained nor freed.
inline void TrickStopDragTrace(Obj* self, const void* method) {
    const unsigned seq = ++trickDragObservations;
    Obj* model = self && trickDragModelField.IsValid() ? trickDragModelField[self].Get() : nullptr;
    const int cardId = model && trickDraggedCardField.IsValid() ? trickDraggedCardField[model].Get() : -1;
    const int inHand = model && trickInHandField.IsValid() ? int(trickInHandField[model].Get()) : -1;
    float x = 0, y = 0, z = 0;
    if (self) {
        const auto* release = reinterpret_cast<const float*>(reinterpret_cast<const std::uint8_t*>(self) + 0x40);
        x = release[0]; y = release[1]; z = release[2];
    }
    if (seq <= 160)
        LOG_INFO("CustomComponents/Pick3/TRICK_PLACE_DIAG: STOP_DRAG_ENTER seq=%u cardEntity=%d inHand=%d release=(%.3f,%.3f,%.3f) dragModel=%p stock=1",
                 seq, cardId, inHand, double(x), double(y), double(z), model);
    // CRASH_ISO: disable the IL2CPP GetComponent identity probe during StopDrag.
    // The two observed crashes occur after STOP_DRAG_ENTER and inside BNM::TryInvoke.
    // Preserve stock drag and all gameplay target-selection paths.
    // if (seq <= 160) TrickLogIdentity("DRAGGED", seq, cardId);
    if (seq <= 160 && model && trickTargetIdsField.IsValid()) {
        auto* ids=trickTargetIdsField[model].Get();
        TrickLogIntList("STOP_BEFORE", seq, cardId, ids);
        // CRASH_ISO: also suppress per-valid-target GetComponent probes at this boundary.
        // TrickLogTargetsIdentity("VALID_TARGET", seq, ids);
    }
    // The stock drag target list can contain discarded-card options, NOT the
    // initial casting target. Never require it to equal the zone in a verified
    // initial Play. Match an exact, recent stock-generated Play instead.
    const int previousDropCard=deferredDropCard;
    const int previousDropZone=deferredDropZone;
    const unsigned previousDropSeq=deferredDropSeq;
    deferredDropCard=-1; deferredDropZone=-1; deferredDropSeq=0;
    // Only the exact recent stock-decider proof permits using its rooted
    // EntityModel for card identity. DragModel itself is NOT EntityModel.
    const bool proofCard = cardId >= 0 && deferredValidatedPlay.card == cardId;
    ScopedReviveDragLookup dragLookup(proofCard);
    if (kDeferredPick3TrialEnabled && model && trickTargetIdsField.IsValid()) {
        auto* ids=trickTargetIdsField[model].Get();
        const bool listValid = ids && ids->size>0 && ids->size<=128 && ids->items;
        const bool sourceMatches = cardId>=0 && deferredValidatedPlay.card==cardId &&
            deferredValidatedPlay.guid==TrickGenerationGuid(cardId);
        const int zone=sourceMatches ? deferredValidatedPlay.zone : -1;
        const bool generatedMatch=zone>=0 && FindDeferredInitialZone(cardId)==zone;
        const bool fresh=sourceMatches && IsRecentValidatedInitialPlay(cardId,zone);
        // An exact initial-zone target list is fine; otherwise the list must
        // contain at least one actual card entity (not a fabricated lane ID).
        bool candidateList=false;
        if(listValid) {
            for(int i=0; i<ids->size && i<128; ++i) {
                const int targetId=ids->items->m_Items[i];
                if (targetId==zone || (targetId>=0 && TrickGenerationGuid(targetId)>0)) {
                    candidateList=true;
                    break;
                }
            }
        }
        const bool arm=sourceMatches && fresh && generatedMatch && listValid &&
            candidateList && inHand==0;
        PF3_REPAIR_TRACE("CustomComponents/Pick3/REPAIR1713: DRAG_MODEL drag=%u dragModel=%p card=%d provenCard=%d proofRoot=%p retained=%p guid=%d proofAgeValid=%d",
            seq,(void*)model,cardId,proofCard?1:0,(void*)Target(reviveLegalPlayModelRoot),
            (void*)Target(boundEntityModelRoot),TrickGenerationGuid(cardId),fresh?1:0);
        PF3_REPAIR_TRACE("CustomComponents/Pick3/REPAIR1712: DROP_PREFLIGHT drag=%u card=%d guid=%d inHand=%d zone=%d generated=%d stockPlay=%d ageValid=%d targetCount=%d targetListValid=%d candidateList=%d arm=%d",
            seq,cardId,cardId>=0?TrickGenerationGuid(cardId):-1,inHand,zone,
            cardId>=0?FindDeferredInitialZone(cardId):-1,sourceMatches?1:0,
            fresh?1:0,ids?ids->size:-1,listValid?1:0,candidateList?1:0,arm?1:0);
        if(arm) {
            deferredDropCard=cardId;
            deferredDropZone=zone;
            deferredDropSeq=seq;
            PF3_REPAIR_TRACE("CustomComponents/Pick3/REPAIR1712: DROP_ARM card=%d zone=%d drag=%u source=stockGeneratedPlay effect=%d",
                cardId,zone,seq,deferredValidatedPlay.effect);
        }
    }
    const unsigned previousSeq = trickActiveDragSeq;
    trickActiveDragSeq = seq;
    PrePlayLifecycleTrace::OnStopDrag(cardId,false);
    if (originalTrickStopDrag) originalTrickStopDrag(self, method);
    PrePlayLifecycleTrace::OnStopDrag(cardId,true);
    if (seq <= 160 && model && trickTargetIdsField.IsValid())
        TrickLogIntList("STOP_AFTER", seq, cardId, trickTargetIdsField[model].Get());
    trickActiveDragSeq = previousSeq;
    deferredDropCard=previousDropCard;
    deferredDropZone=previousDropZone;
    deferredDropSeq=previousDropSeq;
    if (seq <= 160)
        LOG_INFO("CustomComponents/Pick3/TRICK_PLACE_DIAG: STOP_DRAG_EXIT seq=%u cardEntity=%d stockReturned=1", seq, cardId);
}
inline void TrickLogGeneratedTargets(int player, int cardId, Obj* result);
inline Obj* TrickValidTargetsTrace(Obj* self, int playerId, int cardId, Obj* validator, const void* method) {
    Obj* result = originalTrickValidTargets ? originalTrickValidTargets(self, playerId, cardId, validator, method) : nullptr;
    TrickLogGeneratedTargets(playerId, cardId, result);
    const unsigned seq = ++trickTargetObservations;
    if (seq <= 240)
        LOG_INFO("CustomComponents/Pick3/TRICK_PLACE_DIAG: VALID_TARGETS seq=%u activeDrag=%u player=%d cardEntity=%d validator=%p list=%p count=unknown stockUnmodified=1",
                 seq, trickActiveDragSeq, playerId, cardId, validator, result);
    return result;
}

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
inline Execute originalSelectTargetAction = nullptr;
inline unsigned int suppressedStockSelectionActions = 0;
// PvP submission is locally predicted before remote state is observed.
// This ticket is *observational*: never synthesize an ACK or mutate gameplay.
inline bool pendingRemoteRetirement = false;
inline int remoteRetirementEffect = -1, remoteRetirementGroup = -1;
inline int remoteRetirementPolls = 0;
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
// Stock-protocol completion and remote-state repairs are ALWAYS enabled.
// Only NETTRACE output remains ADB-controlled. No extra Play is submitted.
inline BNM::Class effectCollectionInterface, effectQueueConcrete, boundEntityModelClass;
inline BNM::Class chooseBlockingClass;
inline BNM::Field<BNM::Structures::Mono::List<Obj*>*> boundQueueEffects, boundQueueResponses, boundQueueGroupResponses;
inline BNM::Field<Obj*> localServiceFinder;
inline std::uint32_t boundSignalRoot=0, boundEffectsRoot=0, boundEntityModelRoot=0;
inline std::uint32_t reviveLegalPlayModelRoot=0; // rooted and reset on match/binder replacement
inline unsigned int repairDependencyPoll=0, repairSuccesses=0, remotePickRebuilds=0;
inline bool pendingRefresh=false;
// Default-on: the stock playable-card highlight command normally runs when
// GameInputState changes. A custom picker can finish without triggering that
// command until the next hand interaction. Refresh the stock highlights once
// AFTER the targeting phase returns to a normal hand-input state.
inline BNM::Class highlightPlayableCardsClass;
inline BNM::Method<void> executeHighlightPlayableCards;
inline BNM::Field<int> highlightCommandInputState, holderInputState;
inline BNM::Field<Obj*> highlightCommandPlayCache, highlightCommandModel,
    highlightCommandFacade, highlightCommandPlayable, highlightCommandUnplayable;
inline bool pendingHandHighlights=false;
inline int pendingHandHighlightAge=0, pendingHandHighlightStable=0;
inline unsigned int handHighlightRefreshCount=0;
inline int pendingRefreshEffect=-1, pendingRefreshGroup=-1, pendingStableFrames=0, pendingRefreshAge=0;
inline unsigned int pendingStockSuccessBaseline=0;
inline void (*originalLocalSelectProcess)(Obj*, Obj*, const void*) = nullptr;
inline void PumpDefaultRepair();
inline void PumpHandHighlights(Obj* holder);
inline void ResolveRepairDependencies(bool force=false);
inline bool EffectAbsentFromQueue(Obj* queue, int effect, int group, bool& safe);

inline BNM::Method<Obj*> getInjectionBinder, getInjectedInstance;
inline BNM::Field<int> localPlayerField, playType, playPlayer, playCard, playOrder;
inline BNM::Field<IntList*> playTargets;
inline BNM::Field<Obj*> commandModel, commandService, playCardCommandModel;
// Read-only local service queue verification. The game reports
// ServiceFailure::NoBlockingEffect (16) if Process() has no active front.
inline BNM::Field<Obj*> localServiceEffects;
inline BNM::Method<Obj*> peekLocalEffect;
inline BNM::Field<int> liveEffectEntityId, liveEffectGroupId;
// Stock model-side target commitment observer. The ARM64 handler at
// 0x21FE8C8 dispatches to CreateEffectsForTargets (0x21FE910), which
// updates the effect entity and regenerates downstream effects. This is
// distinct from the local SelectTarget service completion callback.
using Pick3ModelSelectFn = void(*)(Obj*, Obj*, const void*);
inline Pick3ModelSelectFn originalPick3ModelSelect = nullptr;
inline bool ValidateAcceptedSelectionCards(Obj* model,const std::vector<Picked::SelectedCard>& cards) {
    if(!model || cards.empty() || cards.size()>3 || !TargetBridge::getEntityById.IsValid()) return false;
    static auto identityField=BNM::Class("PvZCards.Engine","Entity").GetField("<Id>k__BackingField").cast<int>();
    if(!identityField.IsValid()) return false;
    for(const auto& card:cards) {
        if(card.entityId==-1) {if(card.cardGuid<=0) return false;continue;}
        if(card.entityId<=0 || card.cardGuid<=0) return false;
        Obj* entity=TargetBridge::getEntityById[model](card.entityId);
        const int guid=CustomComponents::Queries::InDeckCardGuidOf(entity);
        const int actualId=entity?identityField[entity].Get():-1;
        if(!MatchesPhysicalSelection(card.entityId,card.cardGuid,actualId,guid)) {
            LOG_WARN("CustomComponents/Pick3/PF3_TX: phase=ACCEPTED_CARD_MISMATCH entity=%d expectedGuid=%d actualGuid=%d",card.entityId,card.cardGuid,guid);
            return false;
        }
    }
    return true;
}
inline void Pick3ModelSelectHook(Obj* self, Obj* selected, const void* method) {
    int effectId = -1, groupId = -1, phase = -1, targetCount = -1, first = -1;
    bool revive = false;
    if (selected && liveEffectEntityId.IsValid() && liveEffectGroupId.IsValid()) {
        effectId = liveEffectEntityId[selected].Get();
        groupId = liveEffectGroupId[selected].Get();
        revive = effectId >= 0 && IsGenericPick3ReviveEffect(effectId);
        if (revive) {
            const auto* bytes = reinterpret_cast<const std::uint8_t*>(selected);
            phase = *reinterpret_cast<const std::int32_t*>(bytes + 0x50);
            Obj* targets = *reinterpret_cast<Obj* const*>(bytes + 0x48);
            if (targets) {
                const auto* list = reinterpret_cast<const IntList*>(targets);
                if (list->size >= 0 && list->size <= 32 &&
                    (list->size == 0 || list->items)) {
                    targetCount = list->size;
                    if (targetCount > 0) first = list->items->m_Items[0];
                }
            }
            LOG_INFO("CustomComponents/Pick3/PICK3_REVIVE_MODEL_COMMIT: phase=ENTER effect=%d group=%d selectPhase=%d count=%d first=%d system=%p selection=%p",effectId,groupId,phase,targetCount,first,self,selected);
        }
    }
    // Observe stock SelectTargetEffect state across the already-owned detour.
    // Read-only; this cannot by itself prove an authoritative commit.
    const auto snapshotSelect=[&](const char* stage) {
        int state=-1, count=-1, firstTarget=-1;
        if(selected) {
            const auto* bytes=reinterpret_cast<const std::uint8_t*>(selected);
            state=*reinterpret_cast<const std::int32_t*>(bytes+0x50);
            Obj* listObj=*reinterpret_cast<Obj* const*>(bytes+0x48);
            if(listObj) {
                const auto* list=reinterpret_cast<const IntList*>(listObj);
                if(list->size>=0 && list->size<=32 && (list->size==0 || list->items)) {
                    count=list->size;
                    if(count) firstTarget=list->items->m_Items[0];
                }
            }
        }
        QueryCaller::ObserveSelectState(stage,effectId,groupId,state,count,firstTarget);
    };
    snapshotSelect("BEFORE_STOCK");
    Picked::ObserveStockModelBoundary(effectId,groupId,false,
        selected && ExactRouting::stockCancelled.IsValid(),
        selected && ExactRouting::stockCancelled.IsValid() && ExactRouting::stockCancelled[selected].Get());
    const auto provenanceSeq=NetworkTrace::ObserveExistingSelectEnter(effectId,groupId,self,selected);
    CustomComponents::Pick3::Provenance::ScopedObservation provenanceScope(
        effectId,groupId,provenanceSeq,"EXISTING_SELECT_TARGET_PROCESS");
    if (originalPick3ModelSelect) originalPick3ModelSelect(self, selected, method);
    else LOG_ERR("CustomComponents/Pick3/PATCH2_PROVENANCE: phase=ORIGINAL_MISSING boundary=EXISTING_SELECT_TARGET_PROCESS");
    snapshotSelect("AFTER_STOCK");
    Picked::ObserveStockModelBoundary(effectId,groupId,true,
        selected && ExactRouting::stockCancelled.IsValid(),
        selected && ExactRouting::stockCancelled.IsValid() && ExactRouting::stockCancelled[selected].Get());
    NetworkTrace::ObserveExistingSelectExit(effectId,groupId,provenanceSeq);
    if (revive)
        LOG_INFO("CustomComponents/Pick3/PICK3_REVIVE_MODEL_COMMIT: phase=EXIT effect=%d group=%d selectPhase=%d count=%d first=%d",effectId,groupId,phase,targetCount,first);
}

inline BNM::Method<bool> liveEffectIsResolved;
inline BNM::Class liveBlockingEffectClass;
// A multi-group effect may be queued behind a synthetic (-1) front.  A
// front-only activation check cannot distinguish that legitimate state from
// an unrelated early popup.  Only permit the queued group when it is actually
// present in the concrete local EffectQueue and the current front is a
// placeholder, never while another real blocking effect owns the front.
// Non-mutating effect inspection. Never invoke EffectQueue.Peek(): the prior
// build could terminate inside that managed call. Snapshot only concrete lists.
enum class BlockingQueueState { Unknown, Front, PlaceholderQueued, Queued, Absent };
inline BlockingQueueState InspectBlockingQueueObject(const PreviewRequest& request, Obj* queue) {
    if (!queue || !effectQueueConcrete.IsValid() ||
        !boundQueueEffects.IsValid() || !boundQueueResponses.IsValid() ||
        !boundQueueGroupResponses.IsValid() || !liveEffectEntityId.IsValid() ||
        !liveEffectGroupId.IsValid() || !BNM::IsA(queue, effectQueueConcrete))
        return BlockingQueueState::Unknown;
    auto* effects = boundQueueEffects[queue].Get();
    auto* responses = boundQueueResponses[queue].Get();
    auto* groups = boundQueueGroupResponses[queue].Get();
    const auto valid=[](BNM::Structures::Mono::List<Obj*>* list) {
        return list && list->size >= 0 && list->size <= 4096 &&
               (!list->size || list->items);
    };
    if (!valid(effects) || !valid(responses) || !valid(groups)) return BlockingQueueState::Unknown;
    bool found=false, firstReal=false, firstPlaceholder=false, firstMatch=false;
    const auto scan=[&](BNM::Structures::Mono::List<Obj*>* list, int listId) {
        for(int i=0;i<list->size;i++) {
            Obj* e=list->items->m_Items[i];
            if (!e) continue;
            // Queue lists may contain generic effects; both fields belong to Effect.
            const int id=liveEffectEntityId[e].Get();
            const int group=liveEffectGroupId[e].Get();
            const bool match=id==request.effectEntityId && group==request.effectGroupId;
            if(match) found=true;
            if(listId==0 && i==0) {
                firstReal=id>=0;
                firstPlaceholder=id<0;
                firstMatch=match;
            }
        }
    };
    scan(effects,0);scan(responses,1);scan(groups,2);
    // A front choice is not ready while stock response work is still queued.
    // Do not mutate queues: wait for their normal merge/completion lifecycle.
    const bool pendingResponseWork = responses->size != 0 || groups->size != 0;
    BlockingQueueState state = firstMatch && !pendingResponseWork ? BlockingQueueState::Front :
        found ? BlockingQueueState::Queued : BlockingQueueState::Absent;
    if (firstMatch && pendingResponseWork) {
        static int lastDeferredEffect=-1, lastDeferredGroup=-1;
        if (lastDeferredEffect!=request.effectEntityId || lastDeferredGroup!=request.effectGroupId) {
            lastDeferredEffect=request.effectEntityId;
            lastDeferredGroup=request.effectGroupId;
            LOG_INFO("CustomComponents/Pick3/QUEUE_RESPONSE_BARRIER: effect=%d group=%d responses=%d groupResponses=%d action=WAIT_FOR_STOCK_COMPLETION",
                request.effectEntityId,request.effectGroupId,responses->size,groups->size);
        }
    }
    // Emit only on changes, and include identity to catch multi-trigger reuse.
    static int lastEffect=-1,lastGroup=-1,lastState=-1;
    if(lastEffect!=request.effectEntityId || lastGroup!=request.effectGroupId || lastState!=static_cast<int>(state)) {
        lastEffect=request.effectEntityId;lastGroup=request.effectGroupId;lastState=static_cast<int>(state);
        LOG_INFO("CustomComponents/Pick3/QUEUE_INSPECT: effect=%d group=%d state=%d frontReal=%d frontPlaceholder=%d counts=%d/%d/%d noPeek=1",
          request.effectEntityId,request.effectGroupId,lastState,firstReal?1:0,firstPlaceholder?1:0,
          effects->size,responses->size,groups->size);
    }
    return state;
}
// PvP's injected IEffectCollection and PvE's service may expose the same
// concrete EffectQueue. Never infer activation from a pending offer alone.
inline BlockingQueueState InspectBlockingQueue(const PreviewRequest& request, Obj* service) {
    if (!service || !localServiceEffects.IsValid()) return BlockingQueueState::Unknown;
    return InspectBlockingQueueObject(request, localServiceEffects[service].Get());
}
inline Obj* Target(std::uint32_t root);
inline BlockingQueueState InspectBoundBlockingQueue(const PreviewRequest& request) {
    return InspectBlockingQueueObject(request, Target(boundEffectsRoot));
}
inline bool QueuedBehindPlaceholder(const PreviewRequest& request, Obj* service) {
    return InspectBlockingQueue(request,service)==BlockingQueueState::PlaceholderQueued;
}
// A placeholder at the head means stock has NOT advanced to the requested
// blocking choice.  Do not activate a picker (including AUTO_SOLE) merely
// because the choice occurs later in the concrete queue.
inline bool LocalBlockingEffectMatches(const PreviewRequest& request, Obj* service) {
    const auto state=InspectBlockingQueue(request,service);
    if(state==BlockingQueueState::Front) return true;
    static int lastEffect=-1,lastGroup=-1,lastState=-1;
    if(lastEffect!=request.effectEntityId || lastGroup!=request.effectGroupId ||
       lastState!=static_cast<int>(state)) {
        lastEffect=request.effectEntityId;
        lastGroup=request.effectGroupId;
        lastState=static_cast<int>(state);
        LOG_INFO("CustomComponents/Pick3/QUEUE_TURN: action=DEFER effect=%d group=%d state=%d requiresExactFront=1",
            request.effectEntityId,request.effectGroupId,lastState);
    }
    return false;
}

struct TimerParameters { bool isForLocalPlayer; bool isForMulligans; };
inline BNM::Field<bool> timerRunning, timerPaused;
inline BNM::Field<TimerParameters> timerParameters;
inline bool enabled = false;
inline bool allowPvp = false;
inline bool allowPve = false;
inline int deadlineThresholdMs = 600;
inline std::uint32_t deadlineTimerRoot = 0;
inline std::uint32_t serviceRoot = 0, modelRoot = 0, stackRoot = 0, binderRoot = 0;
// Read the exact blocking Effect while the game still owns it, before Process.
// This root is ONE transaction's trigger context, not a global last-play id.
inline std::uint32_t selectedTriggerEffectRoot = 0;
inline int selectedTriggerEffectId = -1, selectedTriggerGroupId = -1;
inline CustomComponents::Systems::CustomTriggerSystem::Pick3EffectIdentitySnapshot selectedTriggerIdentity{};
// Immutable single-live choice validated against the stock candidate snapshot
// BEFORE dispatch. Do not revalidate against a mutable offer after Process().
struct VerifiedLiveChoice {
    int effectId = -1;
    int groupId = -1;
    int entityId = -1;
    int cardGuid = -1;
    bool valid = false;
};
inline VerifiedLiveChoice verifiedLiveChoice{};
inline void LogPick3EffectLifetime(const char* phase, Obj* effect) {
    const auto now = CustomComponents::Systems::CustomTriggerSystem::ReadPick3EffectIdentity(effect);
    LOG_INFO("CustomComponents/Pick3/PICK3_EFFECT_LIFETIME: phase=%s ptr=%p valid=%d effectEntity=%d group=%d source=%d target=%d springboard=%d capturedEntity=%d capturedSource=%d",
        phase, (void*)effect, int(now.valid), now.effectEntityId, now.effectGroupId,
        now.sourceId, now.targetId, now.springboardId,
        selectedTriggerIdentity.effectEntityId, selectedTriggerIdentity.sourceId);
}

inline int lastEffect = -1, lastGroup = -1;
inline std::vector<std::uint32_t> submittedPlayRoots;
inline std::mt19937 rng{std::random_device{}()};

inline Obj* Target(std::uint32_t root) {
    return root && Core::SceneViewContext::getGCHandle ? Core::SceneViewContext::getGCHandle(root) : nullptr;
}
// Optional read-only entity identity probes, using the already rooted match EntityModel.
// No GetEntityById invocation (that overload takes a List<Entity>, not an int).
inline BNM::Field<BNM::Structures::Mono::List<Obj*>*> trickEntitiesField;
inline BNM::Field<int> trickEntityIdField;
// Inspect the current player's hand entity as optional supporting context.
// It is NOT interchangeable with the initial Play's stock burst target.
// This is a native, read-only component lookup; no pooled-list invocation.
inline int Pick3CastingHandOnModel(Obj* model, int player) {
    if (!model || player < 0 || !trickEntitiesField.IsValid() ||
        !trickEntityIdField.IsValid()) return -1;
    auto* entities = trickEntitiesField[model].Get();
    if (!entities || entities->size < 0 || entities->size > 20000 ||
        (entities->size && !entities->items)) return -1;
    static auto handAssocClass=BNM::Class("PvZCards.Engine.Components", "HandAssociation");
    static auto handIdField=handAssocClass.GetField("HandEntityId").cast<int>();
    if (!handAssocClass.IsValid() || !handIdField.IsValid()) return -1;
    for (int i=0; i<entities->size; ++i) {
        Obj* e=entities->items->m_Items[i];
        if (!e || trickEntityIdField[e].Get()!=player) continue;
        auto field=BNM::Class(e).GetField("Components")
            .cast<BNM::Structures::Mono::List<Obj*>*>();
        auto* comps=field.IsValid()?field[e].Get():nullptr;
        if (!comps || comps->size < 0 || comps->size > 128 ||
            (comps->size && !comps->items)) return -1;
        for (int j=0;j<comps->size;++j) {
            Obj* c=comps->items->m_Items[j];
            if (c && BNM::IsA(c,handAssocClass)) return handIdField[c].Get();
        }
        return -1;
    }
    return -1;
}

inline unsigned trickIdentityEvents = 0;
// PlayOneTimeEffectSystem::SetupTargetsComponents (one PlayCardEffect).
// Read-only snapshot around the original call. No target mutation.
using UnearthSetupTargetsFn=void(*)(Obj*,Obj*,const void*);
inline UnearthSetupTargetsFn originalUnearthSetupTargets=nullptr;
inline BNM::Field<Obj*> unearthSetupModelField;
inline BNM::Field<int> unearthSetupLocationField;
inline unsigned unearthSetupAuditCalls=0;
inline constexpr bool kUnearthStripCastingPrimaryTrialEnabled=true; // scoped to verified PickFromThree revive PlayTrigger; removes only synthetic casting-zone PrimaryTargets
inline BNM::Field<int> unearthSetupSourceField;
inline BNM::Field<int> unearthEffectOwnerSourceField;
inline BNM::Method<Obj*> unearthSetupRemoveComponent;
inline BNM::Class unearthSetupPrimaryTargetsClass;
inline BNM::Field<int> unearthGenericPrimarySelectionType;
inline unsigned unearthSetupTrialEvents=0;
// Run after stock SetupTargetsComponents. Generic signature: Burst card, first
// PlayTrigger for that card, PickFromThree primary filter, revive descriptor.
// Other card effects and later PlayTriggers are left untouched.
inline void UnearthStripCastingPrimaryTrial(Obj* system,Obj* play,int location) {
    if(!kUnearthStripCastingPrimaryTrialEnabled) return;
    const unsigned event=++unearthSetupTrialEvents;
    const bool trace=event<=500;
    if(!system || !play || location<0) {
        if(trace) LOG_INFO("CustomComponents/Pick3/UNEARCH_STRIP_PRIMARY: REJECT event=%u reason=invalid-arguments system=%p play=%p location=%d",event,system,play,location);
        return;
    }
    Obj* model=unearthSetupModelField.IsValid()?unearthSetupModelField[system].Get():nullptr;
    auto* entities=(model && trickEntitiesField.IsValid())?trickEntitiesField[model].Get():nullptr;
    if(!entities || entities->size<0 || entities->size>20000 || (entities->size && !entities->items)) {
        if(trace) LOG_INFO("CustomComponents/Pick3/UNEARCH_STRIP_PRIMARY: REJECT event=%u reason=entities-unavailable model=%p entities=%p",event,model,entities);
        return;
    }
    const int sourceId=unearthSetupSourceField.IsValid()?unearthSetupSourceField[play].Get():-1;
    Obj* source=nullptr;
    for(int i=0;i<entities->size;++i){
        Obj* e=entities->items->m_Items[i];
        if(e && trickEntityIdField.IsValid() && trickEntityIdField[e].Get()==sourceId){source=e;break;}
    }
    Obj* card=nullptr;
    if(source && TargetBridge::getComponent.IsValid() && TargetBridge::cardClass.IsValid()) {
        const auto ex=BNM::TryInvoke([&]{card=TargetBridge::getComponent[source](TargetBridge::cardClass.GetMonoType());});
        if(ex.IsValid())card=nullptr;
    }
    const int guid=(card && TargetBridge::databaseCardGuid.IsValid())?TargetBridge::databaseCardGuid[card].Get():-1;
    if(trace) LOG_INFO("CustomComponents/Pick3/UNEARCH_STRIP_PRIMARY: SOURCE event=%u location=%d sourceId=%d found=%d card=%d guid=%d sourceField=%d guidField=%d entityCount=%d",event,location,sourceId,source?1:0,card?1:0,guid,unearthSetupSourceField.IsValid()?1:0,TargetBridge::databaseCardGuid.IsValid()?1:0,entities->size);
    // PlayCardEffect.SourceId is -1 in observed setup calls; it is NOT a safe
    // ownership signal. Do not reject the whole call based on that field.
    // Verify the EffectEntity component on EACH candidate independently.
    if(trace) LOG_INFO("CustomComponents/Pick3/UNEARCH_STRIP_PRIMARY: SOURCE_NOTE event=%u playSourceId=%d playGuid=%d ownership=per-effect-EffectEntity.SourceId",event,sourceId,guid);
    int changed=0, eligible=0, descriptorMatches=0, primaryMatches=0, playTriggerMatches=0;
    for(int i=0;i<entities->size;++i){
        Obj* e=entities->items->m_Items[i]; if(!e)continue;
        auto field=BNM::Class(e).GetField("Components").cast<BNM::Structures::Mono::List<Obj*>*>();
        auto* comps=field.IsValid()?field[e].Get():nullptr;
        if(!comps || comps->size<0 || comps->size>128 || (comps->size && !comps->items))continue;
        bool revive=false,primaryFilter=false,secondaryFilter=false;
        Obj* primary=nullptr; bool hasSecondaryTargets=false;
        for(int j=0;j<comps->size;++j){
            Obj* c=comps->items->m_Items[j];if(!c)continue;
            const auto* klass=BNM::Class(c).GetClass();const char* n=klass?klass->name:nullptr;
            if(!n)continue;
            if(!std::strcmp(n,"ReturnFromDiscardPileToLaneEffectDescriptor"))revive=true;
            else if(!std::strcmp(n,"PrimaryTargetFilter"))primaryFilter=true;
            else if(!std::strcmp(n,"SecondaryTargetFilter"))secondaryFilter=true;
            else if(!std::strcmp(n,"PrimaryTargets"))primary=c;
            else if(!std::strcmp(n,"SecondaryTargets"))hasSecondaryTargets=true;
        }
        if(!revive || !primaryFilter || !secondaryFilter)continue;
        ++descriptorMatches;
        const int effectId=trickEntityIdField.IsValid()?trickEntityIdField[e].Get():-1;
        Obj* ownerComponent=nullptr;
        for(int j=0;j<comps->size;++j) {
            Obj* c=comps->items->m_Items[j]; if(!c)continue;
            const auto* cls=BNM::Class(c).GetClass();
            if(cls && cls->name && !std::strcmp(cls->name,"EffectEntity")) {ownerComponent=c;break;}
        }
        const int ownerId=(ownerComponent && unearthEffectOwnerSourceField.IsValid())
            ?unearthEffectOwnerSourceField[ownerComponent].Get():-1;
        Obj* ownerEntity=nullptr;
        for(int k=0;k<entities->size && ownerId>=0;++k) {
            Obj* candidate=entities->items->m_Items[k];
            if(candidate && trickEntityIdField.IsValid() && trickEntityIdField[candidate].Get()==ownerId) {ownerEntity=candidate;break;}
        }
        Obj* ownerCard=nullptr;
        if(ownerEntity && TargetBridge::getComponent.IsValid() && TargetBridge::cardClass.IsValid()) {
            const auto ex=BNM::TryInvoke([&]{ownerCard=TargetBridge::getComponent[ownerEntity](TargetBridge::cardClass.GetMonoType());});
            if(ex.IsValid())ownerCard=nullptr;
        }
        const int ownerGuid=(ownerCard && TargetBridge::databaseCardGuid.IsValid())
            ?TargetBridge::databaseCardGuid[ownerCard].Get():-1;
        // Only actual burst/trick source cards qualify. Checking the source's
        // Burst marker excludes fighters, including those with an identical
        // PickFromThree + revive descriptor on one of their abilities.
        bool sourceIsBurst=false;
        if(ownerEntity) {
            auto sourceComponentsField=BNM::Class(ownerEntity).GetField("Components").cast<BNM::Structures::Mono::List<Obj*>*>();
            auto* sourceComponents=sourceComponentsField.IsValid()?sourceComponentsField[ownerEntity].Get():nullptr;
            if(sourceComponents && sourceComponents->size>=0 && sourceComponents->size<=128 &&
               (!sourceComponents->size || sourceComponents->items)) {
                for(int k=0;k<sourceComponents->size;++k) {
                    Obj* component=sourceComponents->items->m_Items[k];if(!component)continue;
                    auto* cls=BNM::Class(component).GetClass();
                    if(cls && cls->name && !std::strcmp(cls->name,"Burst")) {sourceIsBurst=true;break;}
                }
            }
        }
        // Require the ORIGINAL first-stage primary selector to be PickFromThree,
        // not a stock/manual selector or an unrelated revival effect.
        Obj* primaryFilterComponent=nullptr;
        bool playTrigger=false;
        for(int j=0;j<comps->size;++j) {
            Obj* component=comps->items->m_Items[j];if(!component)continue;
            auto* cls=BNM::Class(component).GetClass();const char* n=cls?cls->name:nullptr;
            if(!n)continue;
            if(!std::strcmp(n,"PrimaryTargetFilter"))primaryFilterComponent=component;
            else if(!std::strcmp(n,"PlayTrigger"))playTrigger=true;
        }
        const int primarySelection=(primaryFilterComponent && unearthGenericPrimarySelectionType.IsValid())
            ?unearthGenericPrimarySelectionType[primaryFilterComponent].Get():-1;
        const bool pick3Primary=EnumModes::IsPickFromThree(primarySelection);
        // Entity IDs reflect the created-effect order in the observed game.
        // Fail closed whenever an earlier PlayTrigger exists for this card,
        // even if it has a different descriptor or targets.
        bool firstPlayTrigger=playTrigger && effectId>=0 && ownerId>=0;
        if(firstPlayTrigger) {
            for(int k=0;k<entities->size;++k) {
                Obj* other=entities->items->m_Items[k];if(!other || other==e)continue;
                const int otherId=trickEntityIdField.IsValid()?trickEntityIdField[other].Get():-1;
                if(otherId<0 || otherId>=effectId)continue;
                auto otherField=BNM::Class(other).GetField("Components").cast<BNM::Structures::Mono::List<Obj*>*>();
                auto* otherComps=otherField.IsValid()?otherField[other].Get():nullptr;
                if(!otherComps || otherComps->size<0 || otherComps->size>128 || (otherComps->size && !otherComps->items))continue;
                bool otherPlay=false;Obj* otherOwner=nullptr;
                for(int j=0;j<otherComps->size;++j) {
                    Obj* c=otherComps->items->m_Items[j];if(!c)continue;
                    auto* cl=BNM::Class(c).GetClass();const char* n=cl?cl->name:nullptr;
                    if(!n)continue;
                    if(!std::strcmp(n,"PlayTrigger"))otherPlay=true;
                    else if(!std::strcmp(n,"EffectEntity"))otherOwner=c;
                }
                if(otherPlay && otherOwner && unearthEffectOwnerSourceField.IsValid() &&
                   unearthEffectOwnerSourceField[otherOwner].Get()==ownerId) {firstPlayTrigger=false;break;}
            }
        }
        const bool signature=ownerGuid>=0 && sourceIsBurst && pick3Primary && firstPlayTrigger;
        if(trace && descriptorMatches<=24)
            LOG_INFO("CustomComponents/Pick3/UNEARCH_STRIP_PRIMARY: EFFECT_OWNER event=%u effect=%d ownerSourceId=%d ownerGuid=%d burst=%d pick3=%d selection=%d firstPlay=%d scoped=%d",event,effectId,ownerId,ownerGuid,sourceIsBurst?1:0,pick3Primary?1:0,primarySelection,firstPlayTrigger?1:0,signature?1:0);
        if(!signature) {
            if(trace && descriptorMatches<=24) LOG_INFO("CustomComponents/Pick3/UNEARCH_STRIP_PRIMARY: GUARD event=%u entity=%d reason=generic-signature-rejected guid=%d burst=%d pick3=%d firstPlay=%d",event,effectId,ownerGuid,sourceIsBurst?1:0,pick3Primary?1:0,firstPlayTrigger?1:0);
            continue;
        }
        if(!primary || hasSecondaryTargets) {
            if(trace && descriptorMatches<=16) LOG_INFO("CustomComponents/Pick3/UNEARCH_STRIP_PRIMARY: GUARD event=%u entity=%d reason=target-components primary=%d secondary=%d",event,effectId,primary?1:0,hasSecondaryTargets?1:0);
            continue;
        }
        auto idsField=BNM::Class(primary).GetField("TargetEntityIds").cast<IntList*>();
        auto* ids=idsField.IsValid()?idsField[primary].Get():nullptr;
        const int count=ids?ids->size:-1;
        const int first=(ids && count>0 && ids->items)?ids->items->m_Items[0]:-1;
        if(!ids || count!=1 || !ids->items || first!=location) {
            if(trace && descriptorMatches<=16) LOG_INFO("CustomComponents/Pick3/UNEARCH_STRIP_PRIMARY: GUARD event=%u entity=%d reason=primary-ids count=%d first=%d location=%d",event,effectId,count,first,location);
            continue;
        }
        ++primaryMatches;
        ++eligible;
        // Scope further: only the effect directly tied to the card's PlayTrigger.
        // No general removal for other revive entities from other cards.
        bool fromSource=false;
        for(int j=0;j<comps->size;++j){
            Obj* c=comps->items->m_Items[j];if(!c)continue;
            const auto* klass=BNM::Class(c).GetClass();
            if(klass && klass->name && !std::strcmp(klass->name,"PlayTrigger"))fromSource=true;
        }
        if(trace) LOG_INFO("CustomComponents/Pick3/UNEARCH_STRIP_PRIMARY: GUARD event=%u entity=%d guid=%d primary=%d playTrigger=%d eligible=%d removeMethod=%d primaryType=%d",event,effectId,ownerGuid,first,fromSource?1:0,1,unearthSetupRemoveComponent.IsValid()?1:0,unearthSetupPrimaryTargetsClass.IsValid()?1:0);
        if(!fromSource || !unearthSetupRemoveComponent.IsValid() || !unearthSetupPrimaryTargetsClass.IsValid())continue;
        ++playTriggerMatches;
        Obj* removed=nullptr;
        const auto ex=BNM::TryInvoke([&]{removed=unearthSetupRemoveComponent[e](unearthSetupPrimaryTargetsClass.GetMonoType());});
        LOG_INFO("CustomComponents/Pick3/UNEARCH_STRIP_PRIMARY: entity=%d guid=%d source=%d location=%d attempted=1 exception=%d removed=%d",effectId,ownerGuid,ownerId,location,ex.IsValid()?1:0,removed?1:0);
        if(!ex.IsValid() && removed)++changed;
    }
    if(trace || changed>0)
        LOG_INFO("CustomComponents/Pick3/UNEARCH_STRIP_PRIMARY: SUMMARY event=%u guid=%d source=%d location=%d descriptorMatches=%d primaryMatches=%d eligible=%d playTriggerMatches=%d removed=%d enabled=1",event,guid,sourceId,location,descriptorMatches,primaryMatches,eligible,playTriggerMatches,changed);
}

inline void UnearthSetupSnapshot(const char* stage,Obj* system,Obj* play,int location) {
    Obj* model=unearthSetupModelField.IsValid()?unearthSetupModelField[system].Get():nullptr;
    auto* entities=(model && trickEntitiesField.IsValid())?trickEntitiesField[model].Get():nullptr;
    if(!entities || entities->size<0 || entities->size>20000 || (entities->size && !entities->items)) {
        LOG_INFO("CustomComponents/Pick3/UNEARCH_SETUP_TARGETS: %s location=%d model=%p entities=unavailable",stage,location,model);
        return;
    }
    int matched=0;
    for(int i=0;i<entities->size && matched<24;++i) {
        Obj* entity=entities->items->m_Items[i];
        if(!entity) continue;
        auto field=BNM::Class(entity).GetField("Components").cast<BNM::Structures::Mono::List<Obj*>*>();
        auto* comps=field.IsValid()?field[entity].Get():nullptr;
        if(!comps || comps->size<0 || comps->size>128 || (comps->size && !comps->items)) continue;
        bool primary=false,secondary=false,revive=false;
        int firstPrimary=-1,firstSecondary=-1;
        for(int j=0;j<comps->size;++j) {
            Obj* c=comps->items->m_Items[j]; if(!c)continue;
            auto* cl=BNM::Class(c).GetClass();
            const char* name=cl?cl->name:nullptr; if(!name)continue;
            if(!std::strcmp(name,"PrimaryTargetFilter"))primary=true;
            if(!std::strcmp(name,"SecondaryTargetFilter"))secondary=true;
            if(!std::strcmp(name,"ReturnFromDiscardPileToLaneEffectDescriptor"))revive=true;
            if(!std::strcmp(name,"PrimaryTargets") || !std::strcmp(name,"SecondaryTargets")) {
                auto idsField=BNM::Class(c).GetField("TargetEntityIds").cast<IntList*>();
                auto* ids=idsField.IsValid()?idsField[c].Get():nullptr;
                int first=(ids && ids->size>0 && ids->size<100 && ids->items)?ids->items->m_Items[0]:-1;
                if(!std::strcmp(name,"PrimaryTargets"))firstPrimary=first;
                else firstSecondary=first;
            }
        }
        if(!revive)continue;
        ++matched;
        int id=trickEntityIdField.IsValid()?trickEntityIdField[entity].Get():-1;
        LOG_INFO("CustomComponents/Pick3/UNEARCH_SETUP_TARGETS: %s entity=%d ptr=%p location=%d primaryFilter=%d secondaryFilter=%d revive=%d firstPrimary=%d firstSecondary=%d",stage,id,entity,location,primary,secondary,revive,firstPrimary,firstSecondary);
    }
    LOG_INFO("CustomComponents/Pick3/UNEARCH_SETUP_TARGETS: %s location=%d entities=%d reviveEntities=%d",stage,location,entities->size,matched);
}
inline void UnearthSetupTargetsAudit(Obj* system,Obj* play,const void* method) {
    const int location=(play && unearthSetupLocationField.IsValid())?unearthSetupLocationField[play].Get():-1;
    const bool audit=play && ++unearthSetupAuditCalls<=90;
    // PvP boundary trace: no extra managed calls, no target mutation.
    // Each thread has its own counter so nested/remote callbacks remain distinguishable.
    static thread_local unsigned traceCount = 0;
    const unsigned traceSequence = ++traceCount;
    const bool traceBoundary = traceSequence <= 96;
    if (traceBoundary) LOG_INFO("CustomComponents/Pick3/UNEARCH_PVP_BOUNDARY: phase=SETUP_ENTER seq=%u system=%p play=%p location=%d stripEnabled=%d original=%d",
        traceSequence, system, play, location, kUnearthStripCastingPrimaryTrialEnabled ? 1 : 0,
        originalUnearthSetupTargets ? 1 : 0);
    if(audit) UnearthSetupSnapshot("BEFORE",system,play,location);
    if (traceBoundary) LOG_INFO("CustomComponents/Pick3/UNEARCH_PVP_BOUNDARY: phase=STOCK_ENTER seq=%u", traceSequence);
    if(originalUnearthSetupTargets)originalUnearthSetupTargets(system,play,method);
    if (traceBoundary) LOG_INFO("CustomComponents/Pick3/UNEARCH_PVP_BOUNDARY: phase=STOCK_EXIT seq=%u", traceSequence);
    if(audit) UnearthSetupSnapshot("AFTER",system,play,location);
    if (traceBoundary) LOG_INFO("CustomComponents/Pick3/UNEARCH_PVP_BOUNDARY: phase=STRIP_ENTER seq=%u enabled=%d", traceSequence,
        kUnearthStripCastingPrimaryTrialEnabled ? 1 : 0);
    UnearthStripCastingPrimaryTrial(system,play,location);
    if (traceBoundary) LOG_INFO("CustomComponents/Pick3/UNEARCH_PVP_BOUNDARY: phase=STRIP_EXIT seq=%u", traceSequence);
}

inline void TrickLogIdentity(const char* stage, unsigned seq, int id) {
    if (++trickIdentityEvents > 650 || id < 0) return;
    Obj* model = Target(boundEntityModelRoot);
    if (!model || !trickEntitiesField.IsValid() || !trickEntityIdField.IsValid()) {
        LOG_INFO("CustomComponents/Pick3/TRICK_IDENTITY: phase=%s seq=%u entity=%d modelReady=0", stage, seq, id);
        return;
    }
    auto* entities = trickEntitiesField[model].Get();
    if (!entities || entities->size < 0 || entities->size > 20000 || (entities->size && !entities->items)) {
        LOG_INFO("CustomComponents/Pick3/TRICK_IDENTITY: phase=%s seq=%u entity=%d listReady=0", stage, seq, id);
        return;
    }
    Obj* entity = nullptr;
    for (int i=0; i<entities->size; ++i) {
        Obj* candidate = entities->items->m_Items[i];
        if (candidate && trickEntityIdField[candidate].Get() == id) { entity=candidate; break; }
    }
    int guid = -1;
    if (entity && TargetBridge::getComponent.IsValid() && TargetBridge::cardClass.IsValid() && TargetBridge::databaseCardGuid.IsValid()) {
        Obj* card=nullptr;
        const auto ex=BNM::TryInvoke([&] { card=TargetBridge::getComponent[entity](TargetBridge::cardClass.GetMonoType()); });
        if (!ex.IsValid() && card) guid=TargetBridge::databaseCardGuid[card].Get();
    }
    if (stage && stage[0] == 'D' && stage[1] == 'R' && stage[2] == 'A' && stage[3] == 'G') trickSecondaryFocusGuid = guid;
    LOG_INFO("CustomComponents/Pick3/TRICK_IDENTITY: phase=%s seq=%u entity=%d found=%d cardGuid=%d cardComponent=%d modelReady=1", stage, seq, id, entity?1:0, guid, guid>0?1:0);
}
inline void TrickLogTargetsIdentity(const char* stage, unsigned seq, IntList* list) {
    if (!list || list->size < 0 || list->size > 4096 || (list->size && !list->items)) return;
    const int count = std::min(list->size, 12);
    for (int i=0; i<count; ++i) TrickLogIdentity(stage, seq, list->items->m_Items[i]);
}

// Stock valid-play enumeration, observation only. PoolableList<T> derives from List<T>.
// Never return, cache, or release pooled objects outside their owner.
using TargetGenerationFn = Obj*(*)(Obj*, int, int, Obj*, const void*);
inline TargetGenerationFn originalDeciderValidTargets = nullptr;
inline TargetGenerationFn originalDeciderValidPlays = nullptr;
inline unsigned trickGenerationEvents = 0;
inline int TrickGenerationGuid(int id);
inline bool IsGenericPick3ReviveBurst(int cardId);
// Read-only narrow validation trace. The concrete Validate implementations are
// individually hooked; the Validator interface itself has no native body.
using TrickOrderFn = int(*)(Obj*, Obj*, int, int, const void*);
using TrickValidateFn = bool(*)(Obj*, Obj*, Obj*, const void*);
inline TrickOrderFn originalTrickOrder = nullptr;
inline TrickValidateFn originalTrickCompositeValidate = nullptr;
inline TrickValidateFn originalTrickOneTimeValidate = nullptr;
inline TrickValidateFn originalTrickNullValidate = nullptr;
inline TrickValidateFn originalTrickInHandValidate = nullptr;
// Stock validator child observers: original results, untouched errors and Play.
inline TrickValidateFn originalTrickFighterValidate = nullptr;
inline TrickValidateFn originalTrickOpenLaneValidate = nullptr;
inline unsigned trickChildValidatorSamples = 0;

// Observers only: never fabricate targets or bypass the game's validator.
using TrickHasAnyTargetsFn = bool(*)(Obj*, int, Obj*, const void*);
inline TrickHasAnyTargetsFn originalTrickHasAnyTargets = nullptr;
inline unsigned trickHasAnyTargetSamples = 0;
inline bool TrickHasAnyTargetsObserved(Obj* self, int cardId, Obj* errors, const void* method) {
    const bool result = originalTrickHasAnyTargets ? originalTrickHasAnyTargets(self, cardId, errors, method) : false;
    if (trickHasAnyTargetSamples < 240) {
        ++trickHasAnyTargetSamples;
        // No managed error enumeration or pooled-object dereference.
        LOG_INFO("CustomComponents/Pick3/PICK3_REVIVE_GATE: phase=HAS_ANY_TARGETS card=%d validator=%p errors=%p accepted=%d sample=%u secondaryPairVerified=0",
                 cardId, self, errors, result ? 1 : 0, trickHasAnyTargetSamples);
    }
    return result;
}

inline thread_local unsigned trickValidationDepth = 0;
inline thread_local int trickValidationGuid = -1;
inline thread_local int trickValidationCard = -1;
inline thread_local unsigned trickOrderCalls = 0;
inline thread_local unsigned trickValidatorCalls = 0;
inline unsigned trickTraceGenerationCount = 0;
// Bounded, per-thread rejection matrix. Read only validator return values;
// do not invoke interface collections or retain managed objects.
struct Pick3Stage1ValidationMatrix {
    unsigned oneTimeCalls=0, oneTimeRejects=0;
    unsigned compositeCalls=0, compositeRejects=0;
    unsigned fighterCalls=0, fighterRejects=0;
    unsigned openLaneCalls=0, openLaneRejects=0;
    unsigned inHandCalls=0, inHandRejects=0;
    unsigned nullCalls=0, nullRejects=0;
    unsigned targetingCalls=0, targetingRejects=0;
};
inline thread_local Pick3Stage1ValidationMatrix pick3Stage1Matrix{};
inline void Pick3RecordStage1Validator(const char* type, bool accepted) {
    if (!trickValidationDepth || !IsGenericPick3ReviveBurst(trickValidationCard) || !type) return;
    unsigned* calls=nullptr;
    unsigned* rejects=nullptr;
    if (!std::strcmp(type,"OneTimeEffect")) { calls=&pick3Stage1Matrix.oneTimeCalls; rejects=&pick3Stage1Matrix.oneTimeRejects; }
    else if (!std::strcmp(type,"Composite")) { calls=&pick3Stage1Matrix.compositeCalls; rejects=&pick3Stage1Matrix.compositeRejects; }
    else if (!std::strcmp(type,"Fighter")) { calls=&pick3Stage1Matrix.fighterCalls; rejects=&pick3Stage1Matrix.fighterRejects; }
    else if (!std::strcmp(type,"OpenLane")) { calls=&pick3Stage1Matrix.openLaneCalls; rejects=&pick3Stage1Matrix.openLaneRejects; }
    else if (!std::strcmp(type,"InHand")) { calls=&pick3Stage1Matrix.inHandCalls; rejects=&pick3Stage1Matrix.inHandRejects; }
    else if (!std::strcmp(type,"Null")) { calls=&pick3Stage1Matrix.nullCalls; rejects=&pick3Stage1Matrix.nullRejects; }
    if (calls) { ++*calls; if (!accepted) ++*rejects; }
}

// Resolve the actual first owned effect from the SAME model as the stock
// OneTimeEffectValidator, not from a UI/selection model. No cached managed
// references or pooled target collections leave this synchronous scope.
struct Pick3InitialEffectSnapshot {
    int effectId = -1;
    Obj* primary = nullptr;
    Obj* secondary = nullptr;
    Obj* primaryQuery = nullptr;
    Obj* secondaryQuery = nullptr;
    bool complete = false;
};
inline Pick3InitialEffectSnapshot ResolvePick3InitialEffectOnModel(Obj* model, int cardId) {
    Pick3InitialEffectSnapshot result;
    if (!model || cardId < 0 || !trickEntitiesField.IsValid() ||
        !trickEntityIdField.IsValid() || !unearthEffectOwnerSourceField.IsValid()) return result;
    auto* entities = trickEntitiesField[model].Get();
    if (!entities || entities->size < 0 || entities->size > 20000 ||
        (entities->size && !entities->items)) return result;
    for (int i = 0; i < entities->size; ++i) {
        Obj* entity = entities->items->m_Items[i];
        if (!entity) continue;
        const int id = trickEntityIdField[entity].Get();
        if (id < 0 || (result.effectId >= 0 && id >= result.effectId)) continue;
        auto componentField = BNM::Class(entity).GetField("Components")
            .cast<BNM::Structures::Mono::List<Obj*>*>();
        auto* components = componentField.IsValid() ? componentField[entity].Get() : nullptr;
        if (!components || components->size < 0 || components->size > 128 ||
            (components->size && !components->items)) continue;
        bool onPlay = false;
        Obj* owner = nullptr;
        Obj* primary = nullptr;
        Obj* secondary = nullptr;
        for (int j = 0; j < components->size; ++j) {
            Obj* c = components->items->m_Items[j];
            if (!c) continue;
            auto* klass = BNM::Class(c).GetClass();
            const char* name = klass ? klass->name : nullptr;
            if (!name) continue;
            if (!std::strcmp(name, "PlayTrigger")) onPlay = true;
            else if (!std::strcmp(name, "EffectEntity")) owner = c;
            else if (!std::strcmp(name, "PrimaryTargetFilter")) primary = c;
            else if (!std::strcmp(name, "SecondaryTargetFilter")) secondary = c;
        }
        if (!onPlay || !owner || unearthEffectOwnerSourceField[owner].Get() != cardId)
            continue;
        // Store the earliest owned PlayTrigger, including if it is malformed;
        // never silently skip to a later effect.
        result = {};
        result.effectId = id;
        result.primary = primary;
        result.secondary = secondary;
        result.primaryQuery = primary && TargetBridge::targetFilterQuery.IsValid()
            ? TargetBridge::targetFilterQuery[primary].Get() : nullptr;
        result.secondaryQuery = secondary && TargetBridge::targetFilterQuery.IsValid()
            ? TargetBridge::targetFilterQuery[secondary].Get() : nullptr;
        result.complete = primary && secondary && result.primaryQuery && result.secondaryQuery;
    }
    return result;
}

// This path deliberately supports ONLY the exact HasComponent(Lane) secondary
// query used by our custom revive cards. Any other query is unproven and
// remains stock-unplayable; never interpret an arbitrary secondary query as a
// blanket permission to revive into any lane.
inline bool Pick3SecondaryIsLaneOnly(Obj* query) {
    if (!query) return false;
    const auto* klass = BNM::Class(query).GetClass();
    if (!klass || !klass->name || std::strcmp(klass->name, "HasComponentQuery"))
        return false;
    static auto componentField = BNM::Class("PvZCards.Engine.Queries", "HasComponentQuery")
        .GetField("ComponentType").cast<Obj*>();
    static auto laneType = BNM::Class("PvZCards.Engine.Components", "Lane").GetMonoType();
    return componentField.IsValid() && laneType &&
        componentField[query].Get() == reinterpret_cast<Obj*>(laneType);
}

// Evaluate a secondary lane query through the game's actual EntityFinder
// rather than approximating nested And/Or/Not/owner query semantics. The
// finder creates its own EntityQueryContext for this exact model. This bridge
// only supports filters whose non-query modifiers do not impose additional
// stock-only restrictions; those remain fail-closed.
inline Pick3SecondaryDecision EvaluatePick3SecondaryQueryWithStockFinder(
    Obj* model, int effectId, Obj* secondaryFilter, Obj* secondaryQuery,
    int primaryCardId, int laneId, int playerId) {
    if (!model || !secondaryFilter || !secondaryQuery || effectId < 0 ||
        primaryCardId <= 0 || laneId <= 0 || playerId < 0)
        return Pick3SecondaryDecision::Unsupported;

    // Extra filter modifiers require the stock TargetEntityFinder + validator,
    // not just the EntityQuery.Matches traversal. Never grant permission for
    // an arbitrary additional-target filter from this bridge.
    const auto* filter = reinterpret_cast<const std::uint8_t*>(secondaryFilter);
    if (*reinterpret_cast<const int*>(filter + 0x2C) != 0 ||
        *reinterpret_cast<Obj* const*>(filter + 0x30) != nullptr ||
        *reinterpret_cast<const bool*>(filter + 0x38))
        return Pick3SecondaryDecision::Unsupported;

    static auto finderClass = BNM::Class("PvZCards.Engine", "EntityFinder");
    static auto findEntities = finderClass.GetMethod("FindEntities", 1)
        .cast<BNM::Structures::Mono::List<Obj*>*>();
    static auto queryContext = finderClass.GetField("QueryContext").cast<Obj*>();
    if (!finderClass.IsValid() || !findEntities.IsValid() ||
        !queryContext.IsValid())
        return Pick3SecondaryDecision::Unsupported;

    Obj* finder = nullptr;
    BNM::Structures::Mono::List<Obj*>* matches = nullptr;
    bool completed = false;
    const auto invoked = BNM::TryInvoke([&] {
        finder = finderClass.CreateNewObjectParameters(model);
        if (!finder) return;
        Obj* context = queryContext[finder].Get();
        if (!context) return;
        auto* contextBytes = reinterpret_cast<std::uint8_t*>(context);
        Obj* effectContext = *reinterpret_cast<Obj**>(contextBytes + 0x18);
        if (!effectContext) return;
        // Phase-1 stock query context: the discarded card is now the primary
        // target; the casting card is the source, and the effect is exact.
        *reinterpret_cast<int*>(contextBytes + 0x20) = primaryCardId;
        auto* effectBytes = reinterpret_cast<std::uint8_t*>(effectContext);
        *reinterpret_cast<int*>(effectBytes + 0x10) = primaryCardId;
        *reinterpret_cast<int*>(effectBytes + 0x14) = pick3PrimaryCaptureCard;
        *reinterpret_cast<int*>(effectBytes + 0x18) = effectId;
        matches = findEntities[finder](secondaryQuery);
        completed = matches != nullptr;
    });
    if (invoked.IsValid() || !completed) return Pick3SecondaryDecision::Unsupported;
    if (matches->size < 0 || matches->size > 20000 ||
        (matches->size && !matches->items))
        return Pick3SecondaryDecision::Unsupported;
    for (int i = 0; i < matches->size; ++i) {
        Obj* entity = matches->items->m_Items[i];
        if (entity && trickEntityIdField.IsValid() &&
            trickEntityIdField[entity].Get() == laneId)
            return Pick3SecondaryDecision::Allow;
    }
    return Pick3SecondaryDecision::Reject;
}

inline bool Pick3VerifyCapturedRevivePair(Obj* play, int* foundCard, int* foundLane,
                                          const char** reason) {
    if (foundCard) *foundCard = -1;
    if (foundLane) *foundLane = -1;
    if (reason) *reason = "NO_CAPTURE";
    if (play && pick3CurrentValidationModel && pick3PrimaryCaptureCard>=0 &&
        (deferredPrimaryCards.card!=pick3PrimaryCaptureCard || deferredPrimaryCards.ids.empty())) {
        PF3_REPAIR_TRACE("CustomComponents/Pick3/REPAIR1712: PRIMARY_MISSING card=%d effect=%d model=%p capturedCard=%d candidates=%zu decision=stockFallback_noSyntheticPair",
            pick3PrimaryCaptureCard,pick3PrimaryCaptureEffect,(void*)pick3CurrentValidationModel,
            deferredPrimaryCards.card,deferredPrimaryCards.ids.size());
    }
    if (!play || !pick3PhysicalPairCheck || !pick3CurrentValidationModel ||
        pick3PrimaryCaptureCard < 0 ||
        deferredPrimaryCards.card != pick3PrimaryCaptureCard ||
        deferredPrimaryCards.ids.empty() || !trickPlayPlayerIdField.IsValid()) return false;
    const auto first = ResolvePick3InitialEffectOnModel(
        pick3CurrentValidationModel, pick3PrimaryCaptureCard);
    if (!first.complete || first.effectId != pick3PrimaryCaptureEffect) {
        if (reason) *reason = "FIRST_EFFECT_UNAVAILABLE";
        return false;
    }
    // The built-in fast path is intentionally strict. Richer filters require
    // an effect-aware evaluator that can prove the exact (primary,lane) match.
    // We do not assume that a component-only lane test represents arbitrary
    // nested / owner / attribute / selector queries.
    const auto* filterBytes = reinterpret_cast<const std::uint8_t*>(first.secondary);
    const bool plainFilter = first.secondary &&
        *reinterpret_cast<const int*>(filterBytes + 0x2C) == 0 &&
        *reinterpret_cast<Obj* const*>(filterBytes + 0x30) == nullptr &&
        !*reinterpret_cast<const bool*>(filterBytes + 0x38);
    const bool plainLaneQuery = plainFilter && Pick3SecondaryIsLaneOnly(first.secondaryQuery);
    if (!plainLaneQuery && !pick3SecondaryFilterEvaluator) {
        if (reason) *reason = "SECONDARY_EVALUATOR_REQUIRED";
        return false;
    }
    auto* entities = trickEntitiesField[pick3CurrentValidationModel].Get();
    if (!entities || entities->size < 0 || entities->size > 20000 ||
        (entities->size && !entities->items)) {
        if (reason) *reason = "MODEL_ENTITIES_UNAVAILABLE";
        return false;
    }
    const int player = trickPlayPlayerIdField[play].Get();
    for (int primary : deferredPrimaryCards.ids) {
        for (int i = 0; i < entities->size; ++i) {
            Obj* lane = entities->items->m_Items[i];
            if (!lane) continue;
            auto componentsField = BNM::Class(lane).GetField("Components")
                .cast<BNM::Structures::Mono::List<Obj*>*>();
            auto* components = componentsField.IsValid() ? componentsField[lane].Get() : nullptr;
            if (!components || components->size < 0 || components->size > 128 ||
                (components->size && !components->items)) continue;
            bool laneComponent = false;
            for (int j = 0; j < components->size; ++j) {
                Obj* component = components->items->m_Items[j];
                if (!component) continue;
                auto* type = BNM::Class(component).GetClass();
                if (type && type->name && !std::strcmp(type->name, "Lane")) {
                    laneComponent = true; break;
                }
            }
            if (!laneComponent) continue;
            const int laneId = trickEntityIdField[lane].Get();
            if (laneId < 0) continue;
            // The registered evaluator may support additional query shapes.
            // Unsupported is not Allow. The callback is synchronous and must
            // use the same validation model and effect identity.
            // Test physical capacity (including Team-Up) first, but never
            // mistake it for acceptance by a complex stock lane query.
            if (!pick3PhysicalPairCheck(pick3CurrentValidationModel,
                                        primary, laneId, player)) continue;
            const auto decision = plainLaneQuery
                ? Pick3SecondaryDecision::Allow
                : pick3SecondaryFilterEvaluator(
                    pick3CurrentValidationModel, first.effectId, first.secondary,
                    first.secondaryQuery, primary, laneId, player);
            if (decision == Pick3SecondaryDecision::Unsupported) {
                static thread_local int lastUnsupportedEffect=-1;
                if (lastUnsupportedEffect!=first.effectId) {
                    lastUnsupportedEffect=first.effectId;
                    LOG_INFO("CustomComponents/Pick3/PICK3_COMPLEX_LANE_QUERY: effect=%d result=UNSUPPORTED afterPhysical=1 stockQueryRequired=1",first.effectId);
                }
            }
            if (decision == Pick3SecondaryDecision::Allow) {
                if (foundCard) *foundCard = primary;
                if (foundLane) *foundLane = laneId;
                if (reason) *reason = "LEGAL_PAIR";
                return true;
            }
        }
    }
    if (reason) *reason = "NO_FILTERED_PHYSICAL_PAIR";
    return false;
}

inline int TrickOrderTrace(Obj* self, Obj* card, int player, int target, const void* method) {
    const int result = originalTrickOrder ? originalTrickOrder(self, card, player, target, method) : 0;
    if (trickValidationDepth && TrickFocus(trickValidationGuid) && trickTraceGenerationCount <= 160) {
        ++trickOrderCalls;
        LOG_INFO("CustomComponents/Pick3/TRICK_VALIDATION: phase=ORDERS guid=%d card=%d player=%d target=%d targetGuid=%d orders=%d",
                 trickValidationGuid, trickValidationCard, player, target, TrickGenerationGuid(target), result);
    }
    return result;
}
inline thread_local Obj* receiveReviveRecognitionModel = nullptr;
inline Obj* ReviveRecognitionModel();
inline bool TrickValidateObserved(const char* type, TrickValidateFn original, Obj* self, Obj* play, Obj* errors, const void* method) {
    // 1.7.28: incoming Type-3 Process() does not necessarily nest a
    // DetermineIfPlayIsValid() call. Bind the *validator's own* simulation
    // EntityModel before IsGenericPick3ReviveBurst(), rather than trying to
    // reuse LocalSelectTargetService's Type-6-only Finder. The binding lives
    // solely for this synchronous validation invocation and always restores.
    struct ScopedType3ValidatorRecognition final {
        Obj* previous;
        Obj* model = nullptr;
        int card = -1;
        explicit ScopedType3ValidatorRecognition(const char* kind, Obj* validator, Obj* candidate)
            : previous(receiveReviveRecognitionModel) {
            if (!enabled || !kind || std::strcmp(kind, "OneTimeEffect") ||
                !validator || !candidate || trickServiceActivePlay != candidate ||
                !trickPlayTypeField.IsValid() || !trickPlayCardIdField.IsValid() ||
                trickPlayTypeField[candidate].Get() != 3 ||
                !pick3OneTimeValidatorModelField.IsValid() ||
                !boundEntityModelClass.IsValid()) return;
            card = trickPlayCardIdField[candidate].Get();
            Obj* found = pick3OneTimeValidatorModelField[validator].Get();
            if (found && BNM::IsA(found, boundEntityModelClass)) {
                model = found;
                receiveReviveRecognitionModel = found;
            }
            static thread_local unsigned samples = 0;
            if (++samples <= 96)
                LOG_INFO("CustomComponents/Pick3/RECEIVER1728: phase=VALIDATOR_MODEL_BIND card=%d service=%p play=%p validator=%p model=%p retained=%p accepted=%d",
                    card, trickServiceActivePlay, candidate, validator, found,
                    Target(boundEntityModelRoot), model ? 1 : 0);
        }
        ~ScopedType3ValidatorRecognition() { receiveReviveRecognitionModel = previous; }
    } receiveType3Model(type, self, play);
    const Obj* previousCandidate = trickCandidateActivePlay;
    const bool scopedCandidate = kDeferredPick3TrialEnabled && trickValidationDepth &&
        IsGenericPick3ReviveBurst(trickValidationCard) &&
        play && trickPlayCardIdField.IsValid() &&
        trickPlayCardIdField[play].Get() == trickValidationCard;
    if (scopedCandidate) trickCandidateActivePlay = play;
    Obj* previousModel = pick3CurrentValidationModel;
    const int previousModelCard = pick3CurrentValidationCard;
    // PlayCardService validates a fresh Play (often type 3), not the generated
    // candidate Play. Re-establish the SAME model-bound primary capture inside
    // its own synchronous OneTimeEffectValidator call; never reuse the hand/UI
    // snapshot across transactions or clients.
    const bool scopedService = kDeferredPick3TrialEnabled && self && play &&
        trickServiceActivePlay == play &&
        trickPlayCardIdField.IsValid() && trickPlayTypeField.IsValid() &&
        trickPlayTypeField[play].Get() == 3 &&
        IsGenericPick3ReviveBurst(trickPlayCardIdField[play].Get());
    const bool oneTimeScope = (scopedCandidate || scopedService) && self && type &&
        !std::strcmp(type, "OneTimeEffect") &&
        pick3OneTimeValidatorModelField.IsValid();
    static thread_local unsigned serviceScopeSamples = 0;
    if (oneTimeScope) {
        const int scopedCard = scopedService ? trickPlayCardIdField[play].Get() : trickValidationCard;
        pick3CurrentValidationModel = pick3OneTimeValidatorModelField[self].Get();
        pick3CurrentValidationCard = scopedCard;
        const auto firstEffect = ResolvePick3InitialEffectOnModel(
            pick3CurrentValidationModel, scopedCard);
        // Establish exact effect ownership before the stock target utility
        // executes. The scope is restored after stock validation returns.
        pick3PrimaryCaptureModel = pick3CurrentValidationModel;
        pick3PrimaryCaptureCard = scopedCard;
        pick3PrimaryCaptureEffect = firstEffect.complete ? firstEffect.effectId : -1;
        deferredPrimaryCards = {};
        pick3SynchronousVerifiedCastTarget = -1;
        if (scopedService && ++serviceScopeSamples <= 96)
            LOG_INFO("CustomComponents/Pick3/RECEIVER1727: phase=CAPTURE_ENTER card=%d play=%p service=%p determine=%p model=%p effect=%d complete=%d type=3 samePlay=1", scopedCard,
                     play,trickServiceActivePlay,trickDetermineActivePlay,
                     pick3CurrentValidationModel, firstEffect.effectId, firstEffect.complete ? 1 : 0);
        static thread_local unsigned effectSamples = 0;
        if (++effectSamples <= 96) {
            LOG_INFO("CustomComponents/Pick3/PICK3_INITIAL_EFFECT: card=%d guid=%d model=%p firstEffect=%d primary=%p primaryQuery=%p secondary=%p secondaryQuery=%p complete=%d pairVerified=0 decision=stock-unchanged",
                trickValidationCard, trickValidationGuid, pick3CurrentValidationModel,
                firstEffect.effectId, firstEffect.primary, firstEffect.primaryQuery,
                firstEffect.secondary, firstEffect.secondaryQuery,
                firstEffect.complete ? 1 : 0);
        }
        static thread_local unsigned modelSamples = 0;
        if (++modelSamples <= 96)
            LOG_INFO("CustomComponents/Pick3/PICK3_INITIAL_MODEL: phase=VALIDATOR_ENTER card=%d guid=%d validator=%p model=%p modelVerified=%d queryContextReady=0 pairVerified=0 override=0",
                     trickValidationCard, trickValidationGuid, self,
                     pick3CurrentValidationModel, pick3CurrentValidationModel ? 1 : 0);
    }
    const bool result = original ? original(self, play, errors, method) : false;
    if (oneTimeScope && scopedCandidate && result &&
        pick3CurrentValidationModel && pick3PrimaryCaptureEffect>=0 &&
        deferredPrimaryCards.card==pick3PrimaryCaptureCard &&
        deferredPrimaryCards.ids.size()==1) {
        const int sole=deferredPrimaryCards.ids.front();
        auto& proof=stockSoleInitialPrimary;
        if (proof.card==-1) {
            proof={pick3PrimaryCaptureCard,pick3PrimaryCaptureEffect,sole,
                   pick3CurrentValidationModel,false};
        } else if (proof.card!=pick3PrimaryCaptureCard ||
                   proof.effect!=pick3PrimaryCaptureEffect ||
                   proof.target!=sole || proof.model!=pick3CurrentValidationModel) {
            proof.ambiguous=true;
        }
        PF3_REPAIR_TRACE("CustomComponents/Pick3/REPAIR1716: SOLE_STOCK_PROOF card=%d effect=%d target=%d model=%p ambiguous=%d proofSource=validator",
            pick3PrimaryCaptureCard,pick3PrimaryCaptureEffect,sole,
            (void*)pick3CurrentValidationModel,int(proof.ambiguous));
    }
    if (oneTimeScope && scopedService && result &&
        pick3CurrentValidationModel && pick3PrimaryCaptureEffect >= 0 &&
        deferredPrimaryCards.card == pick3PrimaryCaptureCard &&
        !deferredPrimaryCards.ids.empty() && deferredPrimaryCards.ids.size() <= 128) {
        pick3AcceptedCastPrimaries.model = pick3CurrentValidationModel;
        pick3AcceptedCastPrimaries.card = pick3PrimaryCaptureCard;
        pick3AcceptedCastPrimaries.effect = pick3PrimaryCaptureEffect;
        pick3AcceptedCastPrimaries.ids = deferredPrimaryCards.ids;
        pick3AcceptedCastPrimaries.captured = std::chrono::steady_clock::now();
        if (pick3SynchronousVerifiedCastTarget >= 0) {
            pick3VerifiedInitialCast = {pick3CurrentValidationModel,
                pick3PrimaryCaptureCard,pick3PrimaryCaptureEffect,
                pick3SynchronousVerifiedCastTarget,std::chrono::steady_clock::now()};
            PF3_REPAIR_TRACE("CustomComponents/Pick3/REPAIR1710: CAST_COMMITTED card=%d effect=%d cast=%d model=%p stage=stockValidatorAccepted",
                pick3PrimaryCaptureCard,pick3PrimaryCaptureEffect,
                pick3SynchronousVerifiedCastTarget,(void*)pick3CurrentValidationModel);
        }
        LOG_INFO("CustomComponents/Pick3/PICK3_REVIVE_OFFER_BRIDGE: phase=STORE card=%d effect=%d ids=%zu first=%d model=%p validated=1",
            pick3AcceptedCastPrimaries.card, pick3AcceptedCastPrimaries.effect,
            pick3AcceptedCastPrimaries.ids.size(), pick3AcceptedCastPrimaries.ids[0],
            pick3AcceptedCastPrimaries.model);
    }
    if (oneTimeScope) {
        // Never let IDs or validation-model pointers leak into a later Play,
        // model, or opponent's selection transaction.
        if (scopedService && serviceScopeSamples <= 96)
            LOG_INFO("CustomComponents/Pick3/PICK3_SERVICE_PAIR: phase=CAPTURE_EXIT card=%d result=%d", pick3PrimaryCaptureCard, result ? 1 : 0);
        deferredPrimaryCards = {};
        pick3SynchronousVerifiedCastTarget = -1;
        pick3PrimaryCaptureModel = nullptr;
        pick3PrimaryCaptureCard = -1;
        pick3PrimaryCaptureEffect = -1;
        pick3CurrentValidationModel = previousModel;
        pick3CurrentValidationCard = previousModelCard;
    }
    if (scopedCandidate) trickCandidateActivePlay = const_cast<Obj*>(previousCandidate);
    Pick3RecordStage1Validator(type, result);
    // Correlate the initial Unearth Play with the particular stock validator
    // rejection. No managed collection inspection or return-value override.
    if (scopedCandidate && !result && type &&
        (!std::strcmp(type, "OneTimeEffect") || !std::strcmp(type, "Composite"))) {
        static unsigned unearthGateRejectSamples = 0;
        if (++unearthGateRejectSamples <= 240) {
            const int zone = FindDeferredInitialZone(trickValidationCard);
            // Read only the concrete Play.Targets list already verified by the
            // game's Play layout. Never invoke methods on the pooled validator
            // collections (the earlier native crash came from that path).
            int targetCount = -1, firstTarget = -1;
            if (trickPlayTargetsField.IsValid()) {
                auto* ids = trickPlayTargetsField[play].Get();
                if (ids && ids->size >= 0 && ids->size <= 32 &&
                    (ids->size == 0 || ids->items)) {
                    targetCount = ids->size;
                    if (targetCount > 0) firstTarget = ids->items->m_Items[0];
                }
            }
            LOG_INFO("CustomComponents/Pick3/UNEARCH_STAGE1_GATE: guid=%d card=%d playerScope=generated zone=%d validator=%s play=%p validatorObject=%p targetCount=%d firstTarget=%d targetMatchesZone=%d rejected=1 mutation=0",
                     trickValidationGuid, trickValidationCard, zone, type, play, self,
                     targetCount, firstTarget, (zone >= 0 && firstTarget == zone) ? 1 : 0);
        }
    }
    if (trickDetermineActivePlay && play==trickDetermineActivePlay) {
        LOG_INFO("CustomComponents/Pick3/TRICK_SERVICE: phase=VALIDATOR play=%p guid=%d type=%s validator=%p errors=%p accepted=%d",play,TrickServicePlayGuid(play),type,self,errors,result?1:0);
    }
    if (trickValidationDepth && TrickFocus(trickValidationGuid) && trickTraceGenerationCount <= 160) {
        ++trickValidatorCalls;
        LOG_INFO("CustomComponents/Pick3/TRICK_VALIDATION: phase=VALIDATE guid=%d card=%d type=%s validator=%p play=%p errors=%p accepted=%d",
                 trickValidationGuid, trickValidationCard, type, self, play, errors, result ? 1 : 0);
    }
    return result;
}
// Read-only OneTimeEffectValidator.IsTargetingValid diagnostic.
// The method accepts interface collections; inspect them only through managed
// get_Count/get_Item or GetEnumerator/MoveNext/get_Current, never list offsets.
using TrickIsTargetingValidFn = bool(*)(Obj*, Obj*, Obj*, Obj*, const void*);
inline TrickIsTargetingValidFn originalTrickIsTargetingValid = nullptr;
inline unsigned trickTargetValidSamples = 0;
inline int TrickCollectionCount(Obj* value) {
    if (!value) return -1;
    auto count = BNM::Class(value).GetMethod("get_Count",0).cast<int>();
    if (!count.IsValid()) return -2;
    int n=-2;
    auto exception=BNM::TryInvoke([&]{ n=count[value](); });
    return exception.IsValid() || n<0 || n>4096 ? -2 : n;
}
inline void TrickCollectionInspect(const char* label, Obj* value) {
    if (!value) {
        LOG_INFO("CustomComponents/Pick3/TRICK_TARGET_VALID: list=%s null=1",label);
        return;
    }
    auto klass=BNM::Class(value);
    auto* raw=klass.GetClass();
    const int n=TrickCollectionCount(value);
    int ids[12]{};
    int shown=0;
    bool enumerated=false;
    // Fast path for actual indexable collections (does not assume a memory layout).
    auto item=klass.GetMethod("get_Item",1).cast<int>();
    if (n>=0 && item.IsValid()) {
        enumerated=true;
        for(int i=0;i<n && i<12;i++) {
            int x=0;
            auto ex=BNM::TryInvoke([&]{ x=item[value](i); });
            if(ex.IsValid()) { enumerated=false;break; }
            ids[shown++]=x;
        }
    } else {
        // Generic IEnumerable<int> path; MoveNext and get_Current are invoked
        // on the actual enumerator instance, with an upper bound.
        auto getEnum=klass.GetMethod("GetEnumerator",0).cast<Obj*>();
        if(getEnum.IsValid()) {
            Obj* en=nullptr;
            auto ex=BNM::TryInvoke([&]{en=getEnum[value]();});
            if(!ex.IsValid() && en) {
                auto ek=BNM::Class(en);
                auto move=ek.GetMethod("MoveNext",0).cast<bool>();
                auto current=ek.GetMethod("get_Current",0).cast<int>();
                if(move.IsValid() && current.IsValid()) {
                    enumerated=true;
                    for(int i=0;i<12;i++) {
                        bool more=false;
                        auto mEx=BNM::TryInvoke([&]{more=move[en]();});
                        if(mEx.IsValid()) { enumerated=false;break; }
                        if(!more) break;
                        int x=0;
                        auto cEx=BNM::TryInvoke([&]{x=current[en]();});
                        if(cEx.IsValid()) {enumerated=false;break;}
                        ids[shown++]=x;
                    }
                }
                // Some enumerators implement IDisposable; do not retain them.
                auto dispose=ek.GetMethod("Dispose",0).cast<void>();
                if(dispose.IsValid()) BNM::TryInvoke([&]{dispose[en]();});
            }
        }
    }
    LOG_INFO("CustomComponents/Pick3/TRICK_TARGET_VALID: list=%s class=%s.%s count=%d readable=%d shown=%d ids=[%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d] (first shown entries only)",
        label,raw&&raw->namespaze?raw->namespaze:"?",raw&&raw->name?raw->name:"?",
        n,enumerated?1:0,shown,ids[0],ids[1],ids[2],ids[3],ids[4],ids[5],ids[6],ids[7],ids[8],ids[9],ids[10],ids[11]);
}
// Patch 1.7.18: passive, synchronous snapshots of the *actual arguments*
// passed to OneTimeEffectValidator.IsTargetingValid. Only concrete List<int>
// and PoolableList<int> layouts are known. Interface/HashSet/enumerator
// values are deliberately reported opaque: reflective enumeration in this
// hot path has previously caused crashes during PvP validation.
inline bool TrickReadConcreteTargetArgument(Obj* value, int* count, int* first,
                                            int* second, const char** concrete) {
    *count = -1; *first = -1; *second = -1; *concrete = "opaque";
    if (!value) { *concrete = "null"; return false; }
    auto* klass = BNM::Class(value).GetClass();
    const char* name = klass ? klass->name : nullptr;
    if (!name || (std::strcmp(name, "List`1") != 0 &&
                  std::strcmp(name, "PoolableList`1") != 0 &&
                  std::strcmp(name, "List<int>") != 0 &&
                  std::strcmp(name, "PoolableList<int>") != 0))
        return false;
    const auto* ids = reinterpret_cast<const IntList*>(value);
    const int n = ids->size;
    if (n < 0 || n > 128 ||
        (n && (!ids->items || ids->items->capacity < static_cast<size_t>(n)))) {
        *concrete = "invalid-bounds";
        return false;
    }
    *count = n;
    if (n > 0) *first = ids->items->m_Items[0];
    if (n > 1) *second = ids->items->m_Items[1];
    *concrete = "concrete-list";
    return true;
}
inline bool TrickIsTargetingValidTrace(Obj* self, Obj* valid, Obj* selected, Obj* errors, const void* method) {
    // Service collection enumeration is diagnostic-only. During PvP the game's
    // validation can own pooled/interface enumerators with a shorter lifetime
    // than our reflected reads. Keep this path passive; stock validation and
    // the actual deferred primary-source capture below are unchanged.
    if(trickDetermineActivePlay && TrickFocus(TrickServicePlayGuid(trickDetermineActivePlay))) {
        LOG_INFO("CustomComponents/Pick3/TRICK_SERVICE: phase=TARGET_COMPARE play=%p guid=%d validator=%p collections=not-enumerated",trickDetermineActivePlay,TrickServicePlayGuid(trickDetermineActivePlay),self);
    }
    const bool focus=trickValidationDepth && TrickFocus(trickValidationGuid) && ++trickTargetValidSamples<=160;
    const int before=-2; // Diagnostic collection reads disabled in validation hot path.
    if(focus) LOG_INFO("CustomComponents/Pick3/TRICK_TARGET_VALID: phase=ENTER guid=%d card=%d validator=%p valid=%p selected=%p errors=%p enumeration=disabled",trickValidationGuid,trickValidationCard,self,valid,selected,errors);
    // One narrowly scoped experiment: the initial zone was *actually* produced
    // by the engine for this same card, and the submitted Play selects that exact ID.
    // Preserve stock validation for every other case, including all pick3 effect
    // selections and secondary lane selection after the initial play.
    static thread_local unsigned pvpTargetBoundarySeq = 0;
    const unsigned boundary = ++pvpTargetBoundarySeq;
    const bool receiverService = trickServiceActivePlay &&
        trickPlayCardIdField.IsValid() && trickPlayTypeField.IsValid() &&
        trickPlayTypeField[trickServiceActivePlay].Get() == 3;
    const bool nestedDetermine = receiverService &&
        trickDetermineActivePlay == trickServiceActivePlay;
    LOG_INFO("CustomComponents/Pick3/RECEIVER1727: phase=SERVICE_SCOPE_ENTER seq=%u service=%p determine=%p nested=%d type3=%d valid=%p selected=%p", boundary,
        trickServiceActivePlay,trickDetermineActivePlay,int(nestedDetermine),int(receiverService),valid,selected);
    int serviceCard = -1;
    bool serviceScope = false;
    if (receiverService) {
        serviceCard = trickPlayCardIdField[trickServiceActivePlay].Get();
        LOG_INFO("CustomComponents/Pick3/UNEARCH_PVP_VALID: seq=%u phase=PREDICATE_ENTER card=%d",boundary,serviceCard);
        serviceScope = IsGenericPick3ReviveBurst(serviceCard);
        LOG_INFO("CustomComponents/Pick3/UNEARCH_PVP_VALID: seq=%u phase=PREDICATE_EXIT card=%d matched=%d",boundary,serviceCard,serviceScope?1:0);
        if (!serviceScope) {
            static thread_local unsigned failureSamples = 0;
            if (++failureSamples <= 48) {
                Obj* recognitionModel = ReviveRecognitionModel();
                auto* entities = recognitionModel && trickEntitiesField.IsValid()
                    ? trickEntitiesField[recognitionModel].Get() : nullptr;
                int found = 0;
                if (entities && trickEntityIdField.IsValid() &&
                    entities->size >= 0 && entities->size <= 20000 &&
                    (!entities->size || entities->items)) {
                    for (int i=0; i<entities->size; ++i) {
                        Obj* e = entities->items->m_Items[i];
                        if (e && trickEntityIdField[e].Get() == serviceCard) { found = 1; break; }
                    }
                }
                LOG_INFO("CustomComponents/Pick3/RECEIVER1728: phase=RECOGNITION_MISS card=%d model=%p retained=%p modelBound=%d entities=%p entityCount=%d found=%d",
                    serviceCard, recognitionModel, Target(boundEntityModelRoot),
                    receiveReviveRecognitionModel ? 1 : 0, entities,
                    entities ? entities->size : -1, found);
            }
        }
    }
    // No BNM::TryInvoke(get_Item) or get_Count on service-owned pooled data.
    // Previously this occurred in the opponent's nested validation callback.
    // Keep the last validated snapshot only if obtained through a proven-safe
    // path elsewhere; do not populate one from an unsafe interface collection.
    // The enclosing service OneTimeEffectValidator captured stock primary IDs
    // from its concrete PoolableList synchronously. Do not clear them here:
    // this comparison is precisely where the pair check needs them. We still
    // never enumerate the incoming ICollection interface objects.
    LOG_INFO("CustomComponents/Pick3/UNEARCH_PVP_VALID: seq=%u phase=SERVICE_SCOPE_EXIT matched=%d",boundary,serviceScope?1:0);
    const bool candidateScope = trickCandidateActivePlay && trickValidationDepth &&
        IsGenericPick3ReviveBurst(trickValidationCard) &&
        trickPlayCardIdField.IsValid() &&
        trickPlayCardIdField[trickCandidateActivePlay].Get() == trickValidationCard;
    Obj* activePlay = serviceScope ? trickServiceActivePlay :
        (candidateScope ? trickCandidateActivePlay : nullptr);
    if (kDeferredPick3TrialEnabled && activePlay &&
        trickPlayCardIdField.IsValid() && trickPlayTargetsField.IsValid()) {
        const int card = trickPlayCardIdField[activePlay].Get();
        auto* targets = trickPlayTargetsField[activePlay].Get();
        const int zone = FindDeferredInitialZone(card);
        // PvP safety: never reflectively invoke get_Count on the stock
        // validator's pooled/interface collections. The old "count > 0"
        // shortcut could crash inside OneTimeEffectValidator.Validate and
        // bypassed stock targeting checks for normalized zone Plays.
        // Preserve the existing play-generation bridge, but let stock decide
        // validity here until a proven-safe legality representation exists.
        const char* scope = serviceScope ? "service" : "candidate";
        LOG_INFO("CustomComponents/Pick3/DEFERRED_TRIAL: phase=STOCK_COMPARE_SAFE scope=%s guid=%d card=%d zone=%d play=%p noManagedCount=1 noValidationBypass=1",
            scope,TrickServicePlayGuid(activePlay),card,zone,activePlay);
    }
    // Validate *before* stock writes an initial-targeting error: the first
    // effect selects discarded cards, but the initial Play selects its zone.
    // Do not apply this to subsequent selection-service validation.
    const bool pairScope = (candidateScope || serviceScope) && activePlay &&
        pick3PrimaryCaptureCard >= 0 &&
        pick3PrimaryCaptureCard == (serviceScope ? serviceCard : trickValidationCard) &&
        pick3CurrentValidationCard == pick3PrimaryCaptureCard &&
        pick3PrimaryCaptureModel &&
        pick3PrimaryCaptureModel == pick3CurrentValidationModel &&
        pick3PrimaryCaptureEffect >= 0 && trickPlayTargetsField.IsValid();
    // Diagnostic-only: log exactly the arguments received by this stock
    // comparison, not the earlier PF3 candidate list. Never alter either
    // collection, errors or the return value. Values of unsupported concrete
    // types remain opaque instead of risking managed iterator lifetime bugs.
    if (serviceScope || candidateScope) {
        static thread_local unsigned initialCompareSamples = 0;
        if (++initialCompareSamples <= 128) {
            int validCount=-1,validFirst=-1,validSecond=-1;
            int selectedCount=-1,selectedFirst=-1,selectedSecond=-1;
            const char *validKind=nullptr,*selectedKind=nullptr;
            const bool validRead=TrickReadConcreteTargetArgument(valid,&validCount,
                &validFirst,&validSecond,&validKind);
            const bool selectedRead=TrickReadConcreteTargetArgument(selected,&selectedCount,
                &selectedFirst,&selectedSecond,&selectedKind);
            const int activeCard = serviceScope ? serviceCard : trickValidationCard;
            const int activeType = activePlay && trickPlayTypeField.IsValid()
                ? trickPlayTypeField[activePlay].Get() : -1;
            LOG_INFO("CustomComponents/Pick3/REPAIR1718: INITIAL_PLAY_VALIDATION scope=%s card=%d guid=%d effect=%d model=%p play=%p type=%d validObj=%p validKind=%s validReadable=%d validCount=%d validFirst=%d validSecond=%d selectedObj=%p selectedKind=%s selectedReadable=%d selectedCount=%d selectedFirst=%d selectedSecond=%d pairScope=%d mutation=0",
                serviceScope?"service":"candidate",activeCard,TrickGenerationGuid(activeCard),
                pick3PrimaryCaptureEffect,(void*)pick3CurrentValidationModel,activePlay,activeType,
                valid,validKind,int(validRead),validCount,validFirst,validSecond,
                selected,selectedKind,int(selectedRead),selectedCount,selectedFirst,
                selectedSecond,int(pairScope));
        }
    }
    if (pairScope) {
        auto* playTargets = trickPlayTargetsField[activePlay].Get();
        const int generatedZone = FindDeferredInitialZone(pick3PrimaryCaptureCard);
        // The remote peer has no UI drag-cache. Read the exact player's hand
        // association on the *validator's* simulation model rather than
        // guessing a globally fixed casting zone (old code used literal 10).
        // Both local and remote casting Plays must match this hand entity.
        const int playOwner=trickPlayPlayerIdField.IsValid() ?
            trickPlayPlayerIdField[activePlay].Get() : -1;
        const int modelHand=Pick3CastingHandOnModel(pick3CurrentValidationModel,playOwner);
        const bool remoteMissingGenerated = serviceScope && !candidateScope &&
            generatedZone < 0 && trickPlayTypeField.IsValid() &&
            trickPlayTypeField[activePlay].Get() == 3;
        const bool remoteZoneFallback = remoteMissingGenerated && modelHand >= 0;
        // A generated one-target burst is our exact casting target on the
        // candidate path. It is NOT necessarily HandAssociation.HandEntityId
        // (observed 159 vs hand 3 in the 11:16 PvP test). The remote hand
        // fallback is kept only when it *itself* matches the incoming Play.
        // Do not accept a received non-hand target without equivalent proof.
        const int incomingZone = playTargets && playTargets->size == 1 &&
            playTargets->items ? playTargets->items->m_Items[0] : -1;
        const bool fromStockBurst = generatedZone >= 0 && incomingZone == generatedZone;
        const bool fromVerifiedRemoteHand = remoteZoneFallback && incomingZone == modelHand;
        // 1.7.15: The receiving peer does not execute the sender's drag and
        // its generated-zone cache is sometimes empty. In the two-client
        // 2026-11-02 17:53:59 trace, stock Play.Type=3 targeted entity 10;
        // the receiver had generated=-1 and verified the discarded-card/lane
        // pair successfully. That casting entity is NOT necessarily HandId.
        // This exact compatibility case is deliberately not a replacement
        // for arbitrary remote target verification: only the initial
        // service-side Play, a known legacy casting entity, and the *same*
        // owning validator model/effect may use it. The physical card/lane
        // preflight below still must pass before any local acceptance.
        constexpr int kObservedLegacyInitialCastEntity = 10;
        const bool fromProvenLegacyReceiver =
            remoteMissingGenerated && incomingZone == kObservedLegacyInitialCastEntity &&
            playOwner >= 0 && pick3CurrentValidationModel &&
            pick3PrimaryCaptureModel == pick3CurrentValidationModel &&
            pick3PrimaryCaptureCard == serviceCard &&
            pick3PrimaryCaptureEffect >= 0;
        const int initialZone = fromStockBurst ? generatedZone :
            (fromVerifiedRemoteHand ? modelHand :
             (fromProvenLegacyReceiver ? incomingZone : -1));
        const bool exactZone = initialZone >= 0 &&
            (fromStockBurst || fromVerifiedRemoteHand || fromProvenLegacyReceiver);
        PF3_REPAIR_TRACE("CustomComponents/Pick3/REPAIR1715: RECEIVER_CAST_SCOPE card=%d effect=%d player=%d generated=%d incoming=%d hand=%d model=%p fromBurst=%d fromHand=%d fromLegacyReceiver=%d exact=%d needsPair=1",
            pick3PrimaryCaptureCard,pick3PrimaryCaptureEffect,playOwner,generatedZone,incomingZone,modelHand,
            (void*)pick3CurrentValidationModel,int(fromStockBurst),int(fromVerifiedRemoteHand),
            int(fromProvenLegacyReceiver),int(exactZone));
        PF3_REPAIR_TRACE("CustomComponents/Pick3/REPAIR1710: CAST_SOURCE card=%d effect=%d owner=%d generated=%d incoming=%d hand=%d burstMatch=%d remoteHandMatch=%d accepted=%d scope=%s",
            pick3PrimaryCaptureCard,pick3PrimaryCaptureEffect,playOwner,generatedZone,incomingZone,modelHand,
            int(fromStockBurst),int(fromVerifiedRemoteHand),int(exactZone),serviceScope?"service":"candidate");
        if(serviceScope && !exactZone && incomingZone>=0)
            PF3_REPAIR_TRACE("CustomComponents/Pick3/REPAIR1710: REMOTE_CAST_UNPROVEN card=%d effect=%d incoming=%d generated=%d hand=%d preservedStock=1",
                pick3PrimaryCaptureCard,pick3PrimaryCaptureEffect,incomingZone,generatedZone,modelHand);
        PF3_REPAIR_TRACE("CustomComponents/Pick3/REPAIR178: CAST_PREFLIGHT card=%d owner=%d model=%p hand=%d generated=%d incoming=%d fallback=%d exact=%d scope=%s",
            pick3PrimaryCaptureCard,playOwner,(void*)pick3CurrentValidationModel,
            modelHand,generatedZone,playTargets&&playTargets->size==1&&playTargets->items?playTargets->items->m_Items[0]:-1,
            int(remoteZoneFallback),int(exactZone),serviceScope?"service":"candidate");
        if (serviceScope) {
            static thread_local unsigned zoneSamples = 0;
            if (++zoneSamples <= 120)
                LOG_INFO("CustomComponents/Pick3/PICK3_SERVICE_ZONE: card=%d generated=%d resolved=%d fallback=%d submitted=%d exact=%d model=%p",
                         pick3PrimaryCaptureCard, generatedZone, initialZone,
                         (remoteZoneFallback || fromProvenLegacyReceiver) ? 1 : 0,
                         playTargets && playTargets->size == 1 && playTargets->items
                            ? playTargets->items->m_Items[0] : -1,
                         exactZone ? 1 : 0, pick3CurrentValidationModel);
        }
        int primary = -1, lane = -1;
        const char* why = "NOT_INITIAL_ZONE";
        const bool pair = exactZone && Pick3VerifyCapturedRevivePair(
            activePlay, &primary, &lane, &why);
        static thread_local unsigned pairLogSamples = 0;
        if (++pairLogSamples <= 120)
            LOG_INFO("CustomComponents/Pick3/PICK3_STAGE1_PAIR: card=%d guid=%d zone=%d initial=%d primary=%d lane=%d verified=%d reason=%s",
                     pick3PrimaryCaptureCard, TrickGenerationGuid(pick3PrimaryCaptureCard), initialZone,
                     exactZone ? 1 : 0, primary, lane, pair ? 1 : 0, why);
        if (serviceScope && pairLogSamples <= 120)
            LOG_INFO("CustomComponents/Pick3/PICK3_SERVICE_PAIR: phase=VALIDATE card=%d zone=%d exact=%d primary=%d lane=%d accepted=%d reason=%s",
                pick3PrimaryCaptureCard, initialZone, exactZone ? 1 : 0, primary, lane, pair ? 1 : 0, why);
        if (pair) {
            // The enclosing original validator still decides whether the
            // casting Play is accepted; this record is only staged here.
            pick3SynchronousVerifiedCastTarget=initialZone;
            PF3_REPAIR_TRACE("CustomComponents/Pick3/REPAIR1710: PAIR_APPROVED card=%d effect=%d cast=%d discardedCard=%d lane=%d stage=preStockValidation",
                pick3PrimaryCaptureCard,pick3PrimaryCaptureEffect,initialZone,primary,lane);
            if (serviceScope)
                PF3_REPAIR_TRACE("CustomComponents/Pick3/REPAIR1715: RECEIVER_PAIR_COMMITTED card=%d effect=%d cast=%d selectedCandidate=%d legalLane=%d legacy=%d physicalPair=1 stockServerAck=unknown",
                    pick3PrimaryCaptureCard,pick3PrimaryCaptureEffect,initialZone,primary,lane,
                    int(fromProvenLegacyReceiver));
            // This is a LOCAL CUSTOM validator success, not an invocation of
            // OneTimeEffectValidator.IsTargetingValid and not a server ACK.
            // Preserve the tested 1.7.13 behavior until a stock-compatible
            // authoritative target contract has been demonstrated.
            if (serviceScope && trickServiceActivePlay==activePlay) {
                auth1714LocalPairAccepted=true;
                auth1714LocalPairCard=pick3PrimaryCaptureCard;
                auth1714LocalPairTarget=initialZone;
            }
            PF3_REPAIR_TRACE("CustomComponents/Pick3/REPAIR1714: TARGET_AUTHORITY_CONTRACT guid=%d card=%d effect=%d player=%d scope=%s playType=%d cast=%d primary=%d lane=%d fromBurst=%d remoteHand=%d localBypass=1 stockValidatorCalled=0 authoritativeAccepted=0",
                TrickGenerationGuid(pick3PrimaryCaptureCard),pick3PrimaryCaptureCard,pick3PrimaryCaptureEffect,playOwner,
                serviceScope?"service":"candidate",trickPlayTypeField.IsValid()?trickPlayTypeField[activePlay].Get():-1,
                initialZone,primary,lane,int(fromStockBurst),int(fromVerifiedRemoteHand));
            return true;
        }
        PF3_REPAIR_TRACE("CustomComponents/Pick3/REPAIR1710: PAIR_REJECT card=%d effect=%d cast=%d reason=%s preflight=%d stockFallback=1",
            pick3PrimaryCaptureCard,pick3PrimaryCaptureEffect,initialZone,why,int(exactZone));
    }
    const bool result=originalTrickIsTargetingValid ? originalTrickIsTargetingValid(self,valid,selected,errors,method):false;
    if (serviceScope || candidateScope)
        PF3_REPAIR_TRACE("CustomComponents/Pick3/REPAIR1714: STOCK_TARGET_DECISION guid=%d card=%d scope=%s accepted=%d localBypass=0",
            TrickGenerationGuid(serviceScope?serviceCard:trickValidationCard),serviceScope?serviceCard:trickValidationCard,
            serviceScope?"service":"candidate",int(result));
    if (trickValidationDepth && IsGenericPick3ReviveBurst(trickValidationCard)) {
        ++pick3Stage1Matrix.targetingCalls;
        if (!result) ++pick3Stage1Matrix.targetingRejects;
    }

    if(focus) {
        const int after=-2;
        LOG_INFO("CustomComponents/Pick3/TRICK_TARGET_VALID: phase=EXIT guid=%d card=%d result=%d errorCountBefore=%d errorCountAfter=%d added=%d errors=opaque",
            trickValidationGuid,trickValidationCard,result?1:0,before,after,(before>=0&&after>=0)?after-before:-1);
    }
    return result;
}
inline bool TrickCompositeValidate(Obj* a, Obj* b, Obj* c, const void* m) { return TrickValidateObserved("Composite", originalTrickCompositeValidate, a,b,c,m); }
inline bool TrickOneTimeValidate(Obj* a, Obj* b, Obj* c, const void* m) { return TrickValidateObserved("OneTimeEffect", originalTrickOneTimeValidate, a,b,c,m); }
inline bool TrickNullValidate(Obj* a, Obj* b, Obj* c, const void* m) { return TrickValidateObserved("Null", originalTrickNullValidate, a,b,c,m); }
inline bool TrickInHandValidate(Obj* a, Obj* b, Obj* c, const void* m) { return TrickValidateObserved("InHand", originalTrickInHandValidate, a,b,c,m); }
inline bool TrickFighterValidate(Obj* a, Obj* b, Obj* c, const void* m) {
    return TrickValidateObserved("Fighter", originalTrickFighterValidate, a,b,c,m);
}
inline bool TrickOpenLaneValidate(Obj* a, Obj* b, Obj* c, const void* m) {
    return TrickValidateObserved("OpenLane", originalTrickOpenLaneValidate, a,b,c,m);
}


// Patch 1.7.11: receive-side recognition must inspect the model owned by
// the LocalSelectTargetService actually processing the incoming PvP Play.
// This scope is synchronous and thread-local: never rebind the presentation
// EntityModel or retain the receiver model beyond Process().
inline thread_local Obj* generationReviveRecognitionModel = nullptr;
inline BNM::Field<Obj*> gameStateUtilsEntityModelField;
inline BNM::Field<BNM::Structures::Mono::List<Obj*>*> trickEntityComponentsField;
inline BNM::Class trickCardComponentClass;
inline Obj* ReviveRecognitionModel() {
    if (receiveReviveRecognitionModel) return receiveReviveRecognitionModel;
    if (generationReviveRecognitionModel) return generationReviveRecognitionModel;
    if (reviveDragLookupActive && deferredValidatedPlay.card >= 0) {
        auto* proven = Target(reviveLegalPlayModelRoot);
        const auto age = std::chrono::steady_clock::now() - deferredValidatedPlay.provenAt;
        if (proven && age >= std::chrono::steady_clock::duration::zero() &&
            age <= std::chrono::seconds(8)) return proven;
    }
    return Target(boundEntityModelRoot);
}
struct ScopedGenerationReviveRecognition final {
    Obj* previous;
    Obj* model;
    explicit ScopedGenerationReviveRecognition(Obj* decider) :
        previous(generationReviveRecognitionModel), model(nullptr) {
        if (decider && gameStateUtilsEntityModelField.IsValid()) {
            auto* candidate=gameStateUtilsEntityModelField[decider].Get();
            if (candidate && boundEntityModelClass.IsValid() &&
                BNM::IsA(candidate,boundEntityModelClass)) model=candidate;
        }
        if (model) generationReviveRecognitionModel=model;
        PF3_REPAIR_TRACE("CustomComponents/Pick3/REPAIR1713: GENERATION_MODEL decider=%p deciderModel=%p retained=%p valid=%d",
            (void*)decider,(void*)model,(void*)Target(boundEntityModelRoot),model?1:0);
    }
    ~ScopedGenerationReviveRecognition() { generationReviveRecognitionModel=previous; }
};
struct ScopedReceiveReviveRecognition final {
    Obj* previous = nullptr;
    Obj* serviceModel = nullptr;
    explicit ScopedReceiveReviveRecognition(Obj* service, Obj* play) {
        previous = receiveReviveRecognitionModel;
        if (!enabled || !service || !play || !playType.IsValid() ||
            playType[play].Get() != 6 || !localClass.IsValid() ||
            !BNM::IsA(service, localClass) || !localServiceFinder.IsValid() ||
            !TargetBridge::finderEntityFinder.IsValid() ||
            !TargetBridge::entityFinderModel.IsValid()) return;
        Obj* finder = localServiceFinder[service].Get();
        Obj* entityFinder = finder ? TargetBridge::finderEntityFinder[finder].Get() : nullptr;
        Obj* model = entityFinder ? TargetBridge::entityFinderModel[entityFinder].Get() : nullptr;
        if (!model || !boundEntityModelClass.IsValid() ||
            !BNM::IsA(model, boundEntityModelClass)) {
            PF3_REPAIR_TRACE("CustomComponents/Pick3/REPAIR1711: RX_MODEL_SCOPE_REJECT service=%p serviceModel=%p retainedModel=%p reason=invalid-service-model",
                (void*)service, (void*)model, (void*)Target(boundEntityModelRoot));
            return;
        }
        serviceModel = model;
        receiveReviveRecognitionModel = model;
        PF3_REPAIR_TRACE("CustomComponents/Pick3/REPAIR1711: RX_MODEL_SCOPE_ENTER service=%p serviceModel=%p retainedModel=%p differ=%d",
            (void*)service, (void*)model, (void*)Target(boundEntityModelRoot),
            int(model != Target(boundEntityModelRoot)));
    }
    ~ScopedReceiveReviveRecognition() {
        receiveReviveRecognitionModel = previous;
        if (serviceModel)
            PF3_REPAIR_TRACE("CustomComponents/Pick3/REPAIR1711: RX_MODEL_SCOPE_EXIT serviceModel=%p restored=%p",
                (void*)serviceModel, (void*)previous);
    }
    ScopedReceiveReviveRecognition(const ScopedReceiveReviveRecognition&) = delete;
    ScopedReceiveReviveRecognition& operator=(const ScopedReceiveReviveRecognition&) = delete;
};

// Native-only GUID lookup for diagnostic and reconciliation call sites.
// Do not call managed Entity.GetComponent from card-play/validation hooks:
// re-entrant IL2CPP invocation here was present in the matching PvP crash stack.
// The entity's Components list already contains the Card component.
inline int TrickGenerationGuid(int id) {
    if (id < 0 || !trickEntitiesField.IsValid() || !trickEntityIdField.IsValid() ||
        !TargetBridge::databaseCardGuid.IsValid() || !trickEntityComponentsField.IsValid() ||
        !trickCardComponentClass.IsValid()) return -1;
    Obj* model = ReviveRecognitionModel();
    if (!model) return -1;
    auto* entities = trickEntitiesField[model].Get();
    if (!entities || entities->size < 0 || entities->size > 20000 ||
        (entities->size && !entities->items)) return -1;
    for (int i = 0; i < entities->size; ++i) {
        Obj* entity = entities->items->m_Items[i];
        if (!entity || trickEntityIdField[entity].Get() != id) continue;
        auto* components = trickEntityComponentsField[entity].Get();
        if (!components || components->size < 0 || components->size > 128 ||
            (components->size && !components->items)) return -1;
        for (int j = 0; j < components->size; ++j) {
            Obj* component = components->items->m_Items[j];
            if (!component) continue;
            if (BNM::IsA(component, trickCardComponentClass))
                return TargetBridge::databaseCardGuid[component].Get();
        }
        return -1;
    }
    return -1;
}
// Strict generic entry-point predicate. This runs against the rooted live
// entity model, not a GUID allowlist. Only the first PlayTrigger effect of
// a Burst card is accepted, and it MUST carry the PickFromThree primary
// filter and ReturnFromDiscardPileToLane descriptor. Later effects are not
// repaired just because they have the same descriptor.
inline bool ComputeGenericPick3ReviveBurst(int cardId) {
    struct Scope { int id; bool log; Scope(int c):id(c),log(PvpCrashTraceEnabled()){ if(log) {++pvpCrashTraceSerial; ++pvpCrashTraceDepth; PvpCrashMark("GENERIC_BURST","ENTER",id); } } ~Scope(){if(log){PvpCrashMark("GENERIC_BURST","EXIT",id);--pvpCrashTraceDepth;}} } traceScope(cardId);
    if(cardId<0 || !trickEntitiesField.IsValid() || !trickEntityIdField.IsValid() ||
       !unearthEffectOwnerSourceField.IsValid() || !TargetBridge::selectionType.IsValid())return false;
    Obj* model=ReviveRecognitionModel();
    auto* entities=model?trickEntitiesField[model].Get():nullptr;
    if(!entities || entities->size<0 || entities->size>20000 || (entities->size && !entities->items))return false;
    Obj* source=nullptr;
    for(int i=0;i<entities->size;++i){
        Obj* e=entities->items->m_Items[i];if(e && trickEntityIdField[e].Get()==cardId){source=e;break;}
    }
    if(!source)return false;
    auto srcField=BNM::Class(source).GetField("Components").cast<BNM::Structures::Mono::List<Obj*>*>();
    auto* src=srcField.IsValid()?srcField[source].Get():nullptr;
    bool burst=false;
    if(src && src->size>=0 && src->size<=128 && (!src->size || src->items)) {
        for(int j=0;j<src->size;++j){
            Obj* c=src->items->m_Items[j];if(!c)continue;
            auto* k=BNM::Class(c).GetClass();if(k && k->name && !std::strcmp(k->name,"Burst")){burst=true;break;}
        }
    }
    if(!burst)return false;
    int firstId=INT32_MAX;bool firstQualifies=false;
    for(int i=0;i<entities->size;++i){
        Obj* e=entities->items->m_Items[i];if(!e)continue;
        int id=trickEntityIdField[e].Get();if(id<0 || id>=firstId)continue;
        auto f=BNM::Class(e).GetField("Components").cast<BNM::Structures::Mono::List<Obj*>*>();
        auto* comps=f.IsValid()?f[e].Get():nullptr;
        if(!comps || comps->size<0 || comps->size>128 || (comps->size && !comps->items))continue;
        bool play=false,revive=false,secondary=false;Obj* owner=nullptr;Obj* primary=nullptr;
        for(int j=0;j<comps->size;++j){
            Obj* c=comps->items->m_Items[j];if(!c)continue;
            auto* k=BNM::Class(c).GetClass();const char* n=k?k->name:nullptr;if(!n)continue;
            if(!std::strcmp(n,"PlayTrigger"))play=true;
            else if(!std::strcmp(n,"EffectEntity"))owner=c;
            else if(!std::strcmp(n,"PrimaryTargetFilter"))primary=c;
            else if(!std::strcmp(n,"SecondaryTargetFilter"))secondary=true;
            else if(!std::strcmp(n,"ReturnFromDiscardPileToLaneEffectDescriptor"))revive=true;
        }
        if(!play || !owner || unearthEffectOwnerSourceField[owner].Get()!=cardId)continue;
        firstId=id;
        firstQualifies=revive && primary && secondary &&
            EnumModes::IsPickFromThree(TargetBridge::selectionType[primary].Get());
    }
    return firstQualifies;
}
// Cheap source-identity validation plus a bounded cache prevents repeated
// full-model class/reflection scans during normal card play and PvP submissions.
// Entries expire after 32 lookups, and are invalidated on model/list changes.
// No managed pointers are retained beyond the live model/source identity check.
inline bool IsGenericPick3ReviveBurst(int cardId) {
    if (cardId < 0 || !trickEntitiesField.IsValid() || !trickEntityIdField.IsValid()) return false;
    Obj* model=ReviveRecognitionModel();
    auto* entities=model?trickEntitiesField[model].Get():nullptr;
    if (!entities || entities->size<0 || entities->size>20000 ||
        (entities->size && !entities->items)) return false;
    Obj* source=nullptr;
    for(int i=0;i<entities->size;++i) {
        Obj* e=entities->items->m_Items[i];
        if(e && trickEntityIdField[e].Get()==cardId) {source=e;break;}
    }
    if(!source)return false;
    struct Entry {
        Obj* model=nullptr;
        const void* list=nullptr;
        Obj* source=nullptr;
        int count=-1, id=-1;
        unsigned ticket=0;
        bool result=false;
    };
    static thread_local Entry cache[64]{};
    static thread_local unsigned ticket=0;
    static thread_local bool evaluating=false;
    if(evaluating)return false; // fail closed rather than recursively inspect a mutating model
    ++ticket;
    Entry& slot=cache[static_cast<unsigned>(cardId)%64];
    if(slot.model==model && slot.list==entities && slot.source==source &&
       slot.count==entities->size && slot.id==cardId &&
       (ticket-slot.ticket)<=32) return slot.result;
    struct Guard { bool& flag; explicit Guard(bool& f):flag(f){flag=true;} ~Guard(){flag=false;} } guard(evaluating);
    const bool result=ComputeGenericPick3ReviveBurst(cardId);
    slot={model,entities,source,entities->size,cardId,ticket,result};
    return result;
}
// Before rewriting a gesture, prove its original release target is a real
// board lane in the current live recognition model. This prevents canceled
// drags (which often resolve to the dragged card itself) from casting a trick.
// This is a read-only structural check; it is NOT a replacement for stock
// physical lane legality or the later secondary-target validator.
inline bool IsReviveDropLaneOnCurrentModel(int targetId) {
    if (targetId<0 || !trickEntitiesField.IsValid() ||
        !trickEntityIdField.IsValid()) return false;
    Obj* model=ReviveRecognitionModel();
    auto* entities=model?trickEntitiesField[model].Get():nullptr;
    if (!entities || entities->size<0 || entities->size>20000 ||
        (entities->size && !entities->items)) return false;
    for (int i=0; i<entities->size; ++i) {
        Obj* entity=entities->items->m_Items[i];
        if (!entity || trickEntityIdField[entity].Get()!=targetId) continue;
        auto field=BNM::Class(entity).GetField("Components")
            .cast<BNM::Structures::Mono::List<Obj*>*>();
        auto* components=field.IsValid()?field[entity].Get():nullptr;
        if (!components || components->size<0 || components->size>128 ||
            (components->size && !components->items)) return false;
        for(int j=0; j<components->size; ++j) {
            Obj* component=components->items->m_Items[j];
            if (!component) continue;
            auto* type=BNM::Class(component).GetClass();
            if (type && type->name && !std::strcmp(type->name,"Lane")) return true;
        }
        return false;
    }
    return false;
}
// Identify the exact first-PlayTrigger revive effect behind a picker request.
// No GUID list: this is checked against the live effect ownership and the
// same strict structural signature used for lane-cast validation.
inline bool IsGenericPick3ReviveEffect(int effectId) {
    if(effectId<0 || !trickEntitiesField.IsValid() || !trickEntityIdField.IsValid() ||
       !unearthEffectOwnerSourceField.IsValid())return false;
    Obj* model=ReviveRecognitionModel();
    auto* entities=model?trickEntitiesField[model].Get():nullptr;
    if(!entities || entities->size<0 || entities->size>20000 || (entities->size && !entities->items))return false;
    int ownerId=-1;bool play=false;
    for(int i=0;i<entities->size;++i){
        Obj* e=entities->items->m_Items[i];if(!e || trickEntityIdField[e].Get()!=effectId)continue;
        auto f=BNM::Class(e).GetField("Components").cast<BNM::Structures::Mono::List<Obj*>*>();
        auto* components=f.IsValid()?f[e].Get():nullptr;
        if(!components || components->size<0 || components->size>128 || (components->size && !components->items))return false;
        for(int j=0;j<components->size;++j){
            Obj* c=components->items->m_Items[j];if(!c)continue;
            auto* cl=BNM::Class(c).GetClass();const char* name=cl?cl->name:nullptr;if(!name)continue;
            if(!std::strcmp(name,"PlayTrigger"))play=true;
            if(!std::strcmp(name,"EffectEntity"))ownerId=unearthEffectOwnerSourceField[c].Get();
        }
        break;
    }
    if(!play || ownerId<0 || !IsGenericPick3ReviveBurst(ownerId))return false;
    // Fail closed when this is a later PlayTrigger of that source card.
    for(int i=0;i<entities->size;++i){
        Obj* e=entities->items->m_Items[i];if(!e)continue;
        int id=trickEntityIdField[e].Get();if(id<0 || id>=effectId)continue;
        auto f=BNM::Class(e).GetField("Components").cast<BNM::Structures::Mono::List<Obj*>*>();
        auto* components=f.IsValid()?f[e].Get():nullptr;
        if(!components || components->size<0 || components->size>128 || (components->size && !components->items))continue;
        bool otherPlay=false;int otherOwner=-1;
        for(int j=0;j<components->size;++j){
            Obj* c=components->items->m_Items[j];if(!c)continue;
            auto* cl=BNM::Class(c).GetClass();const char* name=cl?cl->name:nullptr;if(!name)continue;
            if(!std::strcmp(name,"PlayTrigger"))otherPlay=true;
            if(!std::strcmp(name,"EffectEntity"))otherOwner=unearthEffectOwnerSourceField[c].Get();
        }
        if(otherPlay && otherOwner==ownerId)return false;
    }
    return true;
}
// The concrete target generator is GameStateUtils, already hooked by
// TrickValidTargetsTrace. Observe its returned list from that same hook rather
// than double-patching a native entry point.
inline void TrickLogGeneratedTargets(int player, int cardId, Obj* result) {
    const int guid = TrickGenerationGuid(cardId);
    if (!TrickFocus(guid) || ++trickGenerationEvents>100) return;
    auto* list = reinterpret_cast<IntList*>(result);
    if (!list || list->size < 0 || list->size > 256 || (list->size && !list->items)) {
        LOG_INFO("CustomComponents/Pick3/TRICK_GENERATION: phase=TARGETS guid=%d card=%d player=%d result=%p opaque=1", guid, cardId, player, result);
        return;
    }
    LOG_INFO("CustomComponents/Pick3/TRICK_GENERATION: phase=TARGETS guid=%d card=%d player=%d count=%d", guid,cardId,player,list->size);
    for (int i=0; i<std::min(list->size,16); ++i) {
        int id=list->items->m_Items[i];
        LOG_INFO("CustomComponents/Pick3/TRICK_GENERATION: phase=TARGET guid=%d card=%d index=%d entity=%d targetGuid=%d",guid,cardId,i,id,TrickGenerationGuid(id));
    }
}
inline Obj* TrickGeneratedPlays(Obj* self, int player, int cardId, Obj* validator, const void* method) {
    ScopedGenerationReviveRecognition generationScope(self);
    const auto previousSole=stockSoleInitialPrimary;
    stockSoleInitialPrimary={};
    const int scopedGuid = TrickGenerationGuid(cardId);
    PF3_REPAIR_TRACE("CustomComponents/Pick3/REPAIR1713: GENERATION_IDENTITY card=%d guid=%d player=%d deciderModel=%p root=%p",
        cardId,scopedGuid,player,(void*)generationScope.model,(void*)Target(boundEntityModelRoot));
    const unsigned priorDepth = trickValidationDepth;
    const int priorGuid = trickValidationGuid, priorCard = trickValidationCard;
    const unsigned priorOrders = trickOrderCalls, priorValidations = trickValidatorCalls;
    const auto priorMatrix = pick3Stage1Matrix;
    if (TrickFocus(scopedGuid)) {
        trickValidationDepth = priorDepth + 1;
        trickValidationGuid = scopedGuid;
        trickValidationCard = cardId;
        trickOrderCalls = 0;
        trickValidatorCalls = 0;
        ++trickTraceGenerationCount;
        pick3Stage1Matrix = {};
    }
    const bool pick3Revive = IsGenericPick3ReviveBurst(cardId);
    if (pick3Revive && trickTraceGenerationCount <= 96)
        LOG_INFO("CustomComponents/Pick3/PICK3_PAIR_CONTEXT: phase=GENERATION_ENTER guid=%d card=%d player=%d zone=%d validator=%p finderContext=not-captured primaryProducer=missing pairVerified=0", scopedGuid,cardId,player,FindDeferredInitialZone(cardId),validator);
    Obj* result = originalDeciderValidPlays ? originalDeciderValidPlays(self, player, cardId, validator, method) : nullptr;
    if (pick3Revive && trickTraceGenerationCount <= 160) {
        const auto& m = pick3Stage1Matrix;
        LOG_INFO("CustomComponents/Pick3/PICK3_STAGE1_REJECTION_MATRIX: guid=%d card=%d player=%d firstTargeting=%u/%u oneTime=%u/%u composite=%u/%u fighter=%u/%u openLane=%u/%u inHand=%u/%u null=%u/%u scope=stockGeneration override=0 pairVerified=0",
                 scopedGuid, cardId, player,
                 m.targetingRejects,m.targetingCalls,m.oneTimeRejects,m.oneTimeCalls,
                 m.compositeRejects,m.compositeCalls,m.fighterRejects,m.fighterCalls,
                 m.openLaneRejects,m.openLaneCalls,m.inHandRejects,m.inHandCalls,
                 m.nullRejects,m.nullCalls);
    }
    if (TrickFocus(scopedGuid) && trickTraceGenerationCount <= 160)
        LOG_INFO("CustomComponents/Pick3/TRICK_VALIDATION: phase=SUMMARY guid=%d card=%d player=%d validator=%p ordersCalls=%u tracedValidateCalls=%u coverage=partial",
                 scopedGuid, cardId, player, validator, trickOrderCalls, trickValidatorCalls);
    if (TrickFocus(scopedGuid)) {
        trickValidationDepth = priorDepth;
        trickValidationGuid = priorGuid;
        trickValidationCard = priorCard;
        trickOrderCalls = priorOrders;
        trickValidatorCalls = priorValidations;
        pick3Stage1Matrix = priorMatrix;
    }
    const int guid=TrickGenerationGuid(cardId);
    if (IsGenericPick3ReviveBurst(cardId)) {
        // Capture stock candidate count *before* modifying the single generated
        // Play's target. A casting-zone placeholder is not proof of legality.
        auto* stock = reinterpret_cast<BNM::Structures::Mono::List<Obj*>*>(result);
        const int count = stock && stock->size>=0 && stock->size<=256 &&
            (stock->size==0 || stock->items) ? stock->size : -1;
        LOG_INFO("CustomComponents/Pick3/PICK3_LEGALITY_AUDIT: phase=BEFORE_NORMALIZE guid=%d card=%d player=%d stockPlays=%d",guid,cardId,player,count);
        // Follow the exact stock result even when the card's only playable
        // state is a full board with a team-up destination. Never synthesize
        // a Play: legality remains owned by the stock decider/validator.
        // This trace correlates a missing Play with the previously computed
        // initial-zone candidate without inspecting transient managed lists.
        static unsigned zeroPlayAuditSamples = 0;
        if (count == 0 && zeroPlayAuditSamples++ < 240) {
            LOG_INFO("CustomComponents/Pick3/UNEARCH_ZERO_PLAY: guid=%d card=%d player=%d initialZone=%d validator=%p hypothesis=stock-lane-placement-gate",
                     guid, cardId, player, FindDeferredInitialZone(cardId), validator);
        }
    }
    // Snapshot only the single validated zone play (not the pooled list).
    if(kDeferredPick3TrialEnabled && IsGenericPick3ReviveBurst(cardId)) {
        deferredValidatedPlay={};
        auto* candidates=reinterpret_cast<BNM::Structures::Mono::List<Obj*>*>(result);
        if(candidates && candidates->size==1 && candidates->items && trickPlayCardIdField.IsValid() &&
           trickPlayPlayerIdField.IsValid() && trickPlayTypeField.IsValid() && trickPlayTargetsField.IsValid()) {
            Obj* candidate=candidates->items->m_Items[0];
            if(candidate && trickPlayCardIdField[candidate].Get()==cardId &&
               trickPlayPlayerIdField[candidate].Get()==player &&
               trickPlayTypeField[candidate].Get()==0) {
                auto* ids=trickPlayTargetsField[candidate].Get();
                const int zone=FindDeferredInitialZone(cardId);
                if (ids && ids->size==1 && ids->items && zone>=0) {
                    const int generatedTarget=ids->items->m_Items[0];
                    const auto& sole=stockSoleInitialPrimary;
                    // The initial Play is deliberately generated for the casting
                    // entity. Its target is NOT the first effect's selected card.
                    // Bind the synchronous one-option candidate snapshot to the
                    // actual first effect/model, without modifying the generated
                    // casting Play (which the drag/UI logic depends on).
                    const auto firstProof=ResolvePick3InitialEffectOnModel(generationScope.model,cardId);
                    const bool exactSole=generatedTarget==zone &&
                        sole.card==cardId && sole.target>=0 &&
                        sole.target!=cardId && !sole.ambiguous &&
                        firstProof.complete && firstProof.effectId==sole.effect &&
                        sole.model && sole.model==generationScope.model &&
                        TrickGenerationGuid(sole.target)>0;
                    if (exactSole) {
                        deferredValidatedPlay={cardId,zone,player};
                        deferredValidatedPlay.stockSolePrimary=sole.target;
                        PF3_REPAIR_TRACE("CustomComponents/Pick3/REPAIR1717: SOLE_INITIAL_BOUND card=%d guid=%d cast=%d sole=%d effect=%d player=%d model=%p source=validatorPrimarySnapshot",
                            cardId,guid,zone,sole.target,sole.effect,player,(void*)sole.model);
                    } else if (generatedTarget==zone) {
                        deferredValidatedPlay={cardId,zone,player};
                        LOG_INFO("CustomComponents/Pick3/INITIAL_PLAY_BRIDGE: phase=ALREADY_ZONE guid=%d card=%d player=%d zone=%d",guid,cardId,player,zone);
                    } else {
                        // The original decider returned this Play as legal using the
                        // discarded-card target. Preserve that stock eligibility
                        // evidence, but make its initial Play destination the lane.
                        // Only modify the single generated Play for 712/713. Never
                        // touch the effect's own target finder or selection Play.
                        const int eligibleGuid=TrickGenerationGuid(generatedTarget);
                        const int zoneGuid=TrickGenerationGuid(zone);
                        if (generatedTarget>=0 && generatedTarget!=cardId &&
                            eligibleGuid>0 && zoneGuid<0) {
                            ids->items->m_Items[0]=zone;
                            deferredValidatedPlay={cardId,zone,player};
                            LOG_INFO("CustomComponents/Pick3/INITIAL_PLAY_BRIDGE: phase=NORMALIZED guid=%d card=%d player=%d eligibleEntity=%d eligibleGuid=%d zone=%d",guid,cardId,player,generatedTarget,eligibleGuid,zone);
                        } else {
                            LOG_INFO("CustomComponents/Pick3/INITIAL_PLAY_BRIDGE: phase=SKIP guid=%d card=%d player=%d target=%d targetGuid=%d zone=%d zoneGuid=%d",guid,cardId,player,generatedTarget,eligibleGuid,zone,zoneGuid);
                        }
                    }
                }
            }
        }
    }
    if (deferredValidatedPlay.card==cardId && deferredValidatedPlay.zone>=0 &&
        deferredValidatedPlay.player==player) {
        Obj* generationModel = ReviveRecognitionModel();
        const auto first = ResolvePick3InitialEffectOnModel(generationModel,cardId);
        if (first.complete && first.effectId>=0 && generationScope.model == generationModel &&
            generationModel && guid > 0) {
            CaptureRoot(reviveLegalPlayModelRoot,generationModel);
            if (Target(reviveLegalPlayModelRoot) != generationModel) {
                PF3_REPAIR_TRACE("CustomComponents/Pick3/REPAIR1713: PROOF_REJECT card=%d reason=root-failed",cardId);
                deferredValidatedPlay={};
            } else {
            deferredValidatedPlay.guid=guid;
            deferredValidatedPlay.effect=first.effectId;
            deferredValidatedPlay.provenAt=std::chrono::steady_clock::now();
            PF3_REPAIR_TRACE("CustomComponents/Pick3/REPAIR1712: STOCK_PLAY_PROOF card=%d guid=%d effect=%d player=%d zone=%d model=%p source=stockDecider",
                cardId,guid,first.effectId,player,deferredValidatedPlay.zone,(void*)generationModel);
            PF3_REPAIR_TRACE("CustomComponents/Pick3/REPAIR1713: PROOF_ROOT card=%d guid=%d effect=%d zone=%d model=%p rooted=%d",
                cardId,guid,first.effectId,deferredValidatedPlay.zone,(void*)generationModel,
                Target(reviveLegalPlayModelRoot)==generationModel?1:0);
            }
        } else {
            PF3_REPAIR_TRACE("CustomComponents/Pick3/REPAIR1712: STOCK_PLAY_UNPROVEN card=%d guid=%d zone=%d reason=FIRST_EFFECT_UNAVAILABLE",
                cardId,guid,deferredValidatedPlay.zone);
            deferredValidatedPlay={};
        }
    } else if (kDeferredPick3TrialEnabled && pick3Revive) {
        PF3_REPAIR_TRACE("CustomComponents/Pick3/REPAIR1712: STOCK_PLAY_UNPROVEN card=%d guid=%d zone=%d reason=NO_VALIDATED_SINGLE_PLAY",
            cardId,guid,FindDeferredInitialZone(cardId));
    }
    stockSoleInitialPrimary=previousSole;
    if (!TrickFocus(guid) || ++trickGenerationEvents>100) return result;
    auto* list = reinterpret_cast<BNM::Structures::Mono::List<Obj*>*>(result);
    if (!list || list->size<0 || list->size>256 || (list->size && !list->items)) {
        LOG_INFO("CustomComponents/Pick3/TRICK_GENERATION: phase=PLAYS guid=%d card=%d result=%p opaque=1",guid,cardId,result);
        return result;
    }
    LOG_INFO("CustomComponents/Pick3/TRICK_GENERATION: phase=PLAYS guid=%d card=%d player=%d count=%d",guid,cardId,player,list->size);
    for (int i=0; i<std::min(list->size,12); ++i) {
        Obj* play=list->items->m_Items[i];
        if (!play) continue;
        auto* bytes=reinterpret_cast<const std::uint8_t*>(play);
        int type=*reinterpret_cast<const int*>(bytes+0x10);
        int source=*reinterpret_cast<const int*>(bytes+0x14);
        int owner=*reinterpret_cast<const int*>(bytes+0x18);
        auto* targets=*reinterpret_cast<IntList* const*>(bytes+0x20);
        int order=*reinterpret_cast<const int*>(bytes+0x28);
        int count=(targets && targets->size>=0 && targets->size<=32 && (targets->size==0 || targets->items)) ? targets->size:-1;
        LOG_INFO("CustomComponents/Pick3/TRICK_GENERATION: phase=PLAY guid=%d index=%d type=%d source=%d owner=%d order=%d targetCount=%d",guid,i,type,source,owner,order,count);
        if(count>0) for(int j=0;j<std::min(count,8);++j) {
            int id=targets->items->m_Items[j];
            LOG_INFO("CustomComponents/Pick3/TRICK_GENERATION: phase=PLAY_TARGET guid=%d playIndex=%d index=%d entity=%d targetGuid=%d",guid,i,j,id,TrickGenerationGuid(id));
        }
    }
    return result;
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
// Restore the two *read-only* service dependencies without re-entering the
// match's DI binder. Patch 1.5.5 quarantined ResolveRepairDependencies(),
// unintentionally leaving revive recognition and the new UI without their
// live EntityModel. The same stock SelectTargetService already owns both.
inline void CaptureDirectServiceDependencies() {
    Obj* service=Target(serviceRoot);
    if(!service || !localClass.IsValid() || !BNM::IsA(service,localClass)) return;
    if(!Target(boundEntityModelRoot) && localServiceFinder.IsValid() &&
       TargetBridge::finderEntityFinder.IsValid() && TargetBridge::entityFinderModel.IsValid()) {
        Obj* finder=localServiceFinder[service].Get();
        Obj* entityFinder=finder ? TargetBridge::finderEntityFinder[finder].Get() : nullptr;
        Obj* model=entityFinder ? TargetBridge::entityFinderModel[entityFinder].Get() : nullptr;
        if(model && boundEntityModelClass.IsValid() && BNM::IsA(model,boundEntityModelClass)) {
            CaptureRoot(boundEntityModelRoot,model);
            LOG_INFO("CustomComponents/Pick3/SERVICE_DIRECT_BIND: entityModel=%p source=local-select-finder DI=NOT_INVOKED",model);
        }
    }
    if(!Target(boundEffectsRoot) && localServiceEffects.IsValid() && effectQueueConcrete.IsValid()) {
        Obj* queue=localServiceEffects[service].Get();
        if(queue && BNM::IsA(queue,effectQueueConcrete)) {
            CaptureRoot(boundEffectsRoot,queue);
            LOG_INFO("CustomComponents/Pick3/SERVICE_DIRECT_BIND: effectQueue=%p source=local-select-service DI=NOT_INVOKED",queue);
        }
    }
}
inline void CaptureService(Obj* service, const char* source) {
    if (!enabled || !service || (!BNM::IsA(service, localClass) && !BNM::IsA(service, remoteClass))) return;
    CaptureRoot(serviceRoot, service);
    CaptureDirectServiceDependencies();
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
inline bool HasArmedStockHandoff(); // forward; stock handoff state is declared later
static void GameInputHolderUpdated(Obj* self) {
    if (!enabled || !self) return;
    if (stateVisualTurnPhase.IsValid() && visualSubTurn.IsValid()) {
        Obj* phase = stateVisualTurnPhase[self].Get();
        observedVisualPhase = phase != nullptr;
        const int subturn = phase ? visualSubTurn[phase].Get() : -1;
        observedLocalTargetSubTurn = subturn == 1;
        NetworkTrace::ObserveVisualSubturn(phase, subturn, overlayCapturesInput);
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
            // Never borrow match-bound dependencies from the previous binder.
            Drop(boundSignalRoot); Drop(boundEffectsRoot); Drop(boundEntityModelRoot);
            Drop(reviveLegalPlayModelRoot);
            deferredValidatedPlay={};
            pendingRefresh=false;
            pendingHandHighlights=false;
            CaptureRoot(binderRoot, binder);
            CC_DIAG("CustomComponents/Pick3/SUBMIT: BINDER_CAPTURE source=GameInputStateHolder rooted=%d", binderRoot ? 1 : 0);
        }
    }
    // Binder replacement above may invalidate old rooted dependencies.
    // Reacquire only through safe field reads on the already-rooted service.
    CaptureDirectServiceDependencies();
    PumpDefaultRepair();
    // A refresh ticket is issued only after a completed custom selection and
    // the stock state-repair signal. The command itself checks the input state,
    // injected dependencies, and waits for two stable UI frames.
    PumpHandHighlights(self);
    // Stock EndTurnButtonViewMediator has an Interrupted-state bug after
    // successive manual choices (including PF3). The recovery was previously
    // registered but NEVER driven from a frame observer. This is the existing
    // GameInputStateHolder.Update observer: no new IL2CPP inline detour.
    if (TargetButtonTrace::chooseSerial!=TargetButtonTrace::recoveredSerial &&
        TargetButtonTrace::chooseSerial!=0) {
        const auto& observed=Systems::TargetingPresentationRefreshSystem::observedTargetingIdentity;
        // Do not resume the Pass-to-Combat button if the current stock queue
        // front is another manual choice, even if holder state is briefly 3/4.
        Obj* front=nullptr;
        auto& first=Systems::TargetingPresentationRefreshSystem::getFirstEffectInQueue;
        if (first.IsValid()) front=first[self]();
        const bool frontIsTarget=front && chooseBlockingClass.IsValid() &&
            BNM::IsA(front,chooseBlockingClass);
        const bool noActiveTarget=(!observed.valid &&
            observed.state!=6 && observed.state!=7 && !frontIsTarget &&
            !overlayCapturesInput && !TargetBridge::HasQueuedChoice() &&
            !HasArmedStockHandoff());
        TargetButtonTrace::TryRecoverAfterCompletedLocalPhase(noActiveTarget, true);
    }
}
// Resolve the same scene's injected singleton objects used by stock commands.
// This avoids relying on a SelectTargetCommand, which custom pickers bypass.
inline void ResolveRepairDependencies(bool force) {
    if (!force && ++repairDependencyPoll % 60 != 1) return;
    Obj* binder=Target(binderRoot);
    if (!binder || !getInjectionBinder.IsValid() || !getInjectedInstance.IsValid()) return;
    Obj* injector=nullptr;
    const auto e=BNM::TryInvoke([&] {injector=getInjectionBinder[binder]();});
    if (e.IsValid() || !injector) return;
    const auto lookup=[&](BNM::Class& klass, std::uint32_t& root) {
        if (Target(root) || !klass.IsValid()) return;
        Obj* found=nullptr;
        const auto x=BNM::TryInvoke([&] {
            found=getInjectedInstance[injector](klass.GetMonoType(), static_cast<Obj*>(nullptr));
        });
        if (!x.IsValid() && found && (BNM::IsA(found,klass) || &klass==&effectCollectionInterface))
            CaptureRoot(root,found);
    };
    lookup(gameStateUpdatedSignalClass,boundSignalRoot);
    lookup(effectCollectionInterface,boundEffectsRoot);
    lookup(boundEntityModelClass,boundEntityModelRoot);
}
// Read all three live lists. A response-list transfer isn't retirement.
inline bool EffectAbsentFromQueue(Obj* queue, int effect, int group, bool& safe) {
    safe=false;
    if (!queue || !effectQueueConcrete.IsValid() || !BNM::IsA(queue,effectQueueConcrete) ||
        !boundQueueEffects.IsValid() || !liveEffectEntityId.IsValid() || !liveEffectGroupId.IsValid()) return false;
    bool activeOtherBlocking=false;
    const auto scan=[&](BNM::Structures::Mono::List<Obj*>* entries,bool frontList)->bool {
        if (!entries || entries->size<0 || entries->size>4096 ||
            (entries->size>0 && !entries->items)) {safe=false;return true;}
        for (int i=0; i<entries->size; ++i) {
            Obj* entry=entries->items->m_Items[i];
            if (!entry) continue;
            if (liveEffectEntityId[entry].Get()==effect && liveEffectGroupId[entry].Get()==group)
                return true;
            if (frontList && i==0 && chooseBlockingClass.IsValid() &&
                BNM::IsA(entry,chooseBlockingClass)) activeOtherBlocking=true;
        }
        return false;
    };
    auto* all=boundQueueEffects[queue].Get();
    auto* responses=boundQueueResponses.IsValid()?boundQueueResponses[queue].Get():nullptr;
    auto* groups=boundQueueGroupResponses.IsValid()?boundQueueGroupResponses[queue].Get():nullptr;
    // Missing or malformed lists cannot authorize a synthetic notification.
    if (!all || !responses || !groups) return false;
    if (all->size<0 || responses->size<0 || groups->size<0 ||
        all->size>4096 || responses->size>4096 || groups->size>4096) return false;
    safe=true;
    const bool present=scan(all,true) || scan(responses,true) || scan(groups,true);
    return safe && !present && !activeOtherBlocking;
}
// Highlight observer state must be declared before PumpDefaultRepair uses it.
inline unsigned stockHighlightSinceRefresh = 0;
inline int lastRefreshEffect = -1, lastRefreshGroup = -1;
inline void PumpDefaultRepair() {
    if (!enabled) return;
    // Patch 1.5.5: don't resolve injected services from the frame-time repair
    // pump, especially while mulligan/scene initialization is in progress.
    // The old resolver called getInjectionBinder[binder]() through TryInvoke
    // on a not-yet-ready IL2CPP instance and native-crashed at null+0x38.
    // Synthetic refresh is already disabled, so missing roots need no repair.
    if (!Target(boundEffectsRoot) && pendingRefresh) {
        LOG_INFO("CustomComponents/Pick3/CRASH_FIX_155: repair roots absent; skip early dependency resolution and wait for stock scene lifecycle");
        pendingRefresh=false;
        pendingRefreshAge=pendingStableFrames=0;
    }
    if (pendingRemoteRetirement) {
        // The only positive evidence here is the effect disappearing from the
        // already-bound queue. This is NOT proof of server acceptance; label it
        // as observed local-state retirement, never as a network ACK.
        bool retirementSafe = false;
        Obj* observedQueue = Target(boundEffectsRoot);
        const bool absent = observedQueue &&
            EffectAbsentFromQueue(observedQueue, remoteRetirementEffect,
                                  remoteRetirementGroup, retirementSafe);
#if CUSTOMCOMPONENTS_PF3_REPAIR_TRACE
        if ((remoteRetirementPolls % 120)==0 || absent) {
            const auto& holder=Systems::TargetingPresentationRefreshSystem::observedTargetingIdentity;
            int frontEffect=-1,frontGroup=-1,queueCount=-1,queuedMatches=-1;
            if (observedQueue && effectQueueConcrete.IsValid() && BNM::IsA(observedQueue,effectQueueConcrete) &&
                boundQueueEffects.IsValid()) {
                auto* entries=boundQueueEffects[observedQueue].Get();
                if (entries && entries->size>=0 && entries->size<=4096 &&
                    (entries->size==0 || entries->items)) {
                    queueCount=entries->size;
                    queuedMatches=0;
                    for(int i=0;i<entries->size;++i) {
                        Obj* entry=entries->items->m_Items[i];
                        if(!entry || !liveEffectEntityId.IsValid() || !liveEffectGroupId.IsValid()) continue;
                        const int id=liveEffectEntityId[entry].Get();
                        const int group=liveEffectGroupId[entry].Get();
                        if(i==0) {frontEffect=id;frontGroup=group;}
                        if(id==remoteRetirementEffect && group==remoteRetirementGroup) ++queuedMatches;
                    }
                }
            }
            PF3_REPAIR_TRACE("CustomComponents/Pick3/REPAIR177: RETIRE_OBSERVE effect=%d group=%d polls=%d queue=%p model=%p safe=%d absent=%d front=%d/%d activeCount=%d matchingActive=%d holder=%d/%d holderState=%d phase=%d localSubturn=%d stockSuccesses=%u",
                remoteRetirementEffect,remoteRetirementGroup,remoteRetirementPolls,(void*)observedQueue,
                (void*)Target(modelRoot),int(retirementSafe),int(absent),
                frontEffect,frontGroup,queueCount,queuedMatches,
                holder.effectEntityId,holder.effectGroupId,holder.state,holder.phase,
                int(observedLocalTargetSubTurn),stockTargetSuccesses);
        }
#endif
        if (absent && retirementSafe) {
            LOG_INFO("CustomComponents/Pick3/PVP_RETIRE: STATE_ABSENT effect=%d group=%d polls=%d (local state only; not server ACK)",
                     remoteRetirementEffect, remoteRetirementGroup, remoteRetirementPolls);
            LOG_INFO("CustomComponents/Pick3/PICK3_TRIGGER_PROVENANCE: phase=QUEUE_RETIRED_LOCAL_ONLY effect=%d group=%d remote=1 verifiedServerAck=0 dispatch=deferred",remoteRetirementEffect,remoteRetirementGroup);
            pendingRemoteRetirement = false;
        } else if (++remoteRetirementPolls == 600) {
            LOG_ERR("CustomComponents/Pick3/PVP_RETIRE: STILL_PENDING effect=%d group=%d (no authoritative confirmation)",
                    remoteRetirementEffect, remoteRetirementGroup);
            pendingRemoteRetirement = false;
        }
    }
    if (!pendingRefresh) return;
    // If stock subsequently completed a target command, never send a second
    // presentation update for an uncertain overlapping lifecycle.
    if (stockTargetSuccesses != pendingStockSuccessBaseline) {
        pendingRefresh=false;
        CC_DIAG("CustomComponents/Pick3/DEFAULT_REPAIR: REFRESH_CANCELED_STOCK_COMMAND");
        return;
    }
    if (!observedLocalTargetSubTurn) {
        // Stock, not a synthetic Signal.Dispatch, restored the normal play phase.
        LOG_INFO("CustomComponents/Pick3/PATCH154_STOCK_PHASE_RESTORED: effect=%d group=%d queued=%d capture=%d stockSuccess=%u signalDispatch=0",
                 pendingRefreshEffect, pendingRefreshGroup,
                 TargetBridge::HasQueuedChoice()?1:0, overlayCapturesInput?1:0,
                 stockTargetSuccesses);
        pendingRefresh=false;
        pendingRefreshAge=pendingStableFrames=0;
        return;
    }
    // Engine may need several frames to drain its normal effect processing.
    if (++pendingRefreshAge>600) {
        LOG_ERR("CustomComponents/Pick3/DEFAULT_REPAIR: REFRESH_EXPIRED effect=%d group=%d",pendingRefreshEffect,pendingRefreshGroup);
        pendingRefresh=false;return;
    }
    bool safe=false;
    if (!EffectAbsentFromQueue(Target(boundEffectsRoot),pendingRefreshEffect,pendingRefreshGroup,safe)) {
        pendingStableFrames=0;return;
    }
    if (++pendingStableFrames<2) return;
    Obj* signal=Target(boundSignalRoot);
    if (!safe || !signal || !gameStateUpdatedSignalClass.IsValid() ||
        !BNM::IsA(signal,gameStateUpdatedSignalClass) || !dispatchGameStateUpdated.IsValid()) return;
    // Patch 1.5.3: matched native crash mitigation.
    // libsprig BuildId e334967f... frame +0x2A6EB0 resolves to
    // BNM::TryInvoke(dispatchGameStateUpdated[signal]) in this path.
    // The stock signal can be present yet have invalid injected subscribers
    // or expired scene/model context. Never synthesize this event from a
    // post-selection timer; normal stock GameStateUpdated remains untouched.
    const int effect=pendingRefreshEffect, group=pendingRefreshGroup;
    // Keep the ticket until STOCK naturally exits the target subturn.
    // Do not invoke GameStateUpdated or mutate the game's input/command state.
    if (pendingStableFrames==2)
        LOG_INFO("CustomComponents/Pick3/PATCH154_WAIT_STOCK_PHASE: effect=%d group=%d targetSubturn=%d queueRetired=1 signalDispatch=0",
                 pendingRefreshEffect, pendingRefreshGroup, observedLocalTargetSubTurn?1:0);
    pendingStableFrames=0;
    LOG_INFO("CustomComponents/Pick3/CRASH_FIX_153: SYNTHETIC_REFRESH_SKIPPED effect=%d group=%d reason=matched-native-0x2A6EB0", effect, group);

}
// One-time presentation-only resync. The engine owns all legal-play decisions:
// we call its existing HighlightPlayableCardsCommand with a fully injected,
// match-scoped instance, never dispatch playable signals based on our own guess.
// No Play, model mutation, command Retain/Release, or visual subturn forcing.
// Stock-owned highlight lifecycle observer. No injected command construction.
inline unsigned stockHighlightObservations = 0;
// Returned lists are ordinary managed List<int>, not pooled validator lists.
// Read only their bounded in-memory snapshot. Never invoke collection methods.
using StockPlayableCardsFn = IntList*(*)(Obj*, const void*);
inline StockPlayableCardsFn originalStockPlayableCards = nullptr;
using StockShouldHighlightFn = bool(*)(Obj*, const void*);
inline StockShouldHighlightFn originalStockShouldHighlight = nullptr;
inline unsigned stockPlayableAuditSamples = 0;
inline unsigned stockShouldHighlightSamples = 0;
inline IntList* StockPlayableCardsObserved(Obj* self, const void* method) {
    auto* result = originalStockPlayableCards ? originalStockPlayableCards(self, method) : nullptr;
    if (++stockPlayableAuditSamples <= 240) {
        int n = -1, ids[12]{};
        if (result && result->size >= 0 && result->size <= 512 &&
            (result->size == 0 || result->items)) {
            n = result->size;
            for (int i=0; i<std::min(n,12); ++i) ids[i] = result->items->m_Items[i];
        }
        LOG_INFO("CustomComponents/Pick3/PLAYABLE_AUDIT: phase=STOCK_LIST cache=%p count=%d first12=%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d effect=%d group=%d",
                 self,n,ids[0],ids[1],ids[2],ids[3],ids[4],ids[5],ids[6],ids[7],ids[8],ids[9],ids[10],ids[11],lastRefreshEffect,lastRefreshGroup);
    }
    return result;
}
inline bool StockShouldHighlightObserved(Obj* self, const void* method) {
    const bool result = originalStockShouldHighlight ? originalStockShouldHighlight(self,method) : false;
    if (++stockShouldHighlightSamples <= 240) {
        Obj* cache = (self && highlightCommandPlayCache.IsValid()) ? highlightCommandPlayCache[self].Get() : nullptr;
        LOG_INFO("CustomComponents/Pick3/PLAYABLE_AUDIT: phase=DISPLAY_PERMISSION command=%p cache=%p permitted=%d effect=%d group=%d",
                 self,cache,result?1:0,lastRefreshEffect,lastRefreshGroup);
    }
    return result;
}
// Observe the actual effect chosen by stock. This is a borrowed Entity pointer;
// only its stable Id is read while the original call's result remains alive.
using FirstTargetedEffectFn = Obj*(*)(Obj*, int, const void*);
inline FirstTargetedEffectFn originalFirstTargetedEffect = nullptr;
inline unsigned firstTargetedEffectSamples = 0;
inline Obj* FirstTargetedEffectObserved(Obj* self, int cardId, const void* method) {
    Obj* effect = originalFirstTargetedEffect ? originalFirstTargetedEffect(self,cardId,method) : nullptr;
    if (trickValidationDepth && IsGenericPick3ReviveBurst(cardId) && ++firstTargetedEffectSamples <= 160) {
        int effectId = -1;
        if (effect && trickEntityIdField.IsValid()) effectId = trickEntityIdField[effect].Get();
        LOG_INFO("CustomComponents/Pick3/UNEARTH_EFFECT_METADATA: phase=FIRST_TARGETED card=%d guid=%d effectId=%d found=%d",
                 cardId,trickValidationGuid,effectId,effect?1:0);
    }
    return effect;
}

// Only observe native call boundaries; never inspect PoolableList internals.
using StockCachePlaysFn = void(*)(Obj*, Obj*, const void*);
inline StockCachePlaysFn originalStockCachePlays = nullptr;
inline Execute originalStockRefreshPlayable = nullptr;
using StockHighlightUpdateFn = void(*)(Obj*, Obj*, const void*);
inline StockHighlightUpdateFn originalStockHighlightUpdate = nullptr;
// Snapshot stock PlayCache.PlayableCards at its declared IL2CPP field offset
// from the already installed RefreshPlayableCardsList observer.  No additional
// native detour or managed ICollection call is required.
inline BNM::Field<IntList*> stockPlayableCardsField;
inline unsigned stockSafePlayableSnapshots = 0;
inline void AuditPlayableCardsFromRefresh(Obj* cache) {
    if (!cache || !stockPlayableCardsField.IsValid() ||
        lastRefreshEffect < 0 || stockSafePlayableSnapshots++ >= 160) return;
    auto* list = stockPlayableCardsField[cache].Get();
    if (!list) {
        LOG_INFO("CustomComponents/Pick3/PLAYABLE_AUDIT: phase=REFRESH_FIELD cache=%p list=null effect=%d group=%d",cache,lastRefreshEffect,lastRefreshGroup);
        return;
    }
    const int n = list->size;
    if (n < 0 || n > 128 || (n && !list->items)) {
        LOG_INFO("CustomComponents/Pick3/PLAYABLE_AUDIT: phase=REFRESH_FIELD cache=%p invalidCount=%d effect=%d group=%d",cache,n,lastRefreshEffect,lastRefreshGroup);
        return;
    }
    int ids[12]{};
    for(int i=0;i<std::min(n,12);++i) ids[i] = list->items->m_Items[i];
    LOG_INFO("CustomComponents/Pick3/PLAYABLE_AUDIT: phase=REFRESH_FIELD cache=%p count=%d first12=%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d effect=%d group=%d",
             cache,n,ids[0],ids[1],ids[2],ids[3],ids[4],ids[5],ids[6],ids[7],ids[8],ids[9],ids[10],ids[11],lastRefreshEffect,lastRefreshGroup);
}
inline unsigned stockCacheTimelineSamples = 0;
inline thread_local unsigned stockCacheTimelineDepth = 0;
inline void StockCachePlaysObserved(Obj* self, Obj* plays, const void* method) {
    const bool sample = ++stockCacheTimelineSamples <= 300;
    if (sample) LOG_INFO("CustomComponents/Pick3/STOCK_PLAYCACHE: phase=CACHE_ENTER seq=%u cache=%p plays=%p", stockCacheTimelineSamples,self,plays);
    ++stockCacheTimelineDepth;
    if (originalStockCachePlays) originalStockCachePlays(self,plays,method);
    --stockCacheTimelineDepth;
    if (sample) LOG_INFO("CustomComponents/Pick3/STOCK_PLAYCACHE: phase=CACHE_EXIT seq=%u cache=%p",stockCacheTimelineSamples,self);
}
inline void StockRefreshPlayableObserved(Obj* self, const void* method) {
    const bool sample = stockCacheTimelineSamples <= 300;
    if (sample) LOG_INFO("CustomComponents/Pick3/STOCK_PLAYCACHE: phase=REFRESH_ENTER cache=%p nested=%u",self,stockCacheTimelineDepth);
    if (originalStockRefreshPlayable) originalStockRefreshPlayable(self,method);
    AuditPlayableCardsFromRefresh(self);
    if (sample) LOG_INFO("CustomComponents/Pick3/STOCK_PLAYCACHE: phase=REFRESH_EXIT cache=%p",self);
}
inline void StockHighlightUpdateObserved(Obj* self, Obj* allCards, const void* method) {
    if (stockHighlightObservations < 300)
        LOG_INFO("CustomComponents/Pick3/HAND_HIGHLIGHTS: phase=STOCK_UPDATE_ENTER command=%p cardsEnumerable=%p", self,allCards);
    if (originalStockHighlightUpdate) originalStockHighlightUpdate(self,allCards,method);
    // Distinguish a UI update from a stock PlayCache rebuild; a successful
    // UpdateCardsHighlightState does not mean the resulting signals are correct.
    if (stockHighlightObservations < 300 && lastRefreshEffect >= 0)
        LOG_INFO("CustomComponents/Pick3/HAND_HIGHLIGHTS: phase=POST_SELECTION_UPDATE effect=%d group=%d stockExecuteOrdinal=%u cacheNesting=%u",
                 lastRefreshEffect,lastRefreshGroup,stockHighlightSinceRefresh,stockCacheTimelineDepth);
    if (stockHighlightObservations < 300)
        LOG_INFO("CustomComponents/Pick3/HAND_HIGHLIGHTS: phase=STOCK_UPDATE_EXIT command=%p", self);
}
inline Execute originalStockHighlightExecute = nullptr;
inline void StockHighlightExecuteObserved(Obj* self, const void* method) {
    if (stockHighlightObservations < 240) {
        ++stockHighlightObservations;
        if (lastRefreshEffect >= 0 && stockHighlightSinceRefresh < 3) {
            LOG_INFO("CustomComponents/Pick3/HAND_HIGHLIGHTS: phase=POST_SELECTION_STOCK_EXECUTE effect=%d group=%d ordinal=%u command=%p",
                     lastRefreshEffect,lastRefreshGroup,++stockHighlightSinceRefresh,self);
        }
        LOG_INFO("CustomComponents/Pick3/HAND_HIGHLIGHTS: phase=STOCK_EXECUTE_ENTER command=%p sample=%u",
                 self, stockHighlightObservations);
    }
    if (originalStockHighlightExecute) originalStockHighlightExecute(self, method);
    if (stockHighlightObservations <= 240)
        LOG_INFO("CustomComponents/Pick3/HAND_HIGHLIGHTS: phase=STOCK_EXECUTE_EXIT command=%p sample=%u",
                 self, stockHighlightObservations);
}
inline void PumpHandHighlights(Obj* holder) {
    // Deliberate fail-closed safety guard until a stock-owned command lifecycle
    // is established. This suppresses the experimental manual Execute path.
    (void)holder;
    pendingHandHighlights=false;
    return;
#if 0
    if (!enabled || !pendingHandHighlights || !holder) return;
    if (++pendingHandHighlightAge > 180) {
        if (NetworkTrace::Mode()) LOG_INFO("CustomComponents/Pick3/HAND_HIGHLIGHTS: EXPIRED no valid stock highlight command/state");
        pendingHandHighlights=false;
        return;
    }
    if (pendingRefresh || overlayCapturesInput || TargetBridge::HasQueuedChoice() ||
        !stateVisualTurnPhase.IsValid() || !visualSubTurn.IsValid() ||
        !holderInputState.IsValid() || !highlightPlayableCardsClass.IsValid() ||
        !executeHighlightPlayableCards.IsValid()) {
        pendingHandHighlightStable=0;
        return;
    }
    Obj* phase=stateVisualTurnPhase[holder].Get();
    if (!phase || visualSubTurn[phase].Get()==1) {
        pendingHandHighlightStable=0;
        return;
    }
    const int state=holderInputState[holder].Get();
    // These are the game's "SelectCardToPlayInactive/Active" states, not a
    // target selection or ongoing animation. Stock command decides legality.
    if (state!=3 && state!=4) {
        pendingHandHighlightStable=0;
        return;
    }
    if (++pendingHandHighlightStable<2) return;
    Obj* binder=Target(binderRoot);
    if (!binder || !getInjectionBinder.IsValid() || !getInjectedInstance.IsValid()) return;
    Obj* injector=nullptr;
    const auto injectorException=BNM::TryInvoke([&] {injector=getInjectionBinder[binder]();});
    if (injectorException.IsValid() || !injector) return;
    Obj* command=nullptr;
    const auto instanceException=BNM::TryInvoke([&] {
        command=getInjectedInstance[injector](highlightPlayableCardsClass.GetMonoType(),static_cast<Obj*>(nullptr));
    });
    if (instanceException.IsValid() || !command || !BNM::IsA(command,highlightPlayableCardsClass)) return;
    // Only invoke an injected command with ALL stock dependencies available.
    if (!highlightCommandInputState.IsValid() || !highlightCommandPlayCache.IsValid() ||
        !highlightCommandModel.IsValid() || !highlightCommandFacade.IsValid() ||
        !highlightCommandPlayable.IsValid() || !highlightCommandUnplayable.IsValid() ||
        !highlightCommandPlayCache[command].Get() || !highlightCommandModel[command].Get() ||
        !highlightCommandFacade[command].Get() || !highlightCommandPlayable[command].Get() ||
        !highlightCommandUnplayable[command].Get()) return;
    // Respect the injected command's state instead of rewriting pooled data.
    if (highlightCommandInputState[command].Get()!=state) {
        if (NetworkTrace::Mode() && pendingHandHighlightAge==15)
            LOG_INFO("CustomComponents/Pick3/HAND_HIGHLIGHTS: WAIT_STATE actual=%d expected=%d",
                highlightCommandInputState[command].Get(),state);
        return;
    }
    // Consume before calling managed code because dispatching can re-enter.
    pendingHandHighlights=false;
    const auto ex=BNM::TryInvoke([&] {executeHighlightPlayableCards[command]();});
    if (ex.IsValid()) {
        LOG_ERR("CustomComponents/Pick3/HAND_HIGHLIGHTS: stock Execute exception: %s", ex.Message().c_str());
    } else {
        ++handHighlightRefreshCount;
        if (NetworkTrace::Mode())
            LOG_INFO("CustomComponents/Pick3/HAND_HIGHLIGHTS: STOCK_REFRESH state=%d count=%u",state,handHighlightRefreshCount);
    }
#endif
}

// Reconstruct the native PickedFromThree query context from a received stock
// Play.Type=SelectTarget BEFORE the local game service evaluates its effects.
// Both PvP peers may run this method; only real custom effects are handled.
static void LocalSelectProcess(Obj* self,Obj* play,const void* method) {
    TransactionRuntime::Submission receiverSubmission;
    ScopedReceiveReviveRecognition receiveRecognitionScope(self, play);
    TargetBridge::ScopedValidationRecipient bridgeContext;
    bool useValidationBridge=false;
    // Patch 1.7.7: receiver-side mode bridge, scoped strictly to the native
    // Process() of a valid incoming live-PF3 stock Play. Peer B's filter is
    // normally mode 3/4, while peer A's overlay has temporarily set mode 1.
    Obj* validatedLiveFilter=nullptr;
    int validatedLiveMode=-1;
    int validatedLiveEffect=-1, validatedLiveGroup=-1;
    int validatedLiveCount=0;
    Obj* blocking=nullptr;  // Queue front shared with pre-stock replica role correction.
    Obj* submissionModel=nullptr;
    if (enabled && self && play && playType.IsValid() && playTargets.IsValid() &&
        playType[play].Get()==6 && localServiceEffects.IsValid() && localServiceFinder.IsValid()) {
        Obj* queue=localServiceEffects[self].Get();
        // No managed Peek() in inbound simulation either. Read only the first
        // concrete queue entry, preserving stock Process() if inspection fails.
        if (queue && effectQueueConcrete.IsValid() && BNM::IsA(queue,effectQueueConcrete) &&
            boundQueueEffects.IsValid()) {
            auto* entries=boundQueueEffects[queue].Get();
            if (entries && entries->size>0 && entries->size<=4096 && entries->items)
                blocking=entries->items->m_Items[0];
            if (NetworkTrace::Mode())
                LOG_INFO("CustomComponents/Pick3/INBOUND_QUEUE: count=%d first=%p noPeek=1",
                    entries ? entries->size : -1,static_cast<void*>(blocking));
        } else if (queue) {
            LOG_ERR("CustomComponents/Pick3/INBOUND_QUEUE: concrete-type-unavailable queue=%p",static_cast<void*>(queue));
        }
        // Selection diagnostic: inspect the physical queue front without Peek()
        // and without changing any queue, filter, play, or completion state.
        if (play && playType.IsValid() && playType[play].Get()==6) {
            int frontEffect=-1, frontGroup=-1, frontOwner=-1;
            bool frontChoose=false;
            if (blocking && liveEffectEntityId.IsValid() && liveEffectGroupId.IsValid()) {
                frontEffect=liveEffectEntityId[blocking].Get();
                frontGroup=liveEffectGroupId[blocking].Get();
                frontChoose=chooseBlockingClass.IsValid() && BNM::IsA(blocking,chooseBlockingClass);
                if (TargetBridge::blockingPlayerId.IsValid()) frontOwner=TargetBridge::blockingPlayerId[blocking].Get();
            }
            const auto* ids=playTargets[play].Get();
            const int count=ids && ids->size>=0 && ids->size<=32 && (ids->size==0 || ids->items) ? ids->size:-1;
            LOG_INFO("CustomComponents/Pick3/VALIDATION_AUDIT: phase=QUEUE_BEFORE service=%p queue=%p front=%p frontEffect=%d frontGroup=%d frontOwner=%d frontChoose=%d pendingEffect=%d pendingGroup=%d selectedPlayer=%d count=%d target0=%d target1=%d target2=%d cardId=%d",
                (void*)self,(void*)queue,(void*)blocking,frontEffect,frontGroup,frontOwner,int(frontChoose),lastEffect,lastGroup,
                playPlayer.IsValid()?playPlayer[play].Get():-1,count,
                count>0?ids->items->m_Items[0]:-1,count>1?ids->items->m_Items[1]:-1,count>2?ids->items->m_Items[2]:-1,
                playCard.IsValid()?playCard[play].Get():-1);
        }
        // The local service's injected finder uses the *same simulation model*
        // (prediction/received play), which may differ from the UI model.
        Obj* model=nullptr;
        Obj* finder=localServiceFinder.IsValid()?localServiceFinder[self].Get():nullptr;
        Obj* entityFinder=finder && TargetBridge::finderEntityFinder.IsValid()
            ? TargetBridge::finderEntityFinder[finder].Get():nullptr;
        if (entityFinder && TargetBridge::entityFinderModel.IsValid())
            model=TargetBridge::entityFinderModel[entityFinder].Get();
        submissionModel=model;
        if (blocking && model && chooseBlockingClass.IsValid() && BNM::IsA(blocking,chooseBlockingClass) &&
            TargetBridge::getEntityById.IsValid() && TargetBridge::getComponent.IsValid() &&
            TargetBridge::cardGuid.IsValid() && TargetBridge::selectionType.IsValid() &&
            TargetBridge::primaryFilterClass.IsValid()) {
            const int effect=liveEffectEntityId[blocking].Get();
            const int group=liveEffectGroupId[blocking].Get();
            Obj* effectEntity=nullptr;
            auto e=BNM::TryInvoke([&] {effectEntity=TargetBridge::getEntityById[model](effect);});
            Obj* filter=nullptr;
            if (!e.IsValid() && effectEntity) {
                e=BNM::TryInvoke([&] {filter=TargetBridge::getComponent[effectEntity](TargetBridge::primaryFilterClass.GetMonoType());});
            }
            const bool ownerMatches=playPlayer.IsValid() && TargetBridge::blockingPlayerId.IsValid() &&
                playPlayer[play].Get()==TargetBridge::blockingPlayerId[blocking].Get();
            int originalMode=filter && !e.IsValid() ? TargetBridge::selectionType[filter].Get() : -1;
            // A local popup normalizes its filter (3/4/5 -> Manual=1).
            // Recover the original mode only for the EXACT filter/effect/group.
            if (ownerMatches && originalMode==TargetBridge::kManualValue) {
                std::lock_guard<std::mutex> lock(TargetBridge::snapshotMutex);
                for (const auto& item:TargetBridge::runtimeOverrides) {
                    if (item.effectId!=effect || item.groupId!=group) continue;
                    // Preserve the exact existing reconstruction behavior
                    // for live-entity modes 3/4. DB5 additionally requires
                    // the original filter object to match, since a GUID
                    // must never be inferred from an unrelated recipient.
                    if (EnumModes::IsLivePickFromThree(item.originalSelectionType) ||
                        (item.originalSelectionType==EnumModes::kPickFromThreeCardDatabase &&
                         Core::SceneViewContext::getGCHandle(item.filterHandle)==filter)) {
                        originalMode=item.originalSelectionType;
                        break;
                    }
                }
                if (originalMode==TargetBridge::kManualValue)
                    for (const auto& item:TargetBridge::databaseQueryOverrides)
                        if (item.effectId==effect && item.groupId==group &&
                            Core::SceneViewContext::getGCHandle(item.filterRoot)==filter) {
                            originalMode=EnumModes::kPickFromThreeCardDatabase;
                            break;
                        }
            }
            if (playType.IsValid() && playType[play].Get()==6) {
                const auto* ids=playTargets[play].Get();
                const int count=ids && ids->size>=0 && ids->size<=32 && (ids->size==0 || ids->items)?ids->size:-1;
                LOG_INFO("CustomComponents/Pick3/VALIDATION_AUDIT: phase=FILTER_CONTEXT effect=%d group=%d expectedEffect=%d expectedGroup=%d frontMatch=%d effectEntity=%p filter=%p rawMode=%d recoveredMode=%d ownerMatches=%d serviceModel=%p inputModel=%p finder=%p error=%d",
                    effect,group,lastEffect,lastGroup,int(effect==lastEffect && group==lastGroup),
                    (void*)effectEntity,(void*)filter,filter && !e.IsValid()?TargetBridge::selectionType[filter].Get():-1,
                    originalMode,int(ownerMatches),(void*)model,(void*)Target(modelRoot),(void*)finder,int(e.IsValid()));
                for(int i=0;i<count && i<3;++i) {
                    const int id=ids->items->m_Items[i];
                    Obj* target=nullptr; int guid=-1;
                    const auto ex=BNM::TryInvoke([&]{target=TargetBridge::getEntityById[model](id);});
                    bool guidException=false;
                    if(!ex.IsValid() && target) {
                        const auto g=BNM::TryInvoke([&]{guid=TargetBridge::cardGuid(target);});
                        guidException=g.IsValid();
                    }
                    LOG_INFO("CustomComponents/Pick3/VALIDATION_AUDIT: phase=TARGET_IN_SERVICE effect=%d group=%d index=%d id=%d entity=%p guid=%d lookupException=%d guidException=%d pendingSnapshot=%d pendingGuidMatch=%d",
                        effect,group,i,id,(void*)target,guid,int(ex.IsValid()),int(guidException),
                        int(TargetBridge::StillEligible(effect,group,id)),
                        int(guid>0 && TargetBridge::StillEligibleWithGuid(effect,group,id,guid)));
                }
            }
            const bool db5=ownerMatches && originalMode==EnumModes::kPickFromThreeCardDatabase;
            const bool customLive=ownerMatches && EnumModes::IsLivePickFromThree(originalMode);
            if (db5) {
                // The genuine Play target is a real recipient. The GUID is
                // conveyed separately via CardId; NEVER treat Targets[0] as
                // a definition GUID or create a fake match entity.
                IntList* ids=playTargets[play].Get();
                const int chosenGuid=playCard.IsValid() ? playCard[play].Get() : -1;
                const int recipient=ids && ids->items && ids->size==1
                    ? ids->items->m_Items[0] : kNoEntity;
                Obj* recipientEntity=nullptr;
                if (recipient>=0) {
                    auto ex=BNM::TryInvoke([&] {recipientEntity=TargetBridge::getEntityById[model](recipient);});
                    if (ex.IsValid()) recipientEntity=nullptr;
                }
                // Eligibility is checked using the original, rooted database
                // query, even when the receiver already redirected the live
                // primary target query to AdditionalTargetQuery.
                Obj* originalQuery=TargetBridge::OriginalDatabaseQuery(filter,effect,group);
                const auto eligible=TargetBridge::EnumerateCardDatabase(originalQuery,effect);
                const bool validGuid=chosenGuid>0 && std::any_of(eligible.begin(),eligible.end(),
                    [chosenGuid](const auto& item){return item.cardGuid==chosenGuid;});
                const bool shapeValid=TargetBridge::numberOfTargets.IsValid() &&
                    TargetBridge::numberOfTargets[filter].Get()==1 &&
                    TargetBridge::additionalTargetType.IsValid() &&
                    TargetBridge::additionalTargetType[filter].Get()==1 &&
                    TargetBridge::onlyApplyToAdditionalTargets.IsValid() &&
                    TargetBridge::onlyApplyToAdditionalTargets[filter].Get();
                if (recipientEntity && shapeValid && validGuid &&
                    TargetBridge::RedirectDatabaseTargetQuery(filter,effect,group)) {
                    // Only normalize during the exact synchronous stock
                    // Process() call, preserving the custom enum afterward.
                    // The service still validates the recipient normally.
                    const int modeBefore=TargetBridge::selectionType[filter].Get();
                    if (modeBefore!=TargetBridge::kManualValue)
                        TargetBridge::selectionType[filter].Set(TargetBridge::kManualValue);
                    std::uint64_t receiverSequence=0;
                    if (Picked::Store(effect,group,{{kNoEntity,chosenGuid}},"RX_DB",&receiverSequence)) {
                        Picked::TraceLedger("RX_DB_STAGED", effect, group, chosenGuid);
                        ++remotePickRebuilds;
                        if (NetworkTrace::Mode())
                            LOG_INFO("CustomComponents/Pick3/DB5_PVP: RX_VALID effect=%d group=%d guid=%d recipient=%d eligible=%zu",effect,group,chosenGuid,recipient,eligible.size());
                    } else LOG_ERR("CustomComponents/Pick3/DB5_PVP: RX_CACHE_FAILED effect=%d group=%d",effect,group);
                    // Call the original while the filter is in stock Manual
                    // mode and the legal recipient query is active.
                    if (originalLocalSelectProcess) {
                        CustomComponents::Systems::CustomTriggerSystem::LogPick3ServiceProcess("SERVICE_PROCESS_ENTER", self);
                        CustomComponents::Systems::CustomTriggerSystem::ArmPick3QueueAudit("DB_SELECTION", effect, group);
                        ++CustomComponents::Systems::CustomTriggerSystem::pickFromThreeServiceDepth;
                        TransactionRuntime::ScopedSubmission selectedSubmission({effect,group,receiverSequence,{recipient},{{kNoEntity,chosenGuid,-1}}});
                        TransactionRuntime::StageSubmission(TransactionRuntime::ModelOf(model,true),TransactionRuntime::submitted);
                        originalLocalSelectProcess(self,play,method);
                        --CustomComponents::Systems::CustomTriggerSystem::pickFromThreeServiceDepth;
                        CustomComponents::Systems::CustomTriggerSystem::LogPick3ServiceProcess("SERVICE_PROCESS_EXIT", self);
                    }
                    if (modeBefore!=TargetBridge::kManualValue)
                        TargetBridge::selectionType[filter].Set(modeBefore);
                    // The owning popup retains its override until normal
                    // FinishTransaction; receiver-only query roots are
                    // released once stock processing has returned.
                    if (modeBefore!=TargetBridge::kManualValue)
                        TargetBridge::CancelDatabaseOverrideForFilter(filter,effect,group,"remote-process-complete");
                    return;
                }
                LOG_ERR("CustomComponents/Pick3/DB5_PVP: RX_INVALID effect=%d group=%d guid=%d recipient=%d exists=%d shape=%d eligible=%d (original service will decide)",
                        effect,group,chosenGuid,recipient,recipientEntity?1:0,shapeValid?1:0,validGuid?1:0);
                // Fail closed for custom choice reconstruction, but do not
                // bypass or modify the game's stock validation.
            } else if (customLive) {
                IntList* ids=playTargets[play].Get();
                std::vector<Picked::SelectedCard> selected;
                if (ids && ids->items && ids->size>=1 && ids->size<=3) {
                    for(int i=0;i<ids->size;i++) {
                        int id=ids->items->m_Items[i];
                        Obj* entity=nullptr;
                        if (id<0) {selected.clear();break;}
                        auto x=BNM::TryInvoke([&] {entity=TargetBridge::getEntityById[model](id);});
                        int guid=-1;
                        if (!x.IsValid() && entity) x=BNM::TryInvoke([&] {guid=TargetBridge::cardGuid(entity);});
                        if (x.IsValid() || guid<=0) {selected.clear();break;}
                        selected.push_back({id,guid});
                    }
                }
                // Stock's repeated FindValidTargetsFromEffect can yield board
                // lanes even though the original initial Play validator found
                // real discarded-card targets. Scope that validated list to
                // this exact stock service call, not to the popup lifetime.
                if (selected.size()==1 && TargetBridge::blockingSourceId.IsValid() &&
                    TargetBridge::blockingPlayerId.IsValid() && ownerMatches &&
                    originalMode==EnumModes::kPickFromThreeUniqueGuidTargets &&
                    TargetBridge::resolveValidatedInitialCardTargets) {
                    const int source=TargetBridge::blockingSourceId[blocking].Get();
                    Obj* sourceEntity=nullptr; int sourceGuid=-1;
                    auto sx=BNM::TryInvoke([&]{sourceEntity=TargetBridge::getEntityById[model](source);});
                    if (!sx.IsValid() && sourceEntity)
                        sx=BNM::TryInvoke([&]{sourceGuid=TargetBridge::cardGuid(sourceEntity);});
                    if (!sx.IsValid() && IsGenericPick3ReviveBurst(source)) {
                        auto validated=TargetBridge::resolveValidatedInitialCardTargets(model, source, effect);
                        if (std::find(validated.begin(),validated.end(),selected[0].entityId)!=validated.end() &&
                            validated.size()<=12) {
                            bridgeContext.effect=effect;
                            bridgeContext.group=group;
                            bridgeContext.owner=TargetBridge::blockingPlayerId[blocking].Get();
                            bridgeContext.sourceCard=source;
                            bridgeContext.chosen=selected[0].entityId;
                            bridgeContext.validated=std::move(validated);
                            useValidationBridge=true;
                        }
                    }
                    PF3_REPAIR_TRACE("CustomComponents/Pick3/REPAIR1711: RX_REVIVE_RECONSTRUCTION effect=%d group=%d source=%d chosen=%d serviceModel=%p recognitionModel=%p retainedModel=%p enabled=%d validated=%zu",
                        effect,group,source,selected[0].entityId,(void*)model,
                        (void*)ReviveRecognitionModel(),(void*)Target(boundEntityModelRoot),
                        int(useValidationBridge),bridgeContext.validated.size());
                    LOG_INFO("CustomComponents/Pick3/STOCK_VALIDATION_BRIDGE: PREPARE effect=%d group=%d source=%d guid=%d chosen=%d enabled=%d count=%zu",
                        effect,group,source,sourceGuid,selected[0].entityId,int(useValidationBridge),bridgeContext.validated.size());
                }
                // Full list has to decode against the RECEIVING service's
                // model before it can change a game-owned selection filter.
                const bool completeSelected=ids && ids->items && ids->size>=1 && ids->size<=3 &&
                    selected.size()==static_cast<std::size_t>(ids->size);
                std::uint64_t receiverSequence=0;
                const bool stored=completeSelected && Picked::Store(effect,group,selected,"RX_LIVE",&receiverSequence);
                if(stored) {
                    receiverSubmission={effect,group,receiverSequence,{},selected};
                    for(const auto& card:selected) receiverSubmission.targets.push_back(card.entityId);
                }
                if (stored) Picked::TraceLedger("RX_STAGED", effect, group, selected.empty() ? -1 : selected.front().entityId);
                if (stored) {
                    ++remotePickRebuilds;
                    LOG_INFO("CustomComponents/Pick3/DEFAULT_REPAIR: PICK_RECONSTRUCT effect=%d group=%d count=%zu total=%u",effect,group,selected.size(),remotePickRebuilds);
                    if (originalMode!=TargetBridge::kManualValue &&
                        EnumModes::IsLivePickFromThree(originalMode)) {
                        validatedLiveFilter=filter;
                        validatedLiveMode=originalMode;
                        validatedLiveEffect=effect;
                        validatedLiveGroup=group;
                        validatedLiveCount=ids->size;
                    }
                }
#if CUSTOMCOMPONENTS_PF3_REPAIR_TRACE
                PF3_REPAIR_TRACE("CustomComponents/Pick3/REPAIR177: RX_LIVE_PREFLIGHT effect=%d group=%d ownerMatches=%d mode=%d selected=%zu received=%d cached=%d normalizePlanned=%d model=%p",
                    effect,group,int(ownerMatches),originalMode,selected.size(),ids?ids->size:-1,int(stored),int(validatedLiveFilter!=nullptr),(void*)model);
#endif
            }
        }
    }
    if (originalLocalSelectProcess) {
        const bool traceSelection = play && playType.IsValid() && playCard.IsValid() &&
            playPlayer.IsValid() && playTargets.IsValid() && playType[play].Get() == 6;
        const auto* traceIds = traceSelection ? playTargets[play].Get() : nullptr;
        const int traceCount = traceIds && traceIds->items ? traceIds->size : -1;
        const int traceFirst = traceCount > 0 ? traceIds->items->m_Items[0] : -1;
        if (traceSelection) Picked::TraceLedger("STOCK_PROCESS_BEFORE", bridgeContext.effect, bridgeContext.group, traceFirst);
        if (traceSelection) LOG_INFO("CustomComponents/Pick3/HANDOFF_TRACE: phase=STOCK_PROCESS_ENTER service=%p play=%p cardId=%d player=%d type=6 targets=%d first=%d completion=%d reason=%d",
            (void*)self, (void*)play, playCard[play].Get(), playPlayer[play].Get(),
            traceCount, traceFirst, int(SelectionCompletion::Status()), SelectionCompletion::completion.Reason());
        if (traceSelection)
            CustomComponents::Systems::CustomTriggerSystem::ArmPick3QueueAudit("SELECT_SERVICE",
                bridgeContext.effect, bridgeContext.group);
        if (traceSelection) LOG_INFO("CustomComponents/Pick3/PICK3_TRIGGER_TIMELINE: phase=SERVICE_ENTER service=%p play=%p card=%d target=%d status=%d",
            self,play,playCard[play].Get(),traceFirst,int(SelectionCompletion::Status()));
        CustomComponents::Systems::CustomTriggerSystem::LogPick3ServiceProcess("SERVICE_PROCESS_ENTER", self);
        // The reconstructed exact PickFromThree selection is already known at
        // this point. Correct the still-live marked runtime effect BEFORE
        // original Process() can retire it. Never use callback status here:
        // callback delivery is later than queue advancement on both peers.
        if (traceSelection && bridgeContext.effect >= 0) {
            CorrectPick3ReviveRolesBeforeStockSelect(self, bridgeContext.effect,
                bridgeContext.group, bridgeContext.sourceCard, bridgeContext.chosen,
                playPlayer.IsValid()?playPlayer[play].Get():-1, useValidationBridge);
        } else if (traceSelection && blocking && liveEffectEntityId.IsValid() &&
                   liveEffectGroupId.IsValid() && TargetBridge::blockingSourceId.IsValid() &&
                   playTargets.IsValid()) {
            // Replica path: no owner-only validation recipient. Require a
            // single exact transaction-bound Pick3 card already reconstructed
            // from the received stock Play, never the global latest choice.
            const int id = liveEffectEntityId[blocking].Get();
            const int group = liveEffectGroupId[blocking].Get();
            const int source = TargetBridge::blockingSourceId[blocking].Get();
            int chosen = -1, guid = -1;
            const auto* ids = playTargets[play].Get();
            if (ids && ids->items && ids->size == 1 &&
                Picked::TryGetSingleLiveEntity(id, group, chosen, guid) &&
                chosen == ids->items->m_Items[0])
                CorrectPick3ReviveRolesBeforeStockSelect(self, id, group,
                    source, chosen,playPlayer.IsValid()?playPlayer[play].Get():-1,true);
        }
        {
            TargetBridge::ScopedValidationRecipientGuard scoped(useValidationBridge ? &bridgeContext : nullptr);
            if (traceSelection) ++CustomComponents::Systems::CustomTriggerSystem::pickFromThreeServiceDepth;
            Pick3SecondaryCommitScope commit{};
            if (traceSelection && blocking && liveEffectEntityId.IsValid() &&
                liveEffectGroupId.IsValid() && TargetBridge::blockingSourceId.IsValid() &&
                playTargets.IsValid()) {
                const int effectId = liveEffectEntityId[blocking].Get();
                const int groupId = liveEffectGroupId[blocking].Get();
                const int source = TargetBridge::blockingSourceId[blocking].Get();
                const auto* ids = playTargets[play].Get();
                int chosen = -1, guid = -1;
                if (ids && ids->items && ids->size == 1 &&
                    Picked::TryGetSingleLiveEntity(effectId, groupId, chosen, guid) &&
                    chosen == ids->items->m_Items[0] &&
                    IsGenericPick3ReviveEffect(effectId)) {
                    Obj* finder = localServiceFinder.IsValid() ? localServiceFinder[self].Get() : nullptr;
                    Obj* entityFinder = finder && TargetBridge::finderEntityFinder.IsValid()
                        ? TargetBridge::finderEntityFinder[finder].Get() : nullptr;
                    Obj* currentModel = entityFinder && TargetBridge::entityFinderModel.IsValid()
                        ? TargetBridge::entityFinderModel[entityFinder].Get() : nullptr;
                    commit = {effectId, groupId, source, chosen, currentModel,
                              playPlayer.IsValid()?playPlayer[play].Get():-1};
                }
            }
            Pick3SecondaryCommitGuard commitGuard(commit);
            // Normalize only the exact, fully reconstructed live-PF3 RX filter
            // for the synchronous original stock Process() body. The outgoing
            // sender may already be in Manual mode; no extra writes there.
            const bool normalizeRx=validatedLiveFilter && validatedLiveMode>=0 &&
                TargetBridge::selectionType.IsValid() &&
                TargetBridge::selectionType[validatedLiveFilter].Get()==validatedLiveMode;
            if (normalizeRx) {
                TargetBridge::selectionType[validatedLiveFilter].Set(TargetBridge::kManualValue);
                PF3_REPAIR_TRACE("CustomComponents/Pick3/REPAIR177: RX_MODE_NORMALIZE effect=%d group=%d raw=%d stock=%d count=%d service=%p filter=%p",
                    validatedLiveEffect,validatedLiveGroup,validatedLiveMode,TargetBridge::kManualValue,validatedLiveCount,(void*)self,(void*)validatedLiveFilter);
            }
            // Restoration on every ordinary exit, including errors converted
            // to managed IL2CPP exceptions by the stock original method.
            struct RxModeRestore final {
                Obj* filter; int rawMode; bool armed;
                ~RxModeRestore() {
                    if (!armed || !filter || !TargetBridge::selectionType.IsValid()) return;
                    const int after=TargetBridge::selectionType[filter].Get();
                    if (after==TargetBridge::kManualValue)
                        TargetBridge::selectionType[filter].Set(rawMode);
                    PF3_REPAIR_TRACE("CustomComponents/Pick3/REPAIR177: RX_MODE_RESTORE filter=%p raw=%d observed=%d restored=%d",
                        (void*)filter,rawMode,after,int(after==TargetBridge::kManualValue));
                }
            } rxRestore{validatedLiveFilter,validatedLiveMode,normalizeRx};
            TransactionRuntime::ScopedSubmission selectedSubmission(
                receiverSubmission.sequence?receiverSubmission:TransactionRuntime::submitted);
            TransactionRuntime::StageSubmission(TransactionRuntime::ModelOf(submissionModel,true),TransactionRuntime::submitted);
            originalLocalSelectProcess(self,play,method);
#if CUSTOMCOMPONENTS_PF3_REPAIR_TRACE
            // Read the *live* queue after stock Process returns, before its
            // surrounding scope is restored. No pointers survive this frame.
            static thread_local unsigned postSamples1724 = 0;
            if (traceSelection && postSamples1724++ < 48) {
                const int wantedEffect = commit.effect > 0 ? commit.effect : bridgeContext.effect;
                const int wantedGroup = commit.effect > 0 ? commit.group : bridgeContext.group;
                Obj* q = localServiceEffects.IsValid() ? localServiceEffects[self].Get() : nullptr;
                const bool validQueue = q && effectQueueConcrete.IsValid() &&
                    BNM::IsA(q, effectQueueConcrete);
                static auto postPrimary = BNM::Class("PvZCards.Engine", "Effect")
                    .GetField("TargetId").cast<int>();
                static auto postSecondary = BNM::Class("PvZCards.Engine", "Effect")
                    .GetField("SecondaryTargetId").cast<int>();
                static auto postPhase = BNM::Class("PvZCards.Engine.Effects", "ChooseTargetBlockingEffect")
                    .GetField("Phase").cast<int>();
                int matches = 0, examined = 0;
                if (validQueue && wantedEffect > 0 && wantedGroup >= 0 &&
                    liveEffectEntityId.IsValid() && liveEffectGroupId.IsValid()) {
                    struct Collection1724 {
                        const char* name;
                        BNM::Field<BNM::Structures::Mono::List<Obj*>*>* field;
                    };
                    // These fields represent separate stock continuation lists.
                    // Inspect only validated concrete lists; never call Peek().
                    const Collection1724 collections[] = {
                        {"Effects", &boundQueueEffects},
                        {"ResponseEffects", &boundQueueResponses},
                        {"GroupResponseEffects", &boundQueueGroupResponses}
                    };
                    for (const auto& collection : collections) {
                        if (!collection.field->IsValid()) continue;
                        auto* list = (*collection.field)[q].Get();
                        if (!list || list->size < 0 || list->size > 256 ||
                            (list->size > 0 && !list->items)) continue;
                        for (int i = 0; i < list->size && examined < 256; ++i) {
                            Obj* e = list->items->m_Items[i];
                            if (!e) continue;
                            ++examined;
                            if (liveEffectEntityId[e].Get() != wantedEffect ||
                                liveEffectGroupId[e].Get() != wantedGroup) continue;
                            ++matches;
                            const bool choose = chooseBlockingClass.IsValid() &&
                                BNM::IsA(e, chooseBlockingClass);
                            LOG_INFO("CustomComponents/Pick3/PREPLAY1724: stage=POST_PROCESS_EFFECT collection=%s effect=%d group=%d primary=%d secondary=%d phase=%d chosen=%d",
                                collection.name, wantedEffect, wantedGroup,
                                postPrimary.IsValid() ? postPrimary[e].Get() : -1,
                                postSecondary.IsValid() ? postSecondary[e].Get() : -1,
                                choose && postPhase.IsValid() ? postPhase[e].Get() : -1,
                                commit.chosen);
                        }
                    }
                }
                LOG_INFO("CustomComponents/Pick3/PREPLAY1724: stage=POST_PROCESS_SUMMARY wantedEffect=%d wantedGroup=%d scopeActive=%d queueAvailable=%d examined=%d matched=%d",
                    wantedEffect, wantedGroup, int(commit.effect > 0), int(validQueue),
                    examined, matches);
            }
#endif
            if (traceSelection) --CustomComponents::Systems::CustomTriggerSystem::pickFromThreeServiceDepth;
        }
        if (useValidationBridge) {
            // A returned failure is not a committed choice. Never alter the
            // queue in that case; this check is symmetric on both clients.
            const auto outcome = SelectionCompletion::Status();
            const int reason = SelectionCompletion::completion.Reason();
            LOG_INFO("CustomComponents/Pick3/PICK3_REVIVE_COMMIT: effect=%d group=%d chosen=%d status=%d reason=%d success=%d pending=%d failure=%d",
                bridgeContext.effect, bridgeContext.group, bridgeContext.chosen,
                int(outcome), reason, int(outcome == SelectionCompletion::Result::Success && reason == 0),
                int(outcome == SelectionCompletion::Result::Pending),
                int(outcome == SelectionCompletion::Result::Failure));
            if (outcome == SelectionCompletion::Result::Success && reason == 0)
                BindCommittedPick3RevivePrimary(self, bridgeContext);
            else
                LOG_INFO("CustomComponents/Pick3/PICK3_REVIVE_PRIMARY_BIND: result=SKIP_NOT_SUCCESS effect=%d group=%d chosen=%d status=%d reason=%d",
                    bridgeContext.effect, bridgeContext.group, bridgeContext.chosen, int(outcome), reason);
        }
        if (useValidationBridge) LOG_INFO("CustomComponents/Pick3/STOCK_VALIDATION_BRIDGE: PROCESS_EXIT effect=%d group=%d chosen=%d status=%d reason=%d",bridgeContext.effect,bridgeContext.group,bridgeContext.chosen,int(SelectionCompletion::Status()),SelectionCompletion::completion.Reason());
        CustomComponents::Systems::CustomTriggerSystem::LogPick3ServiceProcess("SERVICE_PROCESS_EXIT", self);
        if (traceSelection) LOG_INFO("CustomComponents/Pick3/PICK3_TRIGGER_TIMELINE: phase=SERVICE_EXIT service=%p play=%p target=%d status=%d reason=%d",
            self,play,traceFirst,int(SelectionCompletion::Status()),SelectionCompletion::completion.Reason());
        if (traceSelection) Picked::TraceLedger("STOCK_PROCESS_AFTER", bridgeContext.effect, bridgeContext.group, traceFirst);
        if (traceSelection) LOG_INFO("CustomComponents/Pick3/HANDOFF_TRACE: phase=STOCK_PROCESS_EXIT service=%p play=%p completion=%d reason=%d",
            (void*)self, (void*)play, int(SelectionCompletion::Status()), SelectionCompletion::completion.Reason());
        if (traceSelection) {
            Obj* queue=localServiceEffects.IsValid()?localServiceEffects[self].Get():nullptr;
            Obj* front=nullptr;int queueCount=-1;
            if(queue && effectQueueConcrete.IsValid() && BNM::IsA(queue,effectQueueConcrete) && boundQueueEffects.IsValid()) {
                auto* entries=boundQueueEffects[queue].Get();
                if(entries && entries->size>=0 && entries->size<=4096 && (!entries->size || entries->items)) {
                    queueCount=entries->size;
                    if(queueCount) front=entries->items->m_Items[0];
                }
            }
            LOG_INFO("CustomComponents/Pick3/VALIDATION_AUDIT: phase=QUEUE_AFTER service=%p play=%p queue=%p queueCount=%d front=%p frontEffect=%d frontGroup=%d completion=%d reason=%d activeTicket=%llu",
                (void*)self,(void*)play,(void*)queue,queueCount,(void*)front,
                front && chooseBlockingClass.IsValid() && BNM::IsA(front,chooseBlockingClass) && liveEffectEntityId.IsValid()?liveEffectEntityId[front].Get():-1,
                front && chooseBlockingClass.IsValid() && BNM::IsA(front,chooseBlockingClass) && liveEffectGroupId.IsValid()?liveEffectGroupId[front].Get():-1,
                int(SelectionCompletion::Status()),SelectionCompletion::completion.Reason(),
                (unsigned long long)SelectionCompletion::activeTicket);
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
            LOG_INFO("CustomComponents/Pick3/PREPLAY1722: stage=MULTICHOICE_UNRESOLVED "
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
            LOG_INFO("CustomComponents/Pick3/DEFERRED_DROP: phase=CORRECT card=%d guid=%d drag=%u old=%d target=%d zone=%d stockSole=%d",src,TrickGenerationGuid(src),trickActiveDragSeq,oldTarget,target,deferredDropZone,provenSole);
            PF3_REPAIR_TRACE("CustomComponents/Pick3/REPAIR1712: CAST_REWRITE source=%d oldTarget=%d initialTarget=%d effect=%d stockCommand=1",src,oldTarget,target,deferredValidatedPlay.effect);
        } else {
            LOG_INFO("CustomComponents/Pick3/DEFERRED_DROP: phase=ALREADY_ZONE card=%d drag=%u zone=%d",src,trickActiveDragSeq,target);
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
        LOG_INFO("CustomComponents/Pick3/TRICK_ACCEPT: phase=COMMAND_ENTER seq=%u guidHint=%d command=%p source=%d target=%d (guid-correlated-from-drag)", trickAcceptanceSeq, trickPendingGuid, self, src, target);
    }
    if (postSubmitInputBudget && src >= 0) {
        LOG_INFO("CustomComponents/Pick3/PATCH154_PLAY_HANDOFF: source=%d target=%d sourceEqualsTarget=%d capture=%d queued=%d normalPlaySubturn=%d lastEffect=%d lastGroup=%d pendingRefresh=%d stockUnmodified=1",
                 src, target, src==target?1:0, overlayCapturesInput?1:0,
                 TargetBridge::HasQueuedChoice()?1:0, observedLocalTargetSubTurn?0:1,
                 postSubmitEffect, postSubmitGroup, pendingRefresh?1:0);
    }
    LOG_INFO("CustomComponents/Pick3/TRICK_PLACE_DIAG: PLAYCARD_ENTER activeDrag=%u command=%p gesture=%p source=%d target=%d order=%d handled=%d stock=1",
             trickActiveDragSeq, self, gesture, src, target, order, handled);
    PrePlayLifecycleTrace::OnPlay(src,target,handled,false);
    PvpCrashMark("PLAY_COMMAND","STOCK_ENTER",src,self);
    if (originalPlayCardExecute) originalPlayCardExecute(self, method);
    PvpCrashMark("PLAY_COMMAND","STOCK_EXIT",src,self);
    PrePlayLifecycleTrace::OnPlay(src,target,handled,true);
    LOG_INFO("CustomComponents/Pick3/TRICK_PLACE_DIAG: PLAYCARD_EXIT command=%p stockReturned=1", self);
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
        LOG_INFO("CustomComponents/Pick3/PF3_CHOICE: effect=%d group=%d index=%d entity=%d guid=%d",
            request.effectEntityId,request.effectGroupId,index,option.entityId,option.cardGuid);
    }
    stockHandoffRequest=request; stockHandoffIndices=indices;
    stockHandoffState=StockHandoffState::Armed;
    LOG_INFO("CustomComponents/Pick3/STOCK_HANDOFF: STAGE effect=%d group=%d target=%d selected=%zu",
       request.effectEntityId,request.effectGroupId,StockTapRecipient(request,indices.back()),indices.size());
    return true;
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
            LOG_INFO("CustomComponents/Pick3/STOCK_HANDOFF: COMMAND_MATCH cmd=%p service=%p effect=%d group=%d retained=1",
                     (void*)self,(void*)owner,h.effectEntityId,h.effectGroupId);
            // Input is no longer owned by the modal UI once the retained
            // stock command has accepted this exact effect/group.
            overlayCapturesInput = false;
            LOG_INFO("CustomComponents/Pick3/INPUT_HANDOFF: STOCK_ACCEPTED_CAPTURE_RELEASED effect=%d group=%d",h.effectEntityId,h.effectGroupId);
            // Begin the visual exit before Submit can synchronously invoke
            // stock success and retire the targeting holder. The visual clock
            // is independent of ACK or command release.
            if (onPick3PlayDispatch)
                onPick3PlayDispatch(h.effectEntityId,h.effectGroupId);
            BeginDeadline();
            const bool submitted=Submit(stockHandoffRequest,stockHandoffIndices,false);
            stockHandoffState=submitted ? StockHandoffState::Dispatched : StockHandoffState::Failed;
            LOG_INFO("CustomComponents/Pick3/STOCK_HANDOFF: COMMAND_RESULT cmd=%p dispatch=%d reachedService=%d status=%d",
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
        LOG_INFO("CustomComponents/Pick3/STOCK_HANDOFF: EXECUTE_STAGE_CAPTURE cmd=%p service=%p effect=%d group=%d modal=%d",
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
    LOG_INFO("CustomComponents/Pick3/TRIGGER_CONTEXT: effect=%d group=%d rooted=%d",
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
// PvP crash containment: do not invoke TurnTimerComponent.GetTimeRemaining.
// The 2026-11-01 submission SIGSEGV reached this managed method through
// BNM::TryInvoke from the retained-timer deadline path. A catch wrapper cannot
// recover from a native SIGSEGV. Until the timer's IL2CPP object lifetime is
// proven, custom auto-pick/deadline decisions must be UNKNOWN rather than
// declaring a remote selection expired or crashing UnityMain.
inline long long TimerRemaining() {
    static unsigned suppressedReads = 0;
    if (++suppressedReads <= 8)
        LOG_INFO("CustomComponents/Pick3/PICK3_TIMER_SAFETY: phase=SKIP_MANAGED_REMAINING reason=prior-native-null-deref fallback=unknown");
    return -1;
}
inline bool DeadlineExpired() {
    if (!Remote()) return false;
    static unsigned suppressedDeadlines = 0;
    if (++suppressedDeadlines <= 8)
        LOG_INFO("CustomComponents/Pick3/PICK3_TIMER_SAFETY: phase=DEADLINE_UNKNOWN remote=1 no-unsafe-invoke=1");
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
inline bool Submit(const PreviewRequest& request, const std::vector<int>& indices, bool timeout) {
    LOG_INFO("CustomComponents/UnearthTrace: MANUAL_SUBMIT effect=%d group=%d selected=%zu options=%zu required=%d remote=%d timeout=%d",
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
    LOG_INFO("CustomComponents/Pick3/SUBMIT_CHECKPOINT: after-targets effect=%d group=%d selected=%zu local=%d", request.effectEntityId, request.effectGroupId, selectedCards.size(), Local() ? 1 : 0);
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
    LOG_INFO("CustomComponents/Pick3/SUBMIT_CHECKPOINT: before-live-effect effect=%d group=%d svc=%p model=%p", request.effectEntityId, request.effectGroupId, static_cast<void*>(svc), static_cast<void*>(model));
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
        LOG_INFO("CustomComponents/Pick3/IDENTITY: STOCK_HOLDER_SUBMISSION effect=%d group=%d queueListMismatch=1 stockServiceMustValidate=1",request.effectEntityId,request.effectGroupId);
    }
    LOG_INFO("CustomComponents/Pick3/SUBMIT_CHECKPOINT: live-effect-passed effect=%d group=%d", request.effectEntityId, request.effectGroupId);
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
    // For DB5, Targets remains the real recipient entity, while CardId
    // carries the selected card DEFINITION GUID through the stock serializer.
    // A normal SelectTarget always retains CardId=-1.
    if (databaseMode) action.cardId = selectedCards[0].cardGuid;
    LOG_INFO("CustomComponents/Pick3/VALIDATION_AUDIT: phase=PREPARE_REQUEST effect=%d group=%d mode=%d owner=%d localPlayer=%d inputModel=%p service=%p targetCount=%zu first=%d second=%d third=%d cardId=%d offerCount=%zu rootCount=%zu",
        request.effectEntityId,request.effectGroupId,int(request.mode),request.ownerPlayerId,playerId,
        (void*)model,(void*)svc,entities.size(),entities.size()>0?entities[0]:-1,
        entities.size()>1?entities[1]:-1,entities.size()>2?entities[2]:-1,action.cardId,
        request.options.size(),submittedPlayRoots.size());
    for(std::size_t i=0;i<request.options.size() && i<3;++i)
        LOG_INFO("CustomComponents/Pick3/VALIDATION_AUDIT: phase=OFFER effect=%d group=%d index=%zu entity=%d guid=%d stillEligible=%d guidMatch=%d",
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
        LOG_INFO("CustomComponents/Pick3/DB5_PVP: TX_ENCODE effect=%d group=%d guid=%d recipient=%d player=%d remote=%d",
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
        LOG_INFO("CustomComponents/Pick3/PICK3_LIVE_CAPTURE: effect=%d group=%d entity=%d guid=%d verifiedAtSubmit=1",
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
        LOG_INFO("CustomComponents/Pick3/PICK3_LIVE_EARLY: phase=ARM effect=%d group=%d armed=%d", request.effectEntityId,request.effectGroupId,int(liveEarlyArmed));
    }
    if (databaseEarlyArmed) {
        databaseEarlyRequest = request;
        CreationTrace::onVerifiedDatabaseDraw = &OnVerifiedDatabaseDraw;
    }
    // The live service collection is the exact object passed by stock to
    // EffectCollectionProcessor.ProcessEffects during this selection.
    if (request.mode == SelectionMode::CardDatabase && Local())
        CustomComponents::Systems::CustomTriggerSystem::WatchPick3ResumeEffects(
            svc ? *reinterpret_cast<Obj**>(reinterpret_cast<std::uint8_t*>(svc) + 0x30) : nullptr,
            request.effectEntityId, request.effectGroupId);
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
    LOG_INFO("CustomComponents/Pick3/HANDOFF_TRACE: phase=BEFORE_BEGIN effect=%d group=%d service=%p play=%p player=%d targets=%zu currentCompletion=%d",
        request.effectEntityId, request.effectGroupId, (void*)svc, (void*)play, playerId,
        entities.size(), int(SelectionCompletion::Status()));
    if (!SelectionCompletion::Begin(svc)) {
        CustomComponents::Systems::CustomTriggerSystem::liveSelectionQueueScope = nullptr;
        CustomComponents::Systems::CustomTriggerSystem::liveSelectionQueueCallback = nullptr;
        liveEarlyArmed = false;
        liveEarlyQueued = false;
        databaseEarlyArmed = false;
        submittedPlayRoots.pop_back(); Drop(playRoot);
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
    LOG_INFO("CustomComponents/Pick3/HANDOFF_TRACE: phase=BEGIN_ATTACHED effect=%d group=%d service=%p ticket=%llu priorSuccessHandler=%d completion=%d",
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
    NetworkTrace::MarkStage("PICK3_DISPATCH_RETURN", play);
    LOG_INFO("CustomComponents/Pick3/HANDOFF_TRACE: phase=DISPATCH_RETURN effect=%d group=%d dispatch=%d completion=%d reason=%d",
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
        LOG_INFO("CustomComponents/Pick3/PICK3_SCOPE: effect=%d group=%d uiModel=%p serviceModel=%p producerModel=%p serviceMatch=%d local=1",
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
        LOG_INFO("CustomComponents/Pick3/PICK3_LIVE_CONTEXT: effect=%d group=%d source=%d target=%d verified=%d single=%d",
                 request.effectEntityId, request.effectGroupId, dispatchIdentity.sourceId,
                 boundLiveEntity, int(exactLiveBinding), int(request.requiredTargets == 1));
        LOG_INFO("CustomComponents/Pick3/PICK3_TARGET_CONTEXT: mode=%s effect=%d group=%d snapshotValid=%d originalTarget=%d boundEntity=%d bindingVerified=%d applied=%d finalTarget=%d",
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
        LOG_INFO("CustomComponents/Pick3/PICK3_TRIGGER_TIMELINE: phase=LOCAL_DISPATCH effect=%d group=%d ready=%d fired=%d",
            request.effectEntityId,request.effectGroupId,int(liveTargetReady),int(fired));
        emitted = fired;
        LOG_INFO("CustomComponents/Pick3/PICK3_TRIGGER: effect=%d group=%d local=1 queued=%d model=%p source=%p",
                 request.effectEntityId,request.effectGroupId,fired?1:0,
                 (void*)dispatchModel,(void*)sourceEffect);
    } else if (success) {
        LOG_INFO("CustomComponents/Pick3/PICK3_TRIGGER_TIMELINE: phase=REMOTE_AWAIT_PROVENANCE effect=%d group=%d remote=%d context=%d",
            request.effectEntityId,request.effectGroupId,int(Remote()),int(triggerIdentityMatches));
        LOG_INFO("CustomComponents/Pick3/PICK3_TRIGGER_PROVENANCE: phase=NO_CONFIRMED_REPLICA effect=%d group=%d remote=%d context=%d dispatch=deferred",request.effectEntityId,request.effectGroupId,Remote()?1:0,triggerIdentityMatches?1:0);
        LOG_INFO("CustomComponents/Pick3/PICK3_TRIGGER: deferred effect=%d group=%d remote=%d context=%d reason=not-confirmed-stock-replication",
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
    LOG_INFO("CustomComponents/Pick3/PICK3_LIVE_EARLY: phase=STOCK_QUEUE effect=%d group=%d fired=%d fallback=%d",
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
    LOG_INFO("CustomComponents/Pick3/PICK3_EARLY: phase=DRAW_CREATE effect=%d group=%d entity=%d queued=%d fallback=%d",
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
        !unearthAuditReturnOrder.IsValid() ||
        !unearthAuditEffectPrimary.IsValid() ||
        !unearthAuditEffectSecondary.IsValid()) return;
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
                unearthAuditReturnOrder[e].Get() != kUnearthReturnMarker)
                continue;
            ++matches;
            match = e;
            collectionName = list.name;
        }
    }
    if (matches != 1 || !match || initialZone < 0) {
        LOG_INFO("CustomComponents/Pick3/PICK3_REVIVE_EARLY_HANDOFF: result=SKIP effect=%d group=%d source=%d chosen=%d initialZone=%d matches=%d",
            effectId, groupId, sourceCard, selectedCard, initialZone, matches);
        return;
    }
    const int primary = unearthAuditEffectPrimary[match].Get();
    const int secondary = unearthAuditEffectSecondary[match].Get();
    if (primary == initialZone && secondary == selectedCard) {
        unearthAuditEffectPrimary[match].Set(selectedCard);
        unearthAuditEffectSecondary[match].Set(-1);
        LOG_INFO("CustomComponents/Pick3/PICK3_REVIVE_EARLY_HANDOFF: result=CORRECTED effect=%d group=%d collection=%s originalPrimary=%d originalSecondary=%d primary=%d secondary=-1",
            effectId, groupId, collectionName, primary, secondary, selectedCard);
    } else {
        LOG_INFO("CustomComponents/Pick3/PICK3_REVIVE_EARLY_HANDOFF: result=UNCHANGED effect=%d group=%d collection=%s primary=%d secondary=%d chosen=%d zone=%d",
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
        LOG_INFO("CustomComponents/Pick3/PICK3_REVIVE_CONTINUATION: result=QUEUE_LAYOUT_UNAVAILABLE effect=%d group=%d", recipient.effect, recipient.group);
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
                LOG_INFO("CustomComponents/Pick3/PICK3_REVIVE_CONTINUATION: result=ENTRY collection=%s index=%d effect=%d group=%d expectedEffect=%d expectedGroup=%d choose=%d stage=%d primary=%d secondary=%d",
                    collection.name, i, effectId, groupId, recipient.effect, recipient.group,
                    int(choose), stage, primary[e].Get(), secondary[e].Get());
        }
    }
    LOG_INFO("CustomComponents/Pick3/PICK3_REVIVE_CONTINUATION: result=ASSOCIATION effect=%d group=%d chosen=%d effects=%d responses=%d groupResponses=%d examined=%d exact=%d sameEffectOtherGroup=%d sameGroupOtherEffect=%d otherSecondary=%d collection=%s",
        recipient.effect, recipient.group, recipient.chosen, effects ? effects->size : 0,
        responses ? responses->size : 0, groupResponses ? groupResponses->size : 0,
        examined, matching, sameEffectOtherGroup,
        sameGroupOtherEffect, otherSecondary, matchCollection ? matchCollection : "none");
    if (matching != 1 || !match) {
        LOG_INFO("CustomComponents/Pick3/PICK3_REVIVE_PRIMARY_BIND: result=%s effect=%d group=%d chosen=%d matches=%d",
            matching == 0 ? "NO_MATCH" : "SKIP_AMBIGUOUS", recipient.effect,
            recipient.group, recipient.chosen, matching);
        return;
    }
    const int oldPrimary = primary[match].Get();
    const int oldSecondary = secondary[match].Get();
    // Never overwrite an already assigned card, nor infer a lane from the
    // initial casting zone. The stock secondary choice still asks for a lane.
    if (oldPrimary != -1 || oldSecondary != -1) {
        LOG_INFO("CustomComponents/Pick3/PICK3_REVIVE_PRIMARY_BIND: result=SKIP_BOUND effect=%d group=%d chosen=%d primary=%d secondary=%d",
            recipient.effect, recipient.group, recipient.chosen, oldPrimary, oldSecondary);
        return;
    }
    primary[match].Set(recipient.chosen);
    LOG_INFO("CustomComponents/Pick3/PICK3_REVIVE_PRIMARY_BIND: result=BOUND effect=%d group=%d chosen=%d primary=%d secondary=%d stage=1 collection=%s",
        recipient.effect, recipient.group, recipient.chosen, primary[match].Get(), secondary[match].Get(), matchCollection);
}

inline void TraceUnearthQueueAtBoundary(const char* phase, const PreviewRequest& request) {
    if (request.mode != SelectionMode::UniqueGuidTargets ||
        !boundQueueEffects.IsValid() || !effectQueueConcrete.IsValid() ||
        !liveEffectEntityId.IsValid() || !liveEffectGroupId.IsValid()) return;
    Obj* queue = Target(boundEffectsRoot);
    if (!queue || !BNM::IsA(queue, effectQueueConcrete)) {
        LOG_INFO("CustomComponents/Pick3/PICK3_REVIVE_QUEUE_STAGE: boundary=%s effect=%d group=%d inspect=unavailable",
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
        LOG_INFO("CustomComponents/Pick3/PICK3_REVIVE_QUEUE_STAGE: boundary=%s effect=%d group=%d index=%d count=%d choose=%d stage=%d primary=%d secondary=%d",
                 phase, id, group, i, list->size, int(choose), stage, primary, secondary);
    }
    LOG_INFO("CustomComponents/Pick3/PICK3_REVIVE_QUEUE_STAGE: boundary=%s effect=%d group=%d queueCount=%d matching=%d frontEffect=%d frontGroup=%d",
             phase, request.effectEntityId, request.effectGroupId, list->size,
             matching,
             list->size && list->items->m_Items[0] ? liveEffectEntityId[list->items->m_Items[0]].Get() : -1,
             list->size && list->items->m_Items[0] ? liveEffectGroupId[list->items->m_Items[0]].Get() : -1);
}

inline void FinishTransaction(const PreviewRequest& request, bool success) {
    if (stockHandoffState==StockHandoffState::Dispatched &&
        stockHandoffRequest.effectEntityId==request.effectEntityId &&
        stockHandoffRequest.effectGroupId==request.effectGroupId) {
        LOG_INFO("CustomComponents/Pick3/STOCK_HANDOFF: COMPLETION effect=%d group=%d stockTargetSuccesses=%u capturedCommand=%p",
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
    TraceUnearthQueueAtBoundary("BEFORE_DETACH", request);
    SelectionCompletion::Detach();
    TraceUnearthQueueAtBoundary("AFTER_DETACH", request);
    // A successful local service dispatch does not establish that the
    // blocking effect has retired. Keep the same effect/group consumed on
    // both PvE and PvP; only a genuinely new identity can open another offer.
    // On a failed/uncertain dispatch do not silently reroll this effect either.
    const bool rearm = false;
    TargetBridge::CompleteLocalSelection(request.effectEntityId, request.effectGroupId, rearm);
    // The accepted casting snapshot was needed only to prepare this specific
    // effect's choice. Never expose it to another transaction after cleanup.
    if (pick3AcceptedCastPrimaries.effect == request.effectEntityId) {
        LOG_INFO("CustomComponents/Pick3/PICK3_REVIVE_OFFER_BRIDGE: phase=CLEAR effect=%d card=%d",
            request.effectEntityId, pick3AcceptedCastPrimaries.card);
        pick3AcceptedCastPrimaries = {};
    }
    TraceUnearthQueueAtBoundary("AFTER_COMPLETE", request);
    if (Remote() && success) {
        // Dispatch success can be prediction-only. Do not claim ACK_CONSUMED.
        pendingRemoteRetirement = true;
        remoteRetirementEffect = request.effectEntityId;
        remoteRetirementGroup = request.effectGroupId;
        remoteRetirementPolls = 0;
        LOG_INFO("CustomComponents/Pick3/PVP_RETIRE: PREDICTION_SUCCEEDED effect=%d group=%d waitingForQueueRetirement=1 notServerACK=1",
                 request.effectEntityId, request.effectGroupId);
    }
    if (!success || (request.mode == SelectionMode::CardDatabase
            ? !databaseEarlyQueued : !liveEarlyQueued))
        EmitSelectedTrigger(request, success);
    else LOG_INFO("CustomComponents/Pick3/PICK3_EARLY: phase=FINISH_SKIP effect=%d group=%d alreadyQueued=1", request.effectEntityId, request.effectGroupId);
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
    LOG_INFO("CustomComponents/Pick3/HANDOFF: AFTER_CLEANUP effect=%d group=%d remote=%d "
             "queued=%d capture=%d rearm=%d notify=%d",
             request.effectEntityId, request.effectGroupId, Remote() ? 1 : 0,
             TargetBridge::HasQueuedChoice() ? 1 : 0, overlayCapturesInput ? 1 : 0,
             rearm ? 1 : 0, notify ? 1 : 0);
    TraceUnearthQueueAtBoundary("AFTER_MODAL_CLEANUP", request);
    if (notify) {
        pendingRefreshEffect=request.effectEntityId;
        pendingRefreshGroup=request.effectGroupId;
        pendingRefreshAge=pendingStableFrames=0;
        pendingStockSuccessBaseline=stockTargetSuccesses;
        pendingRefresh=true;
        // No synchronous service lookup here. Stock owns UI refresh; the
        // retired custom repair ticket must not invoke unready injection.
        LOG_INFO("CustomComponents/Pick3/CRASH_FIX_155: post-submit injector re-resolution skipped effect=%d group=%d", pendingRefreshEffect,pendingRefreshGroup);
        CC_DIAG("CustomComponents/Pick3/DEFAULT_REPAIR: REFRESH_PENDING effect=%d group=%d signal=%d queue=%d remote=%d",
                pendingRefreshEffect,pendingRefreshGroup,Target(boundSignalRoot)?1:0,
                Target(boundEffectsRoot)?1:0,Remote()?1:0);
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
                       bool useInjectionResolver = true, bool syncPveCompletion = true,
                       bool prePlayLifecycleTrace = false) {
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
    effectCollectionInterface=BNM::Class("PvZCards.Engine","EffectCollection");
    effectQueueConcrete=BNM::Class("PvZCards.Engine","EffectQueue");
    boundEntityModelClass=BNM::Class("PvZCards.Engine","EntityModel");
    chooseBlockingClass=BNM::Class("PvZCards.Engine.Effects","ChooseTargetBlockingEffect");
    boundQueueEffects=effectQueueConcrete.GetField("Effects").cast<BNM::Structures::Mono::List<Obj*>*>();
    boundQueueResponses=effectQueueConcrete.GetField("ResponseEffects").cast<BNM::Structures::Mono::List<Obj*>*>();
    boundQueueGroupResponses=effectQueueConcrete.GetField("GroupResponseEffects").cast<BNM::Structures::Mono::List<Obj*>*>();
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
    holderInputState=gameInputHolderClass.GetField("GameInputState").cast<int>();
    highlightPlayableCardsClass=BNM::Class("PvZCards.Game","HighlightPlayableCardsCommand");
    executeHighlightPlayableCards=highlightPlayableCardsClass.GetMethod("Execute",0).cast<void>();
    // Observe stock's own command execution; never manually call it.
    const auto stockHighlightMethod = highlightPlayableCardsClass.GetMethod("Execute", 0);
    if (stockHighlightMethod.IsValid())
        BNM::BasicHook(stockHighlightMethod, StockHighlightExecuteObserved, originalStockHighlightExecute);
    LOG_INFO("CustomComponents/Pick3/HAND_HIGHLIGHTS: phase=STOCK_OBSERVER_SETUP installed=%d manualExecution=0",
             originalStockHighlightExecute ? 1 : 0);

    highlightCommandInputState=highlightPlayableCardsClass.GetField("<GameInputState>k__BackingField").cast<int>();
    highlightCommandPlayCache=highlightPlayableCardsClass.GetField("<PlayCache>k__BackingField").cast<Obj*>();
    highlightCommandModel=highlightPlayableCardsClass.GetField("<GameModel>k__BackingField").cast<Obj*>();
    highlightCommandFacade=highlightPlayableCardsClass.GetField("<GameFacadeDecider>k__BackingField").cast<Obj*>();
    highlightCommandPlayable=highlightPlayableCardsClass.GetField("<EntityBecamePlayableSignal>k__BackingField").cast<Obj*>();
    highlightCommandUnplayable=highlightPlayableCardsClass.GetField("<EntityBecameUnplayableSignal>k__BackingField").cast<Obj*>();
    LOG_INFO("CustomComponents/Pick3/HAND_HIGHLIGHTS: setup holderState=%d command=%d execute=%d cache=%d signals=%d (active by default)",
        holderInputState.IsValid()?1:0,highlightPlayableCardsClass.IsValid()?1:0,
        executeHighlightPlayableCards.IsValid()?1:0,highlightCommandPlayCache.IsValid()?1:0,
        highlightCommandPlayable.IsValid()&&highlightCommandUnplayable.IsValid()?1:0);
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
    localServiceEffects = localClass.GetField("<Effects>k__BackingField");
    localServiceFinder=localClass.GetField("<Finder>k__BackingField").cast<Obj*>();
    peekLocalEffect = BNM::Class("PvZCards.Engine", "EffectQueue").GetMethod("Peek", 0).cast<Obj*>();
    auto gameEffect = BNM::Class("PvZCards.Engine", "Effect");
    liveEffectEntityId = gameEffect.GetField("EffectEntityId").cast<int>();
    liveEffectGroupId = gameEffect.GetField("EffectGroupId").cast<int>();
    liveBlockingEffectClass = BNM::Class("PvZCards.Engine", "BlockingEffect");
    liveEffectIsResolved = liveBlockingEffectClass.GetMethod("IsResolved", 0).cast<bool>();
    LOG_INFO("CustomComponents/Pick3/LIVE_EFFECT: setup effects=%d peek=%d entity=%d group=%d resolved=%d",
        localServiceEffects.IsValid()?1:0, peekLocalEffect.IsValid()?1:0,
        liveEffectEntityId.IsValid()?1:0, liveEffectGroupId.IsValid()?1:0,
        liveEffectIsResolved.IsValid()?1:0);

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
    auto stockTargetAction = commandClass.GetMethod("SelectTarget", 0);
    auto boardTapExecute = BNM::Class("PvZCards.Game","HandleBoardTappedCommand").GetMethod("Execute",0);
    auto targetSuccess = commandClass.GetMethod("HandleServiceSuccess", 0);
    auto targetFailure = commandClass.GetMethod("HandleServiceFailure", 1);
    auto commandRelease = BNM::Class("PvZCards.Application", "HeroesCommand").GetMethod("Release", 0);
    auto push = stackClass.GetMethod("Push", 1);
    auto ctor = playClass.GetMethod(".ctor", 0);
    auto peek = stackClass.GetMethod("Peek", 0);
    const bool ok = localClass.IsValid() && remoteClass.IsValid() &&
        localPlayerField.IsValid() && commandModel.IsValid() && playType.IsValid() &&
        playPlayer.IsValid() && playCard.IsValid() && playOrder.IsValid() &&
        playTargets.IsValid() && commandService.IsValid() &&
        playCardExecute.IsValid() && playCardCommandModel.IsValid() &&
        execute.IsValid() && stockTargetAction.IsValid() && push.IsValid() && ctor.IsValid() &&
        inputUpdate.IsValid() && stateGameModel.IsValid() && stateCommandBinder.IsValid() &&
        peek.IsValid() && timerClass.IsValid() && timerParameters.IsValid() && timerRunning.IsValid() && timerPaused.IsValid();
    if (!ok || !SelectionCompletion::Initialize()) { LOG_ERR("CustomComponents/Pick3/SUBMIT: SETUP missing metadata"); return false; }
    allowPve = pve; allowPvp = pvp;
    pvpOwnerGateReady = localPlayerField.IsValid() && opponentPlayerField.IsValid() &&
                        TargetBridge::blockingPlayerId.IsValid();
    TargetBridge::resolveValidatedInitialCardTargets = &ValidatedInitialCardTargets;
    Systems::CustomTriggerSystem::RegisterInFlightSelectionProof(&ProveInFlightSelectedTarget);
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
    // Incoming stock targeting plays seed the same query cache as local picks.
    // Hook is independent of ADB tracing and forwards original exactly once.
    auto localProcess=localClass.GetMethod("Process",1);
    if (localProcess.IsValid()) BNM::BasicHook(localProcess,LocalSelectProcess,originalLocalSelectProcess);
    LOG_INFO("CustomComponents/Pick3/DEFAULT_REPAIR: incoming=%d serviceFinder=%d signalClass=%d queueMeta=%d modelClass=%d (active by default)",
        originalLocalSelectProcess?1:0,localServiceFinder.IsValid()?1:0,
        gameStateUpdatedSignalClass.IsValid()?1:0,
        boundQueueEffects.IsValid()&&boundQueueResponses.IsValid()&&boundQueueGroupResponses.IsValid()?1:0,
        TargetBridge::entityFinderModel.IsValid()?1:0);
    LOG_INFO("CustomComponents/Pick3/DB5_PVP: SETUP defaultOn=1 inbound=%d cardId=%d recipientRedirect=%d dbQuery=%d",
        originalLocalSelectProcess?1:0,playCard.IsValid()?1:0,
        TargetBridge::targetFilterQuery.IsValid()&&TargetBridge::additionalTargetQuery.IsValid()?1:0,
        TargetBridge::findDatabaseEntities.IsValid()?1:0);
    // Optional read-only diagnostics; no gameplay changes and no shared
    // ProcessEffects hook. Exact classes/method counts from dump.cs.
    const auto stopDragClass = BNM::Class("PvZCards.Game", "StopCardDragCommand");
    trickDragModelField = stopDragClass.GetField("<DragModel>k__BackingField").cast<Obj*>();
    const auto dragClass = BNM::Class("PvZCards.Game", "DragModel");
    trickDraggedCardField = dragClass.GetField("CardEntityId").cast<int>();
    trickInHandField = dragClass.GetField("InHand").cast<bool>();
    trickTargetIdsField = dragClass.GetField("Targetids").cast<IntList*>();
    trickEntitiesField = boundEntityModelClass.GetField("Entities").cast<BNM::Structures::Mono::List<Obj*>*>();
    const auto entityClassForRevive = BNM::Class("PvZCards.Engine", "Entity");
    trickEntityIdField = entityClassForRevive.GetField("<Id>k__BackingField").cast<int>();
    trickEntityComponentsField = entityClassForRevive.GetField("Components")
        .cast<BNM::Structures::Mono::List<Obj*>*>();
    trickCardComponentClass = BNM::Class("PvZCards.Engine.Components", "Card");
    const auto startMethod = BNM::Class("PvZCards.Game", "StartCardDragCommand").GetMethod("Execute", 0);
    if (startMethod.IsValid()) BNM::BasicHook(startMethod, TrickStartDragTrace, originalTrickStartDrag);
    const auto updateMethod = BNM::Class("PvZCards.Game", "UpdateCardDragPositionCommand").GetMethod("Execute", 0);
    if (updateMethod.IsValid()) BNM::BasicHook(updateMethod, TrickUpdateDragTrace, originalTrickUpdateDrag);
    const auto topDraggedMethod = BNM::Class("PvZCards.Game", "ViewEntityOverlapFinder").GetMethod("GetTopDraggedEntityId", 2);
    // Read-only cache and UI boundaries. Avoid manual command execution.
    const auto stockPlayCacheClass = BNM::Class("PvZCards.Game", "PlayCache");
    const auto cachePlaysMethod = stockPlayCacheClass.GetMethod("CachePlays", 1);
    const auto refreshPlayableMethod = stockPlayCacheClass.GetMethod("RefreshPlayableCardsList", 0);
    const auto updateHighlightMethod = highlightPlayableCardsClass.GetMethod("UpdateCardsHighlightState", 1);
    if (cachePlaysMethod.IsValid()) BNM::BasicHook(cachePlaysMethod, StockCachePlaysObserved, originalStockCachePlays);
    if (refreshPlayableMethod.IsValid()) BNM::BasicHook(refreshPlayableMethod, StockRefreshPlayableObserved, originalStockRefreshPlayable);
    if (updateHighlightMethod.IsValid()) BNM::BasicHook(updateHighlightMethod, StockHighlightUpdateObserved, originalStockHighlightUpdate);
    stockPlayableCardsField = stockPlayCacheClass.GetField("PlayableCards").cast<IntList*>();
    // Native GetPlayableCards() is two ARM64 instructions (8 bytes): do NOT
    // inline-hook it, because HasPlayableCards() begins at +8. Keep the
    // separate display-permission observer (40-byte method) intact.
    const auto shouldHighlightMethod = highlightPlayableCardsClass.GetMethod("ShouldHighlightPlayableCards", 0);
    if (shouldHighlightMethod.IsValid()) BNM::BasicHook(shouldHighlightMethod, StockShouldHighlightObserved, originalStockShouldHighlight);
    LOG_INFO("CustomComponents/Pick3/PLAYABLE_AUDIT: phase=SETUP refreshField=%d displayPermission=%d tinyGetterHook=0",stockPlayableCardsField.IsValid()?1:0,originalStockShouldHighlight?1:0);
    LOG_INFO("CustomComponents/Pick3/STOCK_PLAYCACHE: phase=SETUP cache=%d refresh=%d highlightUpdate=%d readOnly=1",
             originalStockCachePlays?1:0,originalStockRefreshPlayable?1:0,originalStockHighlightUpdate?1:0);
    // This call is virtual in stock and invokes GetPlaysForCard via slot 7.
    // The private static helper is only entered when the cache lookup is non-null.
    const auto cacheGetMethod = BNM::Class("PvZCards.Game", "PlayCache").GetMethod("GetPlaysForCard", 1);
    const auto firstValidMethod = BNM::Class("PvZCards.Game", "ViewEntityOverlapFinder").GetMethod("GetFirstValidTarget", 2);
    if (cacheGetMethod.IsValid()) BNM::BasicHook(cacheGetMethod, TrickCachedPlaysTrace, originalTrickCachedPlays);
    if (firstValidMethod.IsValid()) BNM::BasicHook(firstValidMethod, TrickFirstValidTrace, originalTrickFirstValid);
    if (topDraggedMethod.IsValid()) BNM::BasicHook(topDraggedMethod, TrickTopDraggedTrace, originalTrickTopDragged);
    LOG_INFO("CustomComponents/Pick3/TRICK_CACHE: SETUP getPlays=%d firstValid=%d topDragged=%d readOnly=1 overlaps=opaque", originalTrickCachedPlays?1:0, originalTrickFirstValid?1:0, originalTrickTopDragged?1:0);

    trickGestureField = playCardCommandClass.GetField("<EntityGestureData>k__BackingField").cast<Obj*>();
    const auto stopDragMethod = stopDragClass.GetMethod("Execute", 0);
    if (stopDragMethod.IsValid())
        BNM::BasicHook(stopDragMethod, TrickStopDragTrace, originalTrickStopDrag);
    const auto targetMethod = BNM::Class("PvZCards.Core.Utilities", "GameStateUtils")
        .GetMethod("GetValidPlayTargetsForCard", 3);
    if (targetMethod.IsValid())
        BNM::BasicHook(targetMethod, TrickValidTargetsTrace, originalTrickValidTargets);
    // GameFacadeDecider is an interface (GetGameFacade only), not the
    // concrete gameplay generator. GameStateUtils implements both methods.
    // Targets are observed by TrickValidTargetsTrace above to avoid a double
    // hook on exactly the same GetValidPlayTargetsForCard entry point.
    const auto decider = BNM::Class("PvZCards.Core.Utilities", "GameStateUtils");
    gameStateUtilsEntityModelField = decider.GetField("<EntityModel>k__BackingField").cast<Obj*>();
    PF3_REPAIR_TRACE("CustomComponents/Pick3/REPAIR1713: MODEL_FIELD_BIND decider=%d components=%d cardClass=%d",
        gameStateUtilsEntityModelField.IsValid()?1:0,trickEntityComponentsField.IsValid()?1:0,
        trickCardComponentClass.IsValid()?1:0);
    // Field identity confirmed against the game's OneTimeEffectValidator
    // declaration: <EntityModel>k__BackingField at object offset 0x28.
    // Binding it here does not change any validator result.
    pick3OneTimeValidatorModelField =
        BNM::Class("PvZCards.Core.Utilities", "OneTimeEffectValidator")
            .GetField("<EntityModel>k__BackingField").cast<Obj*>();
    LOG_INFO("CustomComponents/Pick3/PICK3_INITIAL_MODEL: phase=BIND validatorModelField=%d offsetExpected=0x28 mutation=0",
             pick3OneTimeValidatorModelField.IsValid() ? 1 : 0);
    const auto deciderPlays = decider.GetMethod("GetValidPlaysForCard", 3);
    if (deciderPlays.IsValid()) BNM::BasicHook(deciderPlays, TrickGeneratedPlays, originalDeciderValidPlays);
    // Both gates are observed without modifying their arguments or results.
    const auto orderMethod = decider.GetMethod("GetPossiblePlayOrdersInTargetForCard", 3);
    if (orderMethod.IsValid()) BNM::BasicHook(orderMethod, TrickOrderTrace, originalTrickOrder);
    const auto compositeMethod = BNM::Class("PvZCards.Core.Utilities", "CompositeValidator").GetMethod("Validate", 2);
    const auto oneTimeMethod = BNM::Class("PvZCards.Core.Utilities", "OneTimeEffectValidator").GetMethod("Validate", 2);
    const auto nullMethod = BNM::Class("PvZCards.Core.Utilities", "NullValidator").GetMethod("Validate", 2);
    const auto inHandMethod = BNM::Class("PvZCards.Core.Utilities", "InHandValidator").GetMethod("Validate", 2);
    if (compositeMethod.IsValid()) BNM::BasicHook(compositeMethod, TrickCompositeValidate, originalTrickCompositeValidate);
    if (oneTimeMethod.IsValid()) BNM::BasicHook(oneTimeMethod, TrickOneTimeValidate, originalTrickOneTimeValidate);
    if (nullMethod.IsValid()) BNM::BasicHook(nullMethod, TrickNullValidate, originalTrickNullValidate);
    if (inHandMethod.IsValid()) BNM::BasicHook(inHandMethod, TrickInHandValidate, originalTrickInHandValidate);
    const auto fighterValidateMethod = BNM::Class("PvZCards.Core.Utilities", "FighterValidator").GetMethod("Validate", 2);
    const auto openLaneValidateMethod = BNM::Class("PvZCards.Core.Utilities", "OpenLaneValidator").GetMethod("Validate", 2);
    if (fighterValidateMethod.IsValid()) BNM::BasicHook(fighterValidateMethod, TrickFighterValidate, originalTrickFighterValidate);
    if (openLaneValidateMethod.IsValid()) BNM::BasicHook(openLaneValidateMethod, TrickOpenLaneValidate, originalTrickOpenLaneValidate);
    LOG_INFO("CustomComponents/Pick3/UNEARCH_GATE: phase=SETUP fighter=%d openLane=%d readOnly=1",
             originalTrickFighterValidate?1:0,originalTrickOpenLaneValidate?1:0);
    const auto anyTargetsMethod = BNM::Class("PvZCards.Core.Utilities", "OneTimeEffectValidator").GetMethod("HasAnyTargets", 2);
    if (anyTargetsMethod.IsValid()) BNM::BasicHook(anyTargetsMethod, TrickHasAnyTargetsObserved, originalTrickHasAnyTargets);
    LOG_INFO("CustomComponents/Pick3/UNEARCH_ELIGIBILITY: phase=SETUP hasAnyTargets=%d observerOnly=1", originalTrickHasAnyTargets ? 1 : 0);
    // This stock method is 56 bytes long, unlike the unsafe 8-byte cache
    // getter. Preserve the precise Unearth metadata observer.
    const auto firstEffectMethod = BNM::Class("PvZCards.Core.Utilities", "EffectEntityUtils").GetMethod("GetFirstOnPlayTriggeredAndTargetedEffectEntity", 1);
    if (firstEffectMethod.IsValid()) BNM::BasicHook(firstEffectMethod, FirstTargetedEffectObserved, originalFirstTargetedEffect);
    LOG_INFO("CustomComponents/Pick3/UNEARTH_EFFECT_METADATA: phase=SETUP hook=%d",originalFirstTargetedEffect?1:0);
    const auto targetingMethod = BNM::Class("PvZCards.Core.Utilities", "OneTimeEffectValidator").GetMethod("IsTargetingValid", 3);
    if (targetingMethod.IsValid()) BNM::BasicHook(targetingMethod, TrickIsTargetingValidTrace, originalTrickIsTargetingValid);
    LOG_INFO("CustomComponents/Pick3/TRICK_TARGET_VALID: SETUP helper=%d readOnly=1 focus=699,712,713,27 errors=counts-only collectionAccess=managed", originalTrickIsTargetingValid?1:0);
    LOG_INFO("CustomComponents/Pick3/TRICK_VALIDATION: SETUP orders=%d composite=%d oneTime=%d null=%d inHand=%d readOnly=1 coverage=partial focus=699,712,713,27",
             originalTrickOrder?1:0, originalTrickCompositeValidate?1:0, originalTrickOneTimeValidate?1:0, originalTrickNullValidate?1:0, originalTrickInHandValidate?1:0);
    LOG_INFO("CustomComponents/Pick3/TRICK_GENERATION: SETUP targets=%d plays=%d targetMethod=%d playsMethod=%d concrete=GameStateUtils readOnly=1 focus=699,712,713,27",
             originalTrickValidTargets?1:0, originalDeciderValidPlays?1:0,
             targetMethod.IsValid()?1:0,deciderPlays.IsValid()?1:0);
    // Stock trick-only placement/secondary transition observers. These hooks
    // preserve every original input and return value and are never a play override.
    const auto burstTraceMethod = BNM::Class("PvZCards.Core.Utilities", "GameStateUtils").GetMethod("GetPossiblePlayTargetsForBurst", 2);
    if (burstTraceMethod.IsValid()) BNM::BasicHook(burstTraceMethod, TrickBurstTargetsTrace, originalBurstTargetsTrace);
    RegisterPick3SecondaryFilterEvaluator(&EvaluatePick3SecondaryQueryWithStockFinder);
    LOG_INFO("CustomComponents/Pick3/PICK3_COMPLEX_LANE_QUERY: stockEntityFinderRegistered=1 nestedQueryMatches=1 extraFilterModifiersFailClosed=1");
    const auto finderTraceMethod = BNM::Class("PvZCards.Core.Systems.Utilities", "TargetEntityFinder").GetMethod("FindValidSecondaryTargets", 2);
    // The target finder has two 2-argument overloads. Selecting by name and
    // arity alone is ambiguous, so we intentionally do NOT hook it here.
    (void)finderTraceMethod;
    // ProcessEffect is shared by both simulated PvP models. Observe the
    // committed phase without substituting stock continuation callbacks.
    const auto modelSelectMethod = BNM::Class("PvZCards.Core.Systems", "SelectTargetSystem")
        .GetMethod("ProcessEffect", 1);
    if (modelSelectMethod.IsValid())
        NetworkTrace::validateAcceptedCards=&ValidateAcceptedSelectionCards;
        BNM::BasicHook(modelSelectMethod, Pick3ModelSelectHook, originalPick3ModelSelect);
    LOG_INFO("CustomComponents/Pick3/PICK3_REVIVE_MODEL_COMMIT: phase=SETUP installed=%d readOnly=1",originalPick3ModelSelect?1:0);
    const auto primaryCreatorClass=BNM::Class("PvZCards.Core.Systems.Utilities", "PrimaryTargetEffectCreator");
    const auto preselectedAuditMethod=primaryCreatorClass.GetMethod("CreateEffectsForPreselectedTargets",3);
    const auto primaryListAuditMethod=primaryCreatorClass.GetMethod("CreateEffectsWithPrimaryTargets",3);
    const auto primaryNormalAuditMethod=primaryCreatorClass.GetMethod("CreateEffectsForPrimaryTargets",2);
    const auto primaryMethodAuditMethod=primaryCreatorClass.GetMethod("CreateEffectsBasedOnTargetMethod",4);
    if(preselectedAuditMethod.IsValid()) BNM::BasicHook(preselectedAuditMethod, UnearthPrimaryPreselectedAudit, originalPrimaryPreselectedAudit);
    if(primaryListAuditMethod.IsValid()) BNM::BasicHook(primaryListAuditMethod, UnearthPrimaryListAudit, originalPrimaryTargetListAudit);
    if(primaryNormalAuditMethod.IsValid()) BNM::BasicHook(primaryNormalAuditMethod, UnearthPrimaryNormalAudit, originalPrimaryNormalAudit);
    if(primaryMethodAuditMethod.IsValid()) BNM::BasicHook(primaryMethodAuditMethod, UnearthPrimaryMethodAudit, originalPrimaryMethodAudit);
    LOG_INFO("CustomComponents/Pick3/UNEARCH_PRIMARY_BRANCH: SETUP normal=%d method=%d auditOnly=1 substitution=0", originalPrimaryNormalAudit?1:0, originalPrimaryMethodAudit?1:0);
    const auto contextAuditMethod=BNM::Class("PvZCards.Core.Systems.Utilities", "SecondaryTargetContextSetter").GetMethod("SetSecondaryTargetingContext",2);
    if(contextAuditMethod.IsValid()) BNM::BasicHook(contextAuditMethod, UnearthSecondaryContextAudit, originalSecondaryContextAudit);
    const auto baseAuditEffect=BNM::Class("PvZCards.Engine", "Effect");
    unearthAuditEffectPrimary=baseAuditEffect.GetField("TargetId").cast<int>();
    unearthAuditEffectSecondary=baseAuditEffect.GetField("SecondaryTargetId").cast<int>();
    unearthAuditReturnOrder=BNM::Class("PvZCards.Engine.Effects", "MoveCardToLanesEffect").GetField("OrderInLane").cast<int>();
    LOG_INFO("CustomComponents/Pick3/UNEARCH_TARGET_FLOW: SETUP preselected=%d primaryList=%d contextSetter=%d primaryField=%d secondaryField=%d markerField=%d roleFix=1",originalPrimaryPreselectedAudit?1:0,originalPrimaryTargetListAudit?1:0,originalSecondaryContextAudit?1:0,unearthAuditEffectPrimary.IsValid()?1:0,unearthAuditEffectSecondary.IsValid()?1:0,unearthAuditReturnOrder.IsValid()?1:0);
    const auto secondaryCreatorAuditClass = BNM::Class("PvZCards.Core.Systems.Utilities", "SecondaryTargetEffectCreator");
    const auto addWithTargets = secondaryCreatorAuditClass.GetMethod("AddEffectsWithSecondaryTargets", 2);
    if (addWithTargets.IsValid()) BNM::BasicHook(addWithTargets,
        Pick3AddEffectsWithSecondary, originalPick3AddEffectsWithSecondary);
    LOG_INFO("CustomComponents/Pick3/PICK3_REVIVE_SECONDARY_CREATOR: phase=SETUP installed=%d correctedOnlyWithVerifiedLaneList=1",
        originalPick3AddEffectsWithSecondary ? 1 : 0);

    const auto chooseCreatorAuditClass = BNM::Class("PvZCards.Core.Systems.Utilities", "ChooseTargetEffectCreator");
    const auto chooseDecision3 = chooseCreatorAuditClass.GetMethod("ShouldAddChooseTargetEffects", 3);
    const auto chooseDecision2 = chooseCreatorAuditClass.GetMethod("ShouldAddChooseTargetEffects", 2);
    const auto secondaryAdd = secondaryCreatorAuditClass.GetMethod("AddChooseSecondaryTargetEffects", 3);
    const auto targetChoice = chooseCreatorAuditClass.GetMethod("CreateChooseTargetEffects", 3);
    if(chooseDecision3.IsValid()) BNM::BasicHook(chooseDecision3, UnearthChooseDecision3, originalSecondaryChooseDecision3);
    if(chooseDecision2.IsValid()) BNM::BasicHook(chooseDecision2, UnearthChooseDecision2, originalSecondaryChooseDecision2);
    if(secondaryAdd.IsValid()) BNM::BasicHook(secondaryAdd, UnearthAddSecondaryChoice, originalAddSecondaryChoice);
    if(targetChoice.IsValid()) BNM::BasicHook(targetChoice, UnearthCreateTargetChoice, originalCreateTargetChoice);
    LOG_INFO("CustomComponents/Pick3/UNEARCH_CHOOSE_AUDIT: SETUP decision3=%d decision2=%d addSecondary=%d createChoice=%d readOnly=1 unboundedReviveChoice=1", originalSecondaryChooseDecision3?1:0,originalSecondaryChooseDecision2?1:0,originalAddSecondaryChoice?1:0,originalCreateTargetChoice?1:0);
    const auto creatorTraceMethod = secondaryCreatorAuditClass.GetMethod("CreateEffectsForSecondaryTargets", 3);
    if (creatorTraceMethod.IsValid()) BNM::BasicHook(creatorTraceMethod, TrickCreateSecondaryTrace, originalCreateSecondaryTrace);
    // Exact name from dump.cs TypeDefIndex 12955 (RVA 0x220344C).
    const auto oneTimeClass=BNM::Class("PvZCards.Core.Systems","PlayOneTimeEffectSystem");
    unearthSetupModelField=oneTimeClass.GetField("<EntityModel>k__BackingField").cast<Obj*>();
    unearthSetupLocationField=BNM::Class("PvZCards.Engine.Effects","PlayCardEffect").GetField("LocationId").cast<int>();
    const auto setupMethod=oneTimeClass.GetMethod("SetupTargetsComponents",1);
    unearthSetupSourceField=BNM::Class("PvZCards.Engine","Effect").GetField("SourceId").cast<int>();
    unearthEffectOwnerSourceField=BNM::Class("PvZCards.Engine.Components","EffectEntity").GetField("SourceId").cast<int>();
    unearthSetupPrimaryTargetsClass=BNM::Class("PvZCards.Engine.Components","PrimaryTargets");
    unearthGenericPrimarySelectionType=BNM::Class("PvZCards.Engine.Components","TargetEntityFilter").GetField("SelectionType").cast<int>();
    unearthSetupRemoveComponent=BNM::Class("PvZCards.Engine","Entity").GetMethod("RemoveComponent",1).cast<Obj*>();
    LOG_INFO("CustomComponents/Pick3/UNEARCH_STRIP_PRIMARY: SETUP enabled=%d sourceField=%d effectOwnerField=%d removeMethod=%d primaryType=%d pick3ModeField=%d signature=burst+pick3-primary+revive+first-play-trigger defaultOff=1",kUnearthStripCastingPrimaryTrialEnabled?1:0,unearthSetupSourceField.IsValid()?1:0,unearthEffectOwnerSourceField.IsValid()?1:0,unearthSetupRemoveComponent.IsValid()?1:0,unearthSetupPrimaryTargetsClass.IsValid()?1:0,unearthGenericPrimarySelectionType.IsValid()?1:0);
    if(setupMethod.IsValid())BNM::BasicHook(setupMethod,UnearthSetupTargetsAudit,originalUnearthSetupTargets);
    LOG_INFO("CustomComponents/Pick3/UNEARCH_SETUP_TARGETS: SETUP class=PlayOneTimeEffectSystem method=%d hook=%d modelField=%d locationField=%d readOnly=1",setupMethod.IsValid()?1:0,originalUnearthSetupTargets?1:0,unearthSetupModelField.IsValid()?1:0,unearthSetupLocationField.IsValid()?1:0);
    const auto converterMethod=BNM::Class("PvZCards.Core.Systems.Utilities","EffectEntityConverter").GetMethod("CreateEffectsForEffectEntity",3);
    if(converterMethod.IsValid()) BNM::BasicHook(converterMethod,UnearthConverterAudit,originalUnearthConverter);
    LOG_INFO("CustomComponents/Pick3/UNEARCH_OWNER: SETUP converter=%d identity=component-list readOnly=1",originalUnearthConverter?1:0);
    const auto effectUtils = BNM::Class("PvZCards.Core.Utilities", "EffectEntityUtils");
    const auto isTargetedMethod = effectUtils.GetMethod("IsOnPlayTargetedEffect", 1);
    const auto validEffectTargetsMethod = effectUtils.GetMethod("GetValidTargetsForEffectEntity", 1);
    if (isTargetedMethod.IsValid()) BNM::BasicHook(isTargetedMethod, TrickEffectIsTargeted, originalEffectIsTargeted);
    if (validEffectTargetsMethod.IsValid()) BNM::BasicHook(validEffectTargetsMethod, TrickEffectValidTargets, originalEffectValidTargets);
    LOG_INFO("CustomComponents/Pick3/DEFERRED_TRIAL: SETUP enabled=%d scope=712,713 exactGeneratedZone=1 candidateScope=1 originalEffectFilterPreserved=1", kDeferredPick3TrialEnabled?1:0);
    LOG_INFO("CustomComponents/Pick3/TRICK_EFFECT_PATH: SETUP burst=%d isTargeted=%d effectTargets=%d readOnly=1 initialZoneExperiment=%d scope=712,713 burstOnly=1", originalBurstTargetsTrace?1:0, originalEffectIsTargeted?1:0, originalEffectValidTargets?1:0, kDeferredPick3InitialZoneExperiment?1:0);
    LOG_INFO("CustomComponents/Pick3/TRICK_SECONDARY: SETUP burst=%d create=%d finder=skipped-ambiguous-overload readOnly=1", originalBurstTargetsTrace?1:0, originalCreateSecondaryTrace?1:0);
    LOG_INFO("CustomComponents/Pick3/TRICK_IDENTITY: SETUP entitiesField=%d entityIdField=%d cardGuidField=%d readOnly=1", trickEntitiesField.IsValid()?1:0, trickEntityIdField.IsValid()?1:0, TargetBridge::databaseCardGuid.IsValid()?1:0);
    LOG_INFO("CustomComponents/Pick3/TRICK_TARGET_IDS: SETUP field=%d start=%d update=%d topDragged=%d readOnly=1", trickTargetIdsField.IsValid()?1:0, originalTrickStartDrag?1:0, originalTrickUpdateDrag?1:0, originalTrickTopDragged?1:0);
    LOG_INFO("CustomComponents/Pick3/TRICK_PLACE_DIAG: SETUP stopDrag=%d validTargets=%d gestureField=%d readOnly=1",
             originalTrickStopDrag ? 1 : 0, originalTrickValidTargets ? 1 : 0,
             trickGestureField.IsValid() ? 1 : 0);
    const auto trickSuccess = playCardCommandClass.GetMethod("HandleServiceSuccess", 0);
    const auto trickFailure = playCardCommandClass.GetMethod("HandleServiceFailure", 1);
    const auto playClass=BNM::Class("PvZCards.Engine.Dtos", "Play");
    trickPlayCardIdField=playClass.GetField("CardId").cast<int>();
    trickPlayPlayerIdField=playClass.GetField("PlayerId").cast<int>();
    trickPlayTypeField=playClass.GetField("Type").cast<int>();
    trickPlayOrderField=playClass.GetField("OrderInTargetLocation").cast<int>();
    trickPlayTargetsField=playClass.GetField("Targets").cast<IntList*>();
    const auto helperClass=BNM::Class("PvZCards.Core.Utilities", "PlayCardHelper");
    trickHelperFailModelField=helperClass.GetField("<FailReasonModel>k__BackingField").cast<Obj*>();
    trickFailureReasonsField=BNM::Class("PvZCards.Core.Utilities", "FailReasonModel").GetField("FailureReasons").cast<Obj*>();
    const auto determineMethod=helperClass.GetMethod("DetermineIfPlayIsValid",1);
    if(determineMethod.IsValid()) BNM::BasicHook(determineMethod,TrickDetermineTrace,originalTrickDetermine);
    LOG_INFO("CustomComponents/Pick3/TRICK_SERVICE: SETUP determine=%d card=%d player=%d type=%d order=%d targets=%d failModel=%d failureList=%d readOnly=1",originalTrickDetermine?1:0,trickPlayCardIdField.IsValid()?1:0,trickPlayPlayerIdField.IsValid()?1:0,trickPlayTypeField.IsValid()?1:0,trickPlayOrderField.IsValid()?1:0,trickPlayTargetsField.IsValid()?1:0,trickHelperFailModelField.IsValid()?1:0,trickFailureReasonsField.IsValid()?1:0);
    const auto localPlayMethod = BNM::Class("PvZCards.Core.Services", "LocalPlayCardService").GetMethod("Process", 1);
    if (trickSuccess.IsValid()) BNM::BasicHook(trickSuccess, TrickPlaySuccess, originalTrickCommandSuccess);
    if (trickFailure.IsValid()) BNM::BasicHook(trickFailure, TrickPlayFailure, originalTrickCommandFailure);
    if (localPlayMethod.IsValid()) BNM::BasicHook(localPlayMethod, TrickLocalPlayProcess, originalTrickLocalProcess);
    LOG_INFO("CustomComponents/Pick3/TRICK_ACCEPT: SETUP commandSuccess=%d commandFailure=%d localProcess=%d readOnly=1 focus=699,712,713,27", originalTrickCommandSuccess?1:0, originalTrickCommandFailure?1:0, originalTrickLocalProcess?1:0);
    BNM::BasicHook(playCardExecute, PlayCardExecute, originalPlayCardExecute);
    // Install the action hook before allowing the original Execute to run.
    BNM::BasicHook(stockTargetAction, StockSelectTargetAction, originalSelectTargetAction);
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
    // Use TargetButtonTrace's existing state detours: no duplicate BNM hooks.
    PrePlayLifecycleTrace::Configure(prePlayLifecycleTrace);
    TargetButtonTrace::Setup();
    PhaseRecordTrace::Setup();
    // Passive diagnostics only, disabled unless ADB switch is nonzero.
    NetworkTrace::Initialize();
    QueryCaller::Initialize();
    SelectionCompletion::networkObserver = &NetworkTrace::OnServiceResult;
    CC_DIAG("CustomComponents/Pick3/SUBMIT: SETUP ready=%d pve=%d pvp=%d thresholdMs=%d hooks=playcard:%d execute:%d timer:%d gameInput:%d touchManager:0 setterHooks:0",
         enabled ? 1 : 0, pve ? 1 : 0, pvp ? 1 : 0, deadlineThresholdMs,
         originalPlayCardExecute ? 1 : 0, originalExecute ? 1 : 0,
         originalPush ? 1 : 0, observedGameInput ? 1 : 0);
    return enabled;
}
}
