#include "pick3/PickedFromThreeSelection.hpp"
#include <cassert>
int main() {
 using namespace CustomComponents::Pick3::Picked;
 ResetMatch();
 assert(Store(28,253,{{-1,698}}));
 assert(Matches(28,698,true));
 assert(!MatchesLatestLiveCard(163,698));
 assert(!BindCreated(27,698,163));
 assert(!BindCreated(28,425,163));
 assert(BindCreated(28,698,163));
 assert(MatchesLatestLiveCard(163,698));
 assert(!MatchesLatestLiveCard(164,698));
 assert(BindCreated(28,698,163));
 assert(!BindCreated(28,698,164));
 assert(Store(28,293,{{-1,716}}));
 assert(!MatchesLatestLiveCard(163,698));
 assert(!MatchesLatestLiveCard(47,716));
 assert(BindCreated(28,716,159));
 assert(MatchesLatestLiveCard(159,716));
 assert(!MatchesLatestLiveCard(26,716));
 ResetMatch();
 assert(!MatchesLatestLiveCard(159,716));
}
