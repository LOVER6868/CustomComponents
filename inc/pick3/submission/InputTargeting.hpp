#include "diagnostics/DiagnosticLogging.hpp"
#pragma once
// Private implementation module. Include PickFromThreeSubmission.hpp, not this file.
namespace CustomComponents::Pick3::Submission {
inline void TrickLogIdentity(const char* stage, unsigned seq, int id);
inline void TrickLogTargetsIdentity(const char* stage, unsigned seq, IntList* list);
inline void TrickLogIntList(const char* stage, unsigned seq, int cardId, IntList* list) {
    if (!list) {
        CC_SELECTION_DIAG("CustomComponents/Pick3/TRICK_TARGET_IDS: phase=%s seq=%u card=%d list=null", stage, seq, cardId);
        return;
    }
    const int count = list->size;
    if (count < 0 || count > 4096 || (count && !list->items)) {
        CC_SELECTION_DIAG("CustomComponents/Pick3/TRICK_TARGET_IDS: phase=%s seq=%u card=%d invalidListCount=%d", stage, seq, cardId, count);
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
    CC_SELECTION_DIAG("CustomComponents/Pick3/TRICK_TARGET_IDS: phase=%s seq=%u card=%d count=%d ids=[%s]%s", stage, seq, cardId, count, ids, count>maxPrint?" truncated=1":"");
}
inline void TrickStartDragTrace(Obj* self, const void* method) {
    const unsigned seq = ++trickStartObservations;
    if (seq <= 100) CC_SELECTION_DIAG("CustomComponents/Pick3/TRICK_DRAG_LIFE: phase=START_ENTER seq=%u card=%d", seq, self ? *reinterpret_cast<const int*>(reinterpret_cast<const std::uint8_t*>(self)+0x48) : -1);
    if (originalTrickStartDrag) originalTrickStartDrag(self, method);
    if (seq <= 100) CC_SELECTION_DIAG("CustomComponents/Pick3/TRICK_DRAG_LIFE: phase=START_EXIT seq=%u", seq);
}
inline void TrickUpdateDragTrace(Obj* self, const void* method) {
    if (originalTrickUpdateDrag) originalTrickUpdateDrag(self, method);
    const unsigned seq=++trickUpdateObservations;
    if (seq <= 15 || seq%80==0) {
        Obj* model = self ? *reinterpret_cast<Obj* const*>(reinterpret_cast<const std::uint8_t*>(self)+0x50) : nullptr;
        const int cardId = model && trickDraggedCardField.IsValid() ? trickDraggedCardField[model].Get() : -1;
        CC_SELECTION_DIAG("CustomComponents/Pick3/TRICK_DRAG_LIFE: phase=UPDATE seq=%u card=%d", seq, cardId);
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
        CC_SELECTION_DIAG("CustomComponents/Pick3/TRICK_CACHE: phase=RESOLVE seq=%u dragSeq=%u card=%d cacheCount=%d firstValid=%d final=%d path=%s", seq,trickActiveDragSeq,cardId,trickCacheResultCount,trickCacheFirstResult,result,path);
    }
    trickCacheResolveDepth = oldDepth;
    trickCacheProbeSeq = oldSeq;
    trickCacheProbeCard = oldCard;
    trickCacheResultCount = oldCount;
    trickCacheFirstResult = oldFirst;
    if (seq <= 180) {
        CC_SELECTION_DIAG("CustomComponents/Pick3/TRICK_OVERLAP: seq=%u activeDrag=%u card=%d resolved=%d", seq, trickActiveDragSeq, cardId, result);
        CC_SELECTION_DIAG("CustomComponents/Pick3/TRICK_OVERLAP: seq=%u enumerable=%p note=not-assumed-List", seq, overlapped);
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
        CC_SELECTION_DIAG("CustomComponents/Pick3/TRICK_PLACE_DIAG: STOP_DRAG_ENTER seq=%u cardEntity=%d inHand=%d release=(%.3f,%.3f,%.3f) dragModel=%p stock=1",
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
        CC_SELECTION_DIAG("CustomComponents/Pick3/TRICK_PLACE_DIAG: STOP_DRAG_EXIT seq=%u cardEntity=%d stockReturned=1", seq, cardId);
}
inline void TrickLogGeneratedTargets(int player, int cardId, Obj* result);
inline Obj* TrickValidTargetsTrace(Obj* self, int playerId, int cardId, Obj* validator, const void* method) {
    Obj* result = originalTrickValidTargets ? originalTrickValidTargets(self, playerId, cardId, validator, method) : nullptr;
    TrickLogGeneratedTargets(playerId, cardId, result);
    const unsigned seq = ++trickTargetObservations;
    if (seq <= 240)
        CC_SELECTION_DIAG("CustomComponents/Pick3/TRICK_PLACE_DIAG: VALID_TARGETS seq=%u activeDrag=%u player=%d cardEntity=%d validator=%p list=%p count=unknown stockUnmodified=1",
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
}
