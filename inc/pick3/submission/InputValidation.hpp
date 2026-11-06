#include "diagnostics/DiagnosticLogging.hpp"
#pragma once
// Private implementation module. Include PickFromThreeSubmission.hpp, not this file.
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
            CC_SELECTION_DIAG("CustomComponents/Pick3/PICK3_REVIVE_OFFER_BRIDGE: phase=READ card=%d effect=%d count=%zu modelMatch=1 ageMs=%lld",
                card, pick3AcceptedCastPrimaries.effect, pick3AcceptedCastPrimaries.ids.size(),
                static_cast<long long>(std::chrono::duration_cast<std::chrono::milliseconds>(age).count()));
            return pick3AcceptedCastPrimaries.ids;
        }
        CC_SELECTION_DIAG("CustomComponents/Pick3/PICK3_REVIVE_OFFER_BRIDGE: phase=EXPIRE card=%d", card);
        pick3AcceptedCastPrimaries = {};
    }
    if (!kDeferredPick3TrialEnabled || !pick3CurrentValidationModel ||
        model != pick3CurrentValidationModel ||
        deferredPrimaryCards.card!=card ||
        pick3PrimaryCaptureModel != pick3CurrentValidationModel ||
        pick3PrimaryCaptureCard != card) {
        static thread_local unsigned misses = 0;
        if (kDeferredPick3TrialEnabled && ++misses <= 32)
            CC_SELECTION_DIAG("CustomComponents/Pick3/PICK3_PAIR_CONTEXT: phase=PRIMARY_CACHE_MISS card=%d cachedCard=%d producer=missing copiedIds=0", card, deferredPrimaryCards.card);
        return {};
    }
    static thread_local unsigned hits = 0;
    if (++hits <= 32)
        CC_SELECTION_DIAG("CustomComponents/Pick3/PICK3_PAIR_CONTEXT: phase=PRIMARY_CACHE_READ card=%d ids=%zu producer=stock-effect-targets decision=observation-only", card, deferredPrimaryCards.ids.size());
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
    CC_SELECTION_DIAG("CustomComponents/Pick3/PVP_CRASH_TRACE: serial=%u depth=%u tag=%s phase=%s card=%d ptr=%p",
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
    CC_SELECTION_DIAG("CustomComponents/Pick3/TRICK_SERVICE: phase=%s seq=%u play=%p guid=%d card=%d player=%d type=%d order=%d count=%d shown=%d targets=[%d,%d,%d,%d,%d,%d,%d,%d]",phase,seq,play,guid,card,player,type,order,count,count<0?0:(count<8?count:8),ids[0],ids[1],ids[2],ids[3],ids[4],ids[5],ids[6],ids[7]);
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
        CC_SELECTION_DIAG("CustomComponents/Pick3/TRICK_SERVICE: phase=DETERMINE_CONTEXT seq=%u helper=%p sameServicePlay=%d",seq,self,play==trickServiceActivePlay?1:0);
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
        CC_SELECTION_DIAG("CustomComponents/Pick3/TRICK_SERVICE: phase=DETERMINE_EXIT seq=%u play=%p guid=%d result=%d failModel=%p errorCount=%d",seq,play,guid,result?1:0,failModel,count);
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
        CC_SELECTION_DIAG("CustomComponents/Pick3/TRICK_ACCEPT: phase=COMMAND_SUCCESS seq=%u guid=%d command=%p source=%d target=%d", trickAcceptanceSeq, trickPendingGuid, self, trickPendingSource, trickPendingTarget);
    if (originalTrickCommandSuccess) originalTrickCommandSuccess(self, method);
}
inline void TrickPlayFailure(Obj* self, Obj* failure, const void* method) {
    if (self && self == trickPendingCommand && TrickFocus(trickPendingGuid))
        CC_SELECTION_DIAG("CustomComponents/Pick3/TRICK_ACCEPT: phase=COMMAND_FAILURE seq=%u guid=%d command=%p failure=%p source=%d target=%d", trickAcceptanceSeq, trickPendingGuid, self, failure, trickPendingSource, trickPendingTarget);
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
        CC_SELECTION_DIAG("CustomComponents/Pick3/TRICK_SERVICE: phase=PROCESS_EXIT seq=%u play=%p guid=%d",seq,play,guid);
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
        CC_SELECTION_DIAG("CustomComponents/Pick3/TRICK_CACHE: phase=LOOKUP seq=%u card=%d cache=%p list=%p count=%d", trickCacheProbeSeq,cardId,self,result,trickCacheResultCount);
    }
    return result;
}
inline int TrickFirstValidTrace(Obj* overlapped, Obj* plays, const void* method) {
    const int result = originalTrickFirstValid ? originalTrickFirstValid(overlapped,plays,method) : -1;
    if (trickCacheResolveDepth) {
        trickCacheFirstResult = result;
        CC_SELECTION_DIAG("CustomComponents/Pick3/TRICK_CACHE: phase=FIRST_VALID seq=%u card=%d plays=%p count=%d overlaps=%p resolved=%d", trickCacheProbeSeq,trickCacheProbeCard,plays,TrickSafeManagedPlayCount(plays),overlapped,result);
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
        CC_SELECTION_DIAG("CustomComponents/Pick3/TRICK_EFFECT_PATH: phase=%s guid=%d cardEntity=%d list=%p opaque=1", phase, guid, source, raw);
        return;
    }
    char ids[180] = {};
    size_t n = 0;
    for (int i=0; i<std::min(list->size, 12); ++i) {
        const int z = snprintf(ids+n, sizeof(ids)-n, "%s%d", i?",":"", list->items->m_Items[i]);
        if (z <= 0 || size_t(z) >= sizeof(ids)-n) break;
        n += size_t(z);
    }
    CC_SELECTION_DIAG("CustomComponents/Pick3/TRICK_EFFECT_PATH: phase=%s guid=%d cardEntity=%d count=%d ids=[%s]%s", phase, guid, source, list->size, ids, list->size>12?" truncated=1":"");
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
        CC_SELECTION_DIAG("CustomComponents/Pick3/TRICK_EFFECT_PATH: phase=IS_TARGETED guid=%d cardEntity=%d effectId=%d stock=%d returned=%d zoneExperiment=%d scoped=%d",
                 trickEffectBurstGuid, trickEffectBurstEntity, effectId, stock?1:0,
                 result?1:0, overrideToZone?1:0, scope?1:0);
        if (overrideToZone && ++trickInitialZoneOverrideCount <= 24)
            CC_SELECTION_DIAG("CustomComponents/Pick3/TRICK_INITIAL_ZONE: phase=OVERRIDE guid=%d cardEntity=%d effectId=%d count=%u initialBurstOnly=1 secondaryFilterUntouched=1",
                     trickEffectBurstGuid, trickEffectBurstEntity, effectId, trickInitialZoneOverrideCount);
    }
    return result;
}
inline Obj* TrickEffectValidTargets(Obj* self, int effectId, const void* method) {
    Obj* result = originalEffectValidTargets ? originalEffectValidTargets(self, effectId, method) : nullptr;
    if (trickEffectBurstDepth && TrickFocus(trickEffectBurstGuid)) {
        CC_SELECTION_DIAG("CustomComponents/Pick3/TRICK_EFFECT_PATH: phase=EFFECT_TARGET_REQUEST guid=%d cardEntity=%d effectId=%d", trickEffectBurstGuid, trickEffectBurstEntity, effectId);
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
            CC_SELECTION_DIAG("CustomComponents/Pick3/PICK3_STOCK_PRIMARY_CAPTURE: card=%d guid=%d effect=%d model=%p pool=%p count=%d copied=%d firstId=%d secondId=%d reason=%s secondaryPairVerified=0 decision=stock-unchanged",
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
            if (trace) CC_SELECTION_DIAG("CustomComponents/Pick3/DEFERRED_TRIAL: phase=ZONE_RECORDED guid=%d card=%d zone=%d",guid,id,zone);
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
}
