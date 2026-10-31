// Host: clang++ -std=c++20 -O2 -Iinc tests/Pick3SingleGuidAutoRegression.cpp -o single_guid_test
#include "pick3/PickFromThreeCore.hpp"
#include <cassert>
#include <random>
#include <set>
#include <vector>
#include <iostream>
using namespace CustomComponents::Pick3;
int main() {
    std::vector<Candidate> copies;
    for (int entity=100;entity<140;entity++) copies.push_back({entity,716});
    std::mt19937 rng(20261026);
    for (int trial=0;trial<500;trial++) {
        const auto offered=SelectOptions(SelectionMode::UniqueGuidTargets,copies,rng);
        assert(offered.size()==1 && offered[0].cardGuid==716);
        assert(offered[0].entityId>=100 && offered[0].entityId<140);
        PickAccumulator selected;
        assert(selected.Begin(static_cast<int>(offered.size()),1));
        assert(selected.Add(0));
        assert(selected.Seal());
        assert(selected.Indices().size()==1 && selected.Indices()[0]==0);
        assert(copies.size()==40); // Never remove or submit additional copies.
    }
    std::cout << "PASS: 40 copies collapse to exactly one GUID option / one index / one entity\n";
}
