#include "diagnostics/PvpInsertionOwnerConsensus.hpp"
#include <cassert>
using namespace CustomComponents::Diagnostics::PvpInsertionOwnerConsensus;
int main() {
    // Pool itself has no owner; three already physical copies establish it.
    const PhysicalCopy a{704,210,704,210,5,true,true};
    const PhysicalCopy b{140,211,140,211,5,true,true};
    const PhysicalCopy c{333,212,333,212,5,true,true};
    auto ok=Resolve({a,b,c});
    assert(ok.valid && ok.ownerId==5 && ok.checked==3);
    assert(!Resolve({}).valid);
    auto badOwner=c;badOwner.ownerId=0;
    assert(!Resolve({a,b,badOwner}).valid);
    auto badGuid=c;badGuid.cardGuid=999;
    assert(!Resolve({a,b,badGuid}).valid);
    auto badId=c;badId.cardEntityId=213;
    assert(!Resolve({a,b,badId}).valid);
    auto missingOwner=c;missingOwner.hasOwner=false;
    assert(!Resolve({a,b,missingOwner}).valid);
    auto virtualCopy=c;virtualCopy.poolEntityId=-1;
    assert(!Resolve({a,b,virtualCopy}).valid);
    // Correct owner identity is independent of faction or numeric pool ID.
    auto plant=Resolve({PhysicalCopy{51,300,51,300,0,true,true}});
    assert(plant.valid && plant.ownerId==0);
    return 0;
}
