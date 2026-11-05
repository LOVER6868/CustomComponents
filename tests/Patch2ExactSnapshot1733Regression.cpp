#include <cassert>
#include <cstdarg>
#include "pick3/PickedFromThreeSelection.hpp"
extern "C" int __android_log_print(int, const char*, const char*, ...) { return 0; }
using namespace CustomComponents::Pick3::Picked;
int main() {
    ResetMatch();
    bool matched = true;
    SelectionSnapshot found;
    assert(Store(59, 406, {{30,710,-1}}, "TX_SUBMIT"));
    assert(CopyExactSnapshot(59,406,1,found) == SnapshotLookup::NotConfirmed);
    assert(MatchesExactSnapshot(59,406,1,30,710,true,matched) == SnapshotLookup::NotConfirmed && !matched);
    ObserveCompletion(59,406,true);
    assert(CopyExactSnapshot(59,406,1,found) == SnapshotLookup::Found && found.selection.cards[0].cardGuid==710);
    assert(Store(59,524,{{41,695,-1}},"RX_LIVE"));
    ObserveCompletion(59,524,true);
    assert(MatchesExactSnapshot(59,406,1,30,710,true,matched) == SnapshotLookup::Found && matched);
    assert(MatchesExactSnapshot(59,524,2,30,710,true,matched) == SnapshotLookup::Found && !matched);
    assert(MatchesExactSnapshot(59,524,2,41,695,false,matched) == SnapshotLookup::Found && matched);
    assert(MatchesExactSnapshot(59,406,2,30,710,true,matched) == SnapshotLookup::InvalidIdentity && !matched);
    assert(Store(31,5,{{-1,902,-1}},"RX_DB"));
    ObserveCompletion(31,5,true);
    assert(MatchesExactSnapshot(31,5,3,-1,902,true,matched) == SnapshotLookup::Found && matched);
    assert(MatchesExactSnapshot(31,5,3,902,902,false,matched) == SnapshotLookup::Found && !matched);
    assert(Store(60,6,{{23,715,-1}},"TX_SUBMIT"));
    ObserveCompletion(60,6,false);
    assert(MatchesExactSnapshot(60,6,4,23,715,false,matched) == SnapshotLookup::Rejected && !matched);
    ResetMatch();
    assert(CopyExactSnapshot(59,406,1,found) == SnapshotLookup::Missing);
}
