// Host regression for Patch 2 milestone A. Build with project log.h and
// android/log.h stubs: clang++ -std=c++20 ... thisfile.cpp -o test
#include <cassert>
#include <cstdarg>
#include <vector>
#include "pick3/PickedFromThreeSelection.hpp"
extern "C" int __android_log_print(int, const char*, const char*, ...) { return 0; }
using namespace CustomComponents::Pick3::Picked;
int main() {
    ResetMatch();
    assert(Store(59, 406, {{30, 710, -1}}));
    assert(Matches(59, 710, true));
    assert(Store(59, 524, {{41, 695, -1}}));
    // Legacy current-view behavior is deliberately unchanged.
    assert(Matches(59, 695, true));
    assert(!Matches(59, 710, true));
    assert(MatchesLatestLiveCard(41, 695));
    auto h = CopySnapshotHistory();
    assert(h.size() == 2);
    assert(h[0].sequence != h[1].sequence);
    assert(h[0].selection.effectGroupId == 406);
    assert(h[0].selection.cards[0].cardGuid == 710);
    assert(h[1].selection.effectGroupId == 524);
    ObserveCompletion(59, 524, true);
    h = CopySnapshotHistory();
    assert(h[0].observation == SnapshotObservation::Staged);
    assert(h[1].observation == SnapshotObservation::CallbackSuccessObserved);
    Erase(59, 524);
    h = CopySnapshotHistory();
    assert(h[1].observation == SnapshotObservation::ErasedBeforeCommitProof);
    assert(!Matches(59, 695, true));
    assert(Store(31, 300, {{-1, 901, -1}}));
    assert(BindCreated(31, 901, 77));
    h = CopySnapshotHistory();
    assert(h.back().selection.cards[0].boundEntityId == 77);
    int bound = -1;
    assert(TryGetSingleBoundDatabaseEntity(31, 300, bound) && bound == 77);
    assert(Store(31, 301, {{-1, 901, -1}}));
    assert(BindCreated(31, 901, 88));
    h = CopySnapshotHistory();
    assert(h[h.size()-2].selection.cards[0].boundEntityId == 77);
    assert(h.back().selection.cards[0].boundEntityId == 88);
    ResetMatch();
    assert(CopySnapshotHistory().empty());
    assert(!Matches(31, 901, true));
}
