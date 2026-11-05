#include <cassert>
#include <cstdarg>
#include <vector>
#include "pick3/PickedFromThreeSelection.hpp"
extern "C" int __android_log_print(int, const char*, const char*, ...) { return 0; }
using namespace CustomComponents::Pick3::Picked;
int main() {
    ResetMatch();
    assert(Store(63,390,{{24,710,-1}},"TX_SUBMIT"));
    assert(Store(63,390,{{24,710,-1}},"RX_LIVE"));
    auto h=CopySnapshotHistory();
    assert(h.size()==1 && h[0].stageCalls==2 && h[0].sequence==1);
    assert(h[0].observation==SnapshotObservation::Staged);
    assert(Matches(63,710,true));
    ObserveCompletion(63,390,true);
    assert(Store(63,390,{{24,710,-1}},"TX_SUBMIT"));
    h=CopySnapshotHistory();
    assert(h.size()==2 && h[1].sequence==2 && h[1].stageCalls==1);
    assert(h[0].observation==SnapshotObservation::CallbackSuccessObserved);
    assert(Store(63,515,{{22,711,-1}},"RX_LIVE"));
    assert(Store(63,390,{{24,710,-1}},"RX_LIVE")); // nonconsecutive stages must not merge
    h=CopySnapshotHistory();
    assert(h.size()==4 && h[3].sequence==4);
    ObserveCompletion(63,390,false);
    h=CopySnapshotHistory();
    assert(h[3].observation==SnapshotObservation::CallbackFailureObserved);
    assert(h[0].observation==SnapshotObservation::CallbackSuccessObserved);
    assert(Store(31,300,{{-1,901,-1}},"RX_DB"));
    assert(Store(31,300,{{-1,902,-1}},"RX_DB")); // distinct card must not merge
    h=CopySnapshotHistory();
    assert(h.size()==6);
    assert(Matches(31,902,true));
    ResetMatch();
    assert(CopySnapshotHistory().empty());
    return 0;
}
