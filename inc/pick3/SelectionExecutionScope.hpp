#pragma once
#include "diagnostics/DiagnosticLogging.hpp"
// Patch 2.2: read-only, synchronous execution provenance observations.
// Never use this TLS state to decide gameplay/query results. A queued effect
// may execute later, on another thread, or on a different EntityModel.
#include <cstdint>
#include <atomic>
#include <cstring>
#include "log.h"
#include "pick3/PickedFromThreeSelection.hpp"

namespace CustomComponents::Pick3::Provenance {
struct ScopeState {
    int effect = -1;
    int group = -1;
    std::uint64_t snapshot = 0; // observational sequence, NOT proven commit
    unsigned depth = 0;
    const char* boundary = "NONE";
};
inline thread_local ScopeState active{};
inline std::atomic<unsigned> queryCount{0};
inline std::atomic<unsigned> orphanCount{0};
inline std::atomic<unsigned> queueCount{0};
inline std::atomic<unsigned> boundaryCount{0};
inline constexpr unsigned kMaxQueryLogs = 512;
inline constexpr unsigned kMaxQueueLogs = 512;
inline constexpr unsigned kMaxBoundaryLogs = 512;
inline std::atomic<unsigned> executionCount{0};
inline std::atomic<unsigned> resolutionCount{0};
inline std::atomic<unsigned> missingCount{0};
inline std::atomic<unsigned> ambiguousCount{0};
inline std::atomic<unsigned> mismatchCount{0};
inline std::atomic<unsigned> resolvedQueryCount{0};
inline std::atomic<unsigned> absentQueryCount{0};
inline constexpr unsigned kMaxExecutionSamples = 1024;


struct ScopedObservation {
    ScopeState previous;
    explicit ScopedObservation(int effect, int group, std::uint64_t snapshot,
                               const char* boundary) : previous(active) {
        active = {effect, group, snapshot, previous.depth + 1, boundary};
    }
    ~ScopedObservation() { active = previous; }
    ScopedObservation(const ScopedObservation&) = delete;
    ScopedObservation& operator=(const ScopedObservation&) = delete;
};
// The existing TriggeredEffectQueuingSystem.ProcessEffect hook owns the stock
// execution call. This observer does not install another detour, retain an
// Effect pointer, or infer ancestry from a pointer address. A unique historical
// effect/group collision is diagnostic evidence, NOT verified ancestry/commit.
struct ScopedQueueExecution {
    ScopeState previous;
    bool sampled = false;
    int effect = -1, group = -1;
    unsigned matches = 0;
    std::uint64_t sequence = 0;
    unsigned event = 0;
    explicit ScopedQueueExecution(int e, int g, int source, int target)
        : previous(active), effect(e), group(g) {
        active={e,g,0,previous.depth+1,"QUEUE_PROCESS_EFFECT"};
        if constexpr(!Diagnostics::kSelectionTrace) return;
        event = ++executionCount;
        if (event > kMaxExecutionSamples) return;
        sampled = true;
        if (e >= 0 && g >= 0) {
            sequence=Picked::UniqueObservedSequence(e,g,&matches);
        }
        // Even when the snapshot is ambiguous, the scope records the executing
        // stock effect identity. Sequence zero explicitly forbids exact lookup.
        active = {e, g, matches == 1 ? sequence : 0, previous.depth + 1,
                  "QUEUE_PROCESS_EFFECT"};
        if (matches == 1 && ++resolutionCount <= 128)
            CC_SELECTION_DIAG("CustomComponents/Pick3/PATCH2_PROVENANCE: phase=EXEC_ORIGIN_CANDIDATE event=%u effect=%d group=%d seq=%llu source=%d target=%d ancestryProven=0 commitProven=0 readOnly=1",event,e,g,(unsigned long long)sequence,source,target);
        else if (matches > 1 && ++ambiguousCount <= 32)
            CC_SELECTION_DIAG("CustomComponents/Pick3/PATCH2_PROVENANCE: phase=EXEC_ORIGIN_AMBIGUOUS event=%u effect=%d group=%d matches=%u seq=0 readOnly=1",event,e,g,matches);
        else if (!matches && ++missingCount <= 32)
            CC_SELECTION_DIAG("CustomComponents/Pick3/PATCH2_PROVENANCE: phase=EXEC_ORIGIN_MISSING event=%u effect=%d group=%d reason=NO_EXACT_HISTORY_OR_INVALID_ID readOnly=1",event,e,g);
    }
    ~ScopedQueueExecution() {
        if (sampled) {
            if (matches == 1 && resolutionCount.load() <= 128)
                CC_SELECTION_DIAG("CustomComponents/Pick3/PATCH2_PROVENANCE: phase=EXEC_EXIT event=%u effect=%d group=%d seq=%llu scopeRestored=1 readOnly=1",event,effect,group,(unsigned long long)sequence);
        }
        active = previous;
    }
    ScopedQueueExecution(const ScopedQueueExecution&) = delete;
    ScopedQueueExecution& operator=(const ScopedQueueExecution&) = delete;
};

inline void Query(const char* domain, int contextEffect, int guid, int entity,
                  bool legacyMatch) {
    if constexpr(!Diagnostics::kSelectionTrace) return;
    const unsigned n = ++queryCount;
    const bool contextual = active.depth && contextEffect == active.effect;
    if (n <= kMaxQueryLogs) {
        CC_SELECTION_DIAG("CustomComponents/Pick3/PATCH2_PROVENANCE: phase=QUERY domain=%s event=%u contextEffect=%d candidateGuid=%d candidateEntity=%d legacyMatch=%d scoped=%d scopeEffect=%d scopeGroup=%d scopeSeq=%llu scopeDepth=%u boundary=%s readOnly=1",
            domain,n,contextEffect,guid,entity,int(legacyMatch),int(contextual),active.effect,
            active.group,(unsigned long long)active.snapshot,active.depth,active.boundary);
    }
    if (active.depth && active.boundary &&
        std::strcmp(active.boundary, "QUEUE_PROCESS_EFFECT") == 0) {
        if (contextEffect == active.effect && active.snapshot && ++resolvedQueryCount <= 128)
            CC_SELECTION_DIAG("CustomComponents/Pick3/PATCH2_PROVENANCE: phase=QUERY_ORIGIN_CANDIDATE domain=%s contextEffect=%d group=%d snapshotSeq=%llu candidateGuid=%d candidateEntity=%d legacyMatch=%d ancestryProven=0 commitProven=0 queryUnchanged=1",domain,contextEffect,active.group,(unsigned long long)active.snapshot,guid,entity,int(legacyMatch));
        else if (contextEffect != active.effect && ++mismatchCount <= 32)
            CC_SELECTION_DIAG("CustomComponents/Pick3/PATCH2_PROVENANCE: phase=QUERY_EFFECT_MISMATCH domain=%s contextEffect=%d executingEffect=%d executingGroup=%d seq=%llu readOnly=1",domain,contextEffect,active.effect,active.group,(unsigned long long)active.snapshot);
        else if (contextEffect == active.effect && !active.snapshot && ++absentQueryCount <= 32)
            CC_SELECTION_DIAG("CustomComponents/Pick3/PATCH2_PROVENANCE: phase=QUERY_ORIGIN_UNAVAILABLE domain=%s effect=%d group=%d reason=MISSING_OR_AMBIGUOUS_SNAPSHOT readOnly=1",domain,contextEffect,active.group);
    }
    if (contextEffect > 0 && !contextual && ++orphanCount <= 24)
        CC_SELECTION_DIAG("CustomComponents/Pick3/PATCH2_PROVENANCE: phase=QUERY_ORIGIN_UNRESOLVED reason=NO_MATCHING_SYNCHRONOUS_SCOPE contextEffect=%d scopeEffect=%d scopeGroup=%d queuedMayBeDeferred=1 readOnly=1",contextEffect,active.effect,active.group);
}
inline void Queue(int effect, int group, int source, int target, bool inSelectService) {
    const unsigned n = ++queueCount;
    if (n > kMaxQueueLogs) return;
    const bool same = active.depth && effect == active.effect && group == active.group;
    CC_SELECTION_DIAG("CustomComponents/Pick3/PATCH2_PROVENANCE: phase=QUEUE_EFFECT event=%u effect=%d group=%d source=%d target=%d inSelectService=%d scope=%d scopeEffect=%d scopeGroup=%d scopeSeq=%llu scopeBoundary=%s match=%d readOnly=1",
        n,effect,group,source,target,int(inSelectService),active.depth!=0,active.effect,
        active.group,(unsigned long long)active.snapshot,active.boundary,int(same));
}
} // namespace CustomComponents::Pick3::Provenance
