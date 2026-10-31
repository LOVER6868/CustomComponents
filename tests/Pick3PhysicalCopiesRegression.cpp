// Host: clang++ -std=c++20 -O2 -I../inc Pick3PhysicalCopiesRegression.cpp -o pick3_test
#include "pick3/PickFromThreeCore.hpp"
#include <cassert>
#include <iostream>
#include <random>
#include <set>
using namespace CustomComponents::Pick3;
int main() {
    std::vector<Candidate> deck;
    int id=1;
    for(int guid:{2,2,2,3,3,4,4,4,4,5,5,6,6,6,6}) deck.push_back({id++,guid});
    std::mt19937 rng(12345);
    // Every new effect sees the entire original deck, even after 100 offers.
    for (int n=0;n<100;++n) {
        const auto offer=SelectOptions(SelectionMode::UniqueGuidTargets,deck,rng);
        assert(offer.size()==3);
        std::set<int> ids,guids;
        for(const auto& c:offer) { assert(ids.insert(c.entityId).second); assert(guids.insert(c.cardGuid).second); }
        assert(deck.size()==15);
    }
    // One GUID has many tickets. It still cannot occur twice in one offer.
    auto offer=SelectOptions(SelectionMode::UniqueGuidTargets,
        {{1,140},{2,461},{3,461},{4,461}},rng);
    assert(offer.size()==2);
    assert(offer[0].cardGuid != offer[1].cardGuid);
    // Natural probability: 4 physical copies beat 1 in the first draw.
    int heavy=0;
    for(int i=0;i<10000;i++) {
        auto one=SelectOptions(SelectionMode::UniqueGuidTargets,
            {{1,10},{2,20},{3,20},{4,20},{5,20}},rng);
        if(one[0].cardGuid==20) ++heavy;
    }
    assert(heavy>7400 && heavy<8600);
    // The candidate list is never modified by the sampler.
    assert(deck.size()==15 && deck.front().cardGuid==2);
    std::cout << "PASS: physical-ticket weighting, per-offer GUID exclusion, no persistent depletion\n";
}
