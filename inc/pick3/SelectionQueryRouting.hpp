#include "diagnostics/DiagnosticLogging.hpp"
#include "engine/SubsetSelection.hpp"
#include "core/HookRegistry.hpp"
#pragma once
// CustomComponents 2.8: guarded production exact and deferred-origin failure diagnosis, call-scoped GUID-query provenance.
//
// CardFromSubsetFinder.FindCard(query, Effect) receives the actual stock Effect,
// including EffectGroupId, whereas EntityQueryContext lacks the group. No
// effect/context pointer is stored beyond the synchronous FindCard call.
// Both GUID and live-entity domains share the verified origin boundary.
// Cross-effect entity queries without this proof retain their stock-compatible legacy result.
//
// This complete routing mechanism is compiled in, but actual replacement of
// legacy query results is enabled for verified FindCard/RunQuery scopes only.
// Define CUSTOMCOMPONENTS_PF3_EXACT_ROUTING=0 to immediately return to shadow-only.
#ifndef CUSTOMCOMPONENTS_PF3_EXACT_ROUTING
#define CUSTOMCOMPONENTS_PF3_EXACT_ROUTING 1
#endif
#include <atomic>
#include <cstdint>
#include <BNM/Class.hpp>
#include <BNM/Field.hpp>
#include <BNM/Method.hpp>
#include "pick3/PickedFromThreeSelection.hpp"
#include "pick3/SelectionQueryScope.hpp"
#include "pick3/SelectionExecutionScope.hpp"
#include "log.h"

