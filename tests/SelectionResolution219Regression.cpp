#include <cassert>
#include <initializer_list>
#include <cstring>
#include "pick3/SelectionResolutionPolicy.hpp"
int main() {
    using CustomComponents::Pick3::ResolveSelectionByPriority;
    using CustomComponents::Pick3::MatchesPhysicalSelection;
    assert(MatchesPhysicalSelection(79,84,79,84));
    assert(!MatchesPhysicalSelection(79,84,97,84)); // same definition, different physical copy
    assert(!MatchesPhysicalSelection(79,84,79,716)); // same ID in a different model
    assert(!MatchesPhysicalSelection(79,84,79,-1)); // lane/effect entity
    assert(!MatchesPhysicalSelection(-1,84,-1,84)); // database GUID is not a live card

    // Every owner, including a negative result, stops all later callbacks.
    for(int owner=0;owner<5;++owner) for(bool matched:{false,true}) {
        int calls[4]={};
        auto candidate=[&](int index,bool& out){++calls[index];out=matched;return index==owner;};
        auto result=ResolveSelectionByPriority(matched,
            [&](bool& v){return candidate(0,v);},[&](bool& v){return candidate(1,v);},
            [&](bool& v){return candidate(2,v);},[&](bool& v){return candidate(3,v);});
        assert(result.matched==matched);
        assert(result.transaction==(owner==0));
        for(int i=0;i<4;++i) assert(calls[i]==(i<=owner?1:0));
        if(owner==4) assert(std::strcmp(result.origin,"COMPATIBILITY")==0);
    }
    // A stale positive must not rescue a rejected exact transaction.
    auto never=[](bool&)->bool {assert(false);return false;};
    assert(!ResolveSelectionByPriority(true,[](bool& v){v=false;return true;},never,never,never).matched);
}
