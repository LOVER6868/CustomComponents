#pragma once
// CustomComponents 2.4 — read-only shadow comparisons, NOT an authority switch.
// Neither Finder.RunQuery nor EntityQueryContext carries the selection group.
// A unique effect-ID candidate is diagnostic only, never proven ancestry.
#include <atomic>
#include <cstdint>
#include "pick3/PickedFromThreeSelection.hpp"
#include "pick3/Patch2QueryCallerObservation.hpp"
#include "log.h"

namespace CustomComponents::Pick3::Shadow {
using Obj = BNM::IL2CPP::Il2CppObject;
inline std::atomic<unsigned> calls{0};
inline std::atomic<unsigned> callerMissing{0};
inline std::atomic<unsigned> callerMismatch{0};
inline std::atomic<unsigned> historyMissing{0};
inline std::atomic<unsigned> ambiguous{0};
inline std::atomic<unsigned> uncommitted{0};
inline std::atomic<unsigned> rejected{0};
inline std::atomic<unsigned> compareCount{0};
inline std::atomic<unsigned> differences{0};
inline std::atomic<unsigned> independentCount{0};
inline constexpr unsigned kLimit = 96;

// Logs reasons even when no comparison is possible. Everything returns void.
// Caller pointers are compared only while RunQuery's TLS scope is active.
inline void Observe(const char* domain, Obj* context, int effect,
                    int entityId, int guid, bool legacy, bool independent) {
    const unsigned event = ++calls;
    const bool sample = event <= kLimit;
    if (event == 128 || event == 512 || event == 1024) {
        LOG_INFO("CustomComponents/Pick3/PATCH2_SHADOW: phase=SHADOW_SUMMARY event=%u compared=%u differences=%u missing=%u ambiguous=%u uncommitted=%u rejected=%u callerMissing=%u callerMismatch=%u independent=%u queryUnchanged=1",event,compareCount.load(),differences.load(),historyMissing.load(),ambiguous.load(),uncommitted.load(),rejected.load(),callerMissing.load(),callerMismatch.load(),independentCount.load());
    }
    const auto caller = QueryCaller::active;
    const bool validCaller = caller.depth && context && caller.context == context && caller.effect == effect;
    if (independent) {
        const unsigned n = ++independentCount;
        if (n <= 32) LOG_INFO("CustomComponents/Pick3/PATCH2_SHADOW: phase=SHADOW_INDEPENDENT event=%u domain=%s effect=%d entity=%d guid=%d legacy=%d reason=INTENTIONALLY_CROSS_EFFECT queryUnchanged=1",event,domain,effect,entityId,guid,int(legacy));
        return;
    }
    if (!caller.depth) {
        if (++callerMissing <= 32) LOG_INFO("CustomComponents/Pick3/PATCH2_SHADOW: phase=SHADOW_CALLER_MISSING event=%u domain=%s effect=%d reason=NO_FINDER_SCOPE queryUnchanged=1",event,domain,effect);
        return;
    }
    if (!validCaller) {
        if (++callerMismatch <= 32) LOG_INFO("CustomComponents/Pick3/PATCH2_SHADOW: phase=SHADOW_CALLER_MISMATCH event=%u domain=%s effect=%d callerEffect=%d contextSame=%d reason=CONTEXT_OR_EFFECT_DIFFERENT queryUnchanged=1",event,domain,effect,caller.effect,int(context && caller.context == context));
        return;
    }
    if (effect <= 0) {
        if (++historyMissing <= 32) LOG_INFO("CustomComponents/Pick3/PATCH2_SHADOW: phase=SHADOW_MISSING event=%u domain=%s effect=%d reason=INVALID_EFFECT_ID queryUnchanged=1",event,domain,effect);
        return;
    }
    const auto history = Picked::CopySnapshotHistory();
    // Count *all* generations before checking eligibility: never silently
    // prefer a newer group or a successful callback over an older snapshot.
    unsigned candidates = 0;
    Picked::SelectionSnapshot single;
    for (const auto& entry : history) {
        if (entry.selection.effectEntityId != effect) continue;
        ++candidates;
        if (candidates == 1) single = entry;
    }
    if (!candidates) {
        if (++historyMissing <= 32) LOG_INFO("CustomComponents/Pick3/PATCH2_SHADOW: phase=SHADOW_MISSING event=%u domain=%s effect=%d reason=NO_SNAPSHOT_FOR_EFFECT queryUnchanged=1",event,domain,effect);
        return;
    }
    if (candidates != 1) {
        if (++ambiguous <= 64) LOG_INFO("CustomComponents/Pick3/PATCH2_SHADOW: phase=SHADOW_AMBIGUOUS event=%u domain=%s effect=%d generations=%u reason=QUERY_CONTEXT_LACKS_GROUP_AND_GENERATION noGuess=1 queryUnchanged=1",event,domain,effect,candidates);
        return;
    }
    if (single.observation == Picked::SnapshotObservation::CallbackFailureObserved ||
        single.observation == Picked::SnapshotObservation::ErasedBeforeCommitProof) {
        if (++rejected <= 32) LOG_INFO("CustomComponents/Pick3/PATCH2_SHADOW: phase=SHADOW_REJECTED event=%u domain=%s effect=%d group=%d seq=%llu state=%s reason=FAILED_OR_ERASED queryUnchanged=1",event,domain,effect,single.selection.effectGroupId,(unsigned long long)single.sequence,Picked::SnapshotObservationName(single.observation));
        return;
    }
    if (single.observation != Picked::SnapshotObservation::CallbackSuccessObserved) {
        if (++uncommitted <= 64) LOG_INFO("CustomComponents/Pick3/PATCH2_SHADOW: phase=SHADOW_UNCOMMITTED event=%u domain=%s effect=%d group=%d seq=%llu state=%s reason=NO_CALLBACK_SUCCESS_AND_NO_MODEL_COMMIT_PROOF queryUnchanged=1",event,domain,effect,single.selection.effectGroupId,(unsigned long long)single.sequence,Picked::SnapshotObservationName(single.observation));
        // We may still compare this staged candidate diagnostically; it is never authoritative.
    } else if (sample) {
        LOG_INFO("CustomComponents/Pick3/PATCH2_SHADOW: phase=SHADOW_UNVERIFIED_COMMIT event=%u domain=%s effect=%d group=%d seq=%llu reason=CALLBACK_SUCCESS_IS_NOT_STOCK_COMMIT_PROOF queryUnchanged=1",event,domain,effect,single.selection.effectGroupId,(unsigned long long)single.sequence);
    }
    bool shadowMatch = false;
    const bool guidDomain = !independent; // Only GUID is dependent in current module.
    for (const auto& card : single.selection.cards) {
        if (guidDomain && guid > 0 && card.cardGuid == guid) shadowMatch = true;
    }
    const unsigned tested = ++compareCount;
    if (legacy != shadowMatch) {
        if (++differences <= 96) LOG_INFO("CustomComponents/Pick3/PATCH2_SHADOW: phase=SHADOW_RESULT_DIFFERENCE event=%u domain=%s effect=%d group=%d seq=%llu candidateGuid=%d legacy=%d shadow=%d callerVerified=1 ancestryProven=0 commitProven=0 reason=UNIQUE_EFFECT_CANDIDATE_NOT_ORIGIN_PROOF queryUnchanged=1",event,domain,effect,single.selection.effectGroupId,(unsigned long long)single.sequence,guid,int(legacy),int(shadowMatch));
    } else if (sample) {
        LOG_INFO("CustomComponents/Pick3/PATCH2_SHADOW: phase=SHADOW_COMPARE event=%u domain=%s effect=%d group=%d seq=%llu candidateGuid=%d legacy=%d shadow=%d callerVerified=1 ancestryProven=0 commitProven=0 queryUnchanged=1",event,domain,effect,single.selection.effectGroupId,(unsigned long long)single.sequence,guid,int(legacy),int(shadowMatch));
    }
    if (tested == 128 || tested == 512)
        LOG_INFO("CustomComponents/Pick3/PATCH2_SHADOW: phase=SHADOW_SUMMARY evaluated=%u differences=%u missing=%u ambiguous=%u uncommitted=%u rejected=%u callerMissing=%u callerMismatch=%u independent=%u queryUnchanged=1",tested,differences.load(),historyMissing.load(),ambiguous.load(),uncommitted.load(),rejected.load(),callerMissing.load(),callerMismatch.load(),independentCount.load());
}
} // namespace CustomComponents::Pick3::Shadow