namespace CustomComponents::Pick3::ExactRouting {
using Obj = BNM::IL2CPP::Il2CppObject;
inline BNM::Field<int> stockEffectId;
inline BNM::Field<int> stockEffectGroup;
inline BNM::Field<bool> stockCancelled;
inline int (*originalStockFindCard)(Obj*,Obj*,Obj*,const void*) = nullptr;
inline std::atomic<unsigned> scopes{0};
inline std::atomic<unsigned> candidates{0};
inline std::atomic<unsigned> fallbacks{0};
inline std::atomic<unsigned> mismatches{0};
inline std::atomic<unsigned> differences{0};
inline std::atomic<unsigned> routed{0};
inline std::atomic<unsigned> originalMissing{0};
inline std::atomic<unsigned> positiveMatches{0};
inline std::atomic<unsigned> olderReads{0};
inline std::atomic<unsigned> lookupFailures{0};
inline std::atomic<unsigned> shadowComparisons{0};
inline std::atomic<unsigned> appliedDifferences{0};
inline std::atomic<unsigned> appliedSame{0};
inline std::atomic<unsigned> deferredUnbound{0};
inline std::atomic<unsigned> executionCandidate{0};
inline std::atomic<unsigned> executionMismatch{0};
// Bounded diagnostic sampling: never query the ledger again just to log.
inline bool ImportantCount(unsigned n) { return n<=16 || (n & (n-1))==0; }
inline constexpr unsigned kDetailLimit = 128;

struct State {
    int effect = -1;
    int group = -1;
    std::uint64_t sequence = 0;
    unsigned totalMatches = 0;
    bool cancelled = false;
    bool cancellationKnown = false;
    const char* origin = "NONE";
    unsigned depth = 0;
    std::uint64_t newerSequence = 0;
    int newerGroup = -1;
};
inline thread_local State active{};

// 2.8: a queued PlayTrigger may evaluate the GUID query after FindCard has
// returned. Observe its *value-only* execution identity without claiming that
// a matching effect ID proves historical group ancestry. No new IL2CPP calls.
// Queries outside verified FindCard/RunQuery stay on the 2.7 legacy fallback.
inline void TraceDeferredOrigin(int effect, int guid, bool legacy) {
    const auto exec=Provenance::active;
    if(exec.depth && exec.boundary &&
       std::strcmp(exec.boundary,"QUEUE_PROCESS_EFFECT")==0) {
        const bool same=exec.effect==effect;
        const unsigned n=same?++executionCandidate:++executionMismatch;
        if(ImportantCount(n))
            CC_SELECTION_DIAG("CustomComponents/Pick3/PATCH2_ROUTING: phase=%s event=%u queryEffect=%d executionEffect=%d executionGroup=%d candidateSeq=%llu candidateGuid=%d legacy=%d reason=%s exactApplied=0",
                     same?"DEFERRED_EXECUTION_CANDIDATE":"DEFERRED_EXECUTION_MISMATCH",
                     n,effect,exec.effect,exec.group,
                     (unsigned long long)exec.snapshot,guid,int(legacy),
                     same?"EXECUTION_IDENTITY_NOT_QUERY_ANCESTRY":"EFFECT_IDS_DISAGREE");
    } else if(effect>0) {
        const unsigned n=++deferredUnbound;
        // A bounded sample of misses, not a managed getter or an expensive
        // scan for every candidate evaluated by stock EntityFinder.
        if(ImportantCount(n))
            CC_SELECTION_DIAG("CustomComponents/Pick3/PATCH2_ROUTING: phase=DEFERRED_ORIGIN_BLOCKED event=%u queryEffect=%d candidateGuid=%d legacy=%d reason=NO_VERIFIED_EFFECT_GROUP_FOR_QUERY finderDepth=%u queueDepth=%u groupGuessForbidden=1 legacyPreserved=1",
                     n,effect,guid,int(legacy),QueryCaller::active.depth,exec.depth);
    }
}

inline void ReportFallback(const char* reason, int effect, int group, int guid,
                           std::uint64_t seq=0) {
    const unsigned n=++fallbacks;
    if (n<=kDetailLimit || n==512 || n==2048)
        CC_SELECTION_DIAG("CustomComponents/Pick3/PATCH2_ROUTING: phase=QUERY_FALLBACK event=%u reason=%s effect=%d group=%d seq=%llu candidateGuid=%d legacyPreserved=1 routingEnabled=%d",
                 n,reason,effect,group,(unsigned long long)seq,guid,int(CUSTOMCOMPONENTS_PF3_EXACT_ROUTING));
}

// All generation checks are by copied IDs, never by a retained object pointer.
// *Every* matching generation counts, including failed or erased selections.
inline State Identify(int effect, int group, bool cancellationKnown, bool cancelled,
                      const char* origin) {
    State result{};
    result.effect=effect; result.group=group;
    result.cancellationKnown=cancellationKnown;
    result.cancelled=cancelled;
    result.origin=origin;
    if(effect<=0 || group<0) return result;
    const auto history=Picked::InspectFinderHistory(effect,group);
    result.totalMatches=history.matches;result.sequence=history.sequence;
    result.newerSequence=history.newerSequence;result.newerGroup=history.newerGroup;
    return result;
}

struct ScopedStockFinder {
    State previous;
    State current;
    unsigned event=0;
    explicit ScopedStockFinder(Obj* effect, const char* origin)
        : previous(active) {
        event=++scopes;
        const bool idReady=stockEffectId.IsValid() && stockEffectGroup.IsValid();
        const int id=effect && idReady ? stockEffectId[effect].Get() : -1;
        const int group=effect && idReady ? stockEffectGroup[effect].Get() : -1;
        const bool cancellationKnown=effect && stockCancelled.IsValid();
        const bool cancelled=cancellationKnown && stockCancelled[effect].Get();
        current=Identify(id,group,cancellationKnown,cancelled,origin);
        current.depth=previous.depth+1;
        active=current;
        if(event<=kDetailLimit || (current.totalMatches>1 && event<=512))
            CC_SELECTION_DIAG("CustomComponents/Pick3/PATCH2_ROUTING: phase=ORIGIN_SCOPE_ENTER event=%u origin=%s effect=%d group=%d seq=%llu generations=%u cancelled=%d cancelKnown=%d depth=%u routingEnabled=%d",
                     event,origin,id,group,(unsigned long long)current.sequence,current.totalMatches,int(cancelled),int(cancellationKnown),current.depth,int(CUSTOMCOMPONENTS_PF3_EXACT_ROUTING));
        if(!effect) ReportFallback("FINDER_EFFECT_NULL",id,group,-1);
        else if(!idReady) ReportFallback("EFFECT_FIELDS_UNAVAILABLE",id,group,-1);
        else if(id<=0 || group<0) ReportFallback("INVALID_STOCK_EFFECT_IDENTITY",id,group,-1);
        else if(!cancellationKnown) ReportFallback("CANCELLED_FIELD_UNAVAILABLE",id,group,-1);
        else if(cancelled) ReportFallback("STOCK_EFFECT_CANCELLED",id,group,-1);
        else if(!current.totalMatches) ReportFallback("NO_EXACT_EFFECT_GROUP_HISTORY",id,group,-1);
        else if(current.totalMatches>1) ReportFallback("MULTIPLE_GENERATIONS_SAME_EFFECT_GROUP",id,group,-1);
        else {
            const unsigned n=++candidates;
            if(n<=kDetailLimit)
                CC_SELECTION_DIAG("CustomComponents/Pick3/PATCH2_ROUTING: phase=ORIGIN_BOUND event=%u source=%s effect=%d group=%d seq=%llu meaning=CALL_LOCAL_IDENTITY_NOT_COMMIT_PROOF",
                         n,origin,id,group,(unsigned long long)current.sequence);
        }
    }
    ~ScopedStockFinder() {
        if(event<=kDetailLimit) CC_SELECTION_DIAG("CustomComponents/Pick3/PATCH2_ROUTING: phase=ORIGIN_SCOPE_EXIT event=%u effect=%d group=%d seq=%llu restoredDepth=%u",
                                    event,current.effect,current.group,(unsigned long long)current.sequence,previous.depth);
        active=previous;
    }
    ScopedStockFinder(const ScopedStockFinder&)=delete;
    ScopedStockFinder& operator=(const ScopedStockFinder&)=delete;
};

// Requiring identical RunQuery context/effect ties the comparison to the
// *actual* query invocation. Returning false means "keep legacy result";
// out is then ignored. Staged choices are intentionally readable inside an
// active stock finder, but this never promotes them to model-committed state.
enum class Domain : std::uint8_t { Guid, Entity };

inline bool EvaluateDomain(Obj* context, int queryEffect, int candidateEntity,
                          int candidateGuid, Domain domain, bool legacy, bool& out) {
    out=legacy;
    const auto scope=active;
    if(!scope.depth) { TraceDeferredOrigin(queryEffect,candidateGuid,legacy); ReportFallback("NO_STOCK_FINDCARD_SCOPE",queryEffect,-1,candidateGuid);return false; }
    if(!scope.cancellationKnown) { ReportFallback("CANCELLED_FIELD_UNAVAILABLE",scope.effect,scope.group,candidateGuid);return false; }
    if(scope.cancelled) { ReportFallback("STOCK_EFFECT_CANCELLED",scope.effect,scope.group,candidateGuid);return false; }
    if(scope.effect<=0 || scope.group<0 || !scope.sequence || scope.totalMatches!=1) {
        ReportFallback(scope.totalMatches>1?"ORIGIN_AMBIGUOUS": "ORIGIN_MISSING",
                       scope.effect,scope.group,candidateGuid);return false;
    }
    if(queryEffect!=scope.effect) {
        if(++mismatches<=kDetailLimit)
            CC_SELECTION_DIAG("CustomComponents/Pick3/PATCH2_ROUTING: phase=ORIGIN_MISMATCH queryEffect=%d scopeEffect=%d group=%d seq=%llu reason=CONTEXT_EFFECT_DIFFERENT fallback=1",
                     queryEffect,scope.effect,scope.group,(unsigned long long)scope.sequence);
        return false;
    }
    const auto finder=QueryCaller::active;
    if(!context || !finder.depth || finder.context!=context || finder.effect!=queryEffect) {
        ReportFallback(finder.depth?"FINDER_CONTEXT_MISMATCH":"NO_RUNQUERY_SCOPE",
                       queryEffect,scope.group,candidateGuid,scope.sequence);
        return false;
    }
    if(domain==Domain::Guid && candidateGuid<=0) { ReportFallback("INVALID_CANDIDATE_GUID",queryEffect,scope.group,candidateGuid,scope.sequence);return false; }
    if(domain==Domain::Entity && candidateEntity<=0) { ReportFallback("INVALID_CANDIDATE_ENTITY",queryEffect,scope.group,candidateGuid,scope.sequence);return false; }
    bool exact=false;
    const auto lookup=Picked::MatchesExactSnapshot(scope.effect,scope.group,scope.sequence,
                                                  candidateEntity,candidateGuid,
                                                  domain==Domain::Guid,exact,false);
    if(lookup!=Picked::SnapshotLookup::Found) {
        const unsigned failure=++lookupFailures;
        if(ImportantCount(failure))
            CC_SELECTION_DIAG("CustomComponents/Pick3/PATCH2_ROUTING: phase=SNAPSHOT_LOOKUP_FAILURE event=%u effect=%d group=%d seq=%llu guid=%d reason=%s legacyPreserved=1",
                     failure,scope.effect,scope.group,(unsigned long long)scope.sequence,candidateGuid,Picked::SnapshotLookupName(lookup));
        ReportFallback(Picked::SnapshotLookupName(lookup),queryEffect,scope.group,candidateGuid,scope.sequence);
        return false;
    }
    const unsigned comparisons=++shadowComparisons;
    if(domain==Domain::Entity && (exact!=legacy || ImportantCount(comparisons)))
        CC_SELECTION_DIAG("CustomComponents/Pick3/PATCH2_ENTITY_ROUTING: phase=EXACT_COMPARE effect=%d group=%d seq=%llu entity=%d guid=%d legacy=%d exact=%d bound=FINDCARD_RUNQUERY",
                 scope.effect,scope.group,(unsigned long long)scope.sequence,candidateEntity,candidateGuid,int(legacy),int(exact));
    if(exact) {
        const unsigned n=++positiveMatches;
        if(ImportantCount(n))
            CC_SELECTION_DIAG("CustomComponents/Pick3/PATCH2_ROUTING: phase=%s event=%u effect=%d group=%d seq=%llu guid=%d legacy=%d exact=%d newerSeq=%llu commitProven=0 routingEnabled=%d",
                     legacy?"POSITIVE_EXACT_MATCH":"POSITIVE_EXACT_DIFFERENCE",
                     n,scope.effect,scope.group,(unsigned long long)scope.sequence,candidateGuid,int(legacy),int(exact),
                     (unsigned long long)scope.newerSequence,int(CUSTOMCOMPONENTS_PF3_EXACT_ROUTING));
    }
    if(scope.newerSequence) {
        const unsigned n=++olderReads;
        if(ImportantCount(n))
            CC_SELECTION_DIAG("CustomComponents/Pick3/PATCH2_ROUTING: phase=OLDER_SNAPSHOT_READ event=%u effect=%d group=%d seq=%llu newerGroup=%d newerSeq=%llu guid=%d legacy=%d exact=%d commitProven=0",
                     n,scope.effect,scope.group,(unsigned long long)scope.sequence,scope.newerGroup,
                     (unsigned long long)scope.newerSequence,candidateGuid,int(legacy),int(exact));
    }
    if(legacy!=exact) {
        const unsigned n=++differences;
        if(ImportantCount(n))
            CC_SELECTION_DIAG("CustomComponents/Pick3/PATCH2_ROUTING: phase=RESULT_DIFFERENCE event=%u effect=%d group=%d seq=%llu guid=%d legacy=%d exact=%d verifiedContext=1 commitProven=0",
                     n,scope.effect,scope.group,(unsigned long long)scope.sequence,candidateGuid,int(legacy),int(exact));
    }
    if(comparisons==512 || comparisons==2048 || comparisons==8192)
        CC_SELECTION_DIAG("CustomComponents/Pick3/PATCH2_ROUTING: phase=VALIDATION_SUMMARY comparisons=%u positives=%u olderReads=%u resultDifferences=%u lookupFailures=%u fallback=%u routingEnabled=%d",
                 comparisons,positiveMatches.load(),olderReads.load(),differences.load(),lookupFailures.load(),fallbacks.load(),int(CUSTOMCOMPONENTS_PF3_EXACT_ROUTING));
    const unsigned n=++routed;
    if(n<=kDetailLimit || n==512 || n==2048)
        CC_SELECTION_DIAG("CustomComponents/Pick3/PATCH2_ROUTING: phase=%s event=%u effect=%d group=%d seq=%llu candidateGuid=%d legacy=%d exact=%d status=IN_FLIGHT_OR_CALLBACK_OBSERVED commitProven=0 routingEnabled=%d",
                 CUSTOMCOMPONENTS_PF3_EXACT_ROUTING?"QUERY_EXACT_MATCH":"SHADOW_EXACT",
                 n,scope.effect,scope.group,(unsigned long long)scope.sequence,candidateGuid,int(legacy),int(exact),int(CUSTOMCOMPONENTS_PF3_EXACT_ROUTING));
#if CUSTOMCOMPONENTS_PF3_EXACT_ROUTING
    // Production substitution occurs only after all verified scope/context,
    // cancellation, generation and snapshot lookup checks above have passed.
    // No additional managed getters are invoked on this hot path.
    out=exact;
    if(domain==Domain::Entity && exact!=legacy)
        CC_SELECTION_DIAG("CustomComponents/Pick3/PATCH2_ENTITY_ROUTING: phase=EXACT_APPLIED_DIFFERENCE effect=%d group=%d seq=%llu entity=%d guid=%d legacy=%d returned=%d",
                 scope.effect,scope.group,(unsigned long long)scope.sequence,candidateEntity,candidateGuid,int(legacy),int(exact));
    if (exact != legacy) {
        const unsigned n=++appliedDifferences;
        // Never suppress an actual behavior change behind the detail cap.
        CC_SELECTION_DIAG("CustomComponents/Pick3/PATCH2_ROUTING: phase=QUERY_EXACT_APPLIED_DIFFERENCE event=%u effect=%d group=%d seq=%llu guid=%d legacy=%d returned=%d reason=VERIFIED_FINDCARD_RUNQUERY_SCOPE commitProven=0",
                 n,scope.effect,scope.group,(unsigned long long)scope.sequence,candidateGuid,int(legacy),int(exact));
    } else {
        const unsigned n=++appliedSame;
        if(ImportantCount(n))
            CC_SELECTION_DIAG("CustomComponents/Pick3/PATCH2_ROUTING: phase=QUERY_EXACT_APPLIED_SAME event=%u effect=%d group=%d seq=%llu guid=%d returned=%d reason=VERIFIED_FINDCARD_RUNQUERY_SCOPE commitProven=0",
                     n,scope.effect,scope.group,(unsigned long long)scope.sequence,candidateGuid,int(exact));
    }
#endif
    return true;
}

// A single verified provenance mechanism supports both query domains.
inline bool Evaluate(Obj* context, int queryEffect, int candidateGuid,
                     bool legacy, bool& out) {
    return EvaluateDomain(context,queryEffect,-1,candidateGuid,Domain::Guid,legacy,out);
}
inline bool EvaluateEntity(Obj* context, int queryEffect, int candidateEntity,
                           int candidateGuid, bool legacy, bool& out) {
    return EvaluateDomain(context,queryEffect,candidateEntity,candidateGuid,Domain::Entity,legacy,out);
}

// Fallback when the production creation adapter does not own FindCard.
// Original gets invoked exactly once. No global effect pointer is cached.
inline int FindCardHook(Obj* self,Obj* query,Obj* effect,const void* method) {
    Engine::SubsetSelection::Scope sharedScope(self,query,effect);
    ScopedStockFinder scope(effect,"PATCH2_STOCK_FINDCARD");
    if(!originalStockFindCard) {
        if(++originalMissing<=32) LOG_ERR("CustomComponents/Pick3/PATCH2_ROUTING: phase=ORIGINAL_MISSING boundary=STOCK_FINDCARD returning=-1");
        return -1;
    }
    return sharedScope.Finish(originalStockFindCard(self,query,effect,method));
}
inline bool Initialize(bool installOwnFinderHook) {
    auto effect=BNM::Class("PvZCards.Engine","Effect");
    stockEffectId=effect.GetField("EffectEntityId").cast<int>();
    stockEffectGroup=effect.GetField("EffectGroupId").cast<int>();
    stockCancelled=effect.GetField("Cancelled").cast<bool>();
    bool installed=false;
    bool resolved=false;
    if(installOwnFinderHook) {
        auto find=BNM::Class("PvZCards.Core.Systems.Utilities","CardFromSubsetFinder")
            .GetMethod("FindCard",2);
        resolved=find.IsValid();
        if(resolved && stockEffectId.IsValid() && stockEffectGroup.IsValid() && stockCancelled.IsValid())
            CustomComponents::Core::Hooks::Install(find,FindCardHook,originalStockFindCard);
        installed=originalStockFindCard!=nullptr;
    }
    CC_SELECTION_DIAG("CustomComponents/Pick3/PATCH2_ROUTING: phase=INIT version=2.16 ownHookRequested=%d findResolved=%d findInstalled=%d fieldEffect=%d fieldGroup=%d fieldCancelled=%d exactEnabled=%d fallback=LEGACY",
             int(installOwnFinderHook),int(resolved),int(installed),int(stockEffectId.IsValid()),int(stockEffectGroup.IsValid()),int(stockCancelled.IsValid()),int(CUSTOMCOMPONENTS_PF3_EXACT_ROUTING));
    if((installOwnFinderHook&&!installed) || !stockEffectId.IsValid() || !stockEffectGroup.IsValid() || !stockCancelled.IsValid())
        LOG_ERR("CustomComponents/Pick3/PATCH2_ROUTING: phase=HOOK_OR_FIELD_FAILURE reason=NO_VERIFIED_STOCK_FINDER_OR_IDENTITY originFallback=LEGACY");
    return !installOwnFinderHook || installed;
}
} // namespace CustomComponents::Pick3::ExactRouting
