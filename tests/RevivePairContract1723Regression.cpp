#include "pick3/RevivePairContract1723.hpp"
#include <cassert>
#include <cstring>
using namespace CustomComponents::Pick3::RevivePairContract1723;
int main() {
    assert(Classify(238,238,10)==PrimaryOrigin::AlreadyChosen);
    assert(Classify(10,238,10)==PrimaryOrigin::VerifiedCast);
    assert(Classify(12,238,10)==PrimaryOrigin::Other);
    assert(Classify(-1,238,10)==PrimaryOrigin::Invalid);
    assert(std::strcmp(Name(PrimaryOrigin::AlreadyChosen),"ALREADY_CHOSEN")==0);
    const int candidates[]{100,238};
    int calls=0;
    const auto physical=[&](int card,int lane){ ++calls; return card==238 && lane==13; };
    const auto filter=[&](int card,int lane){ ++calls; return card==238 && lane==13; };
    assert(VerifyExact(112,238,13,candidates,2,physical,filter));
    assert(calls==2);
    calls=0;
    assert(!VerifyExact(112,239,13,candidates,2,physical,filter));
    assert(calls==0);
    assert(!VerifyExact(112,238,238,candidates,2,physical,filter));
    assert(calls==0);
    assert(!VerifyExact(112,238,13,nullptr,2,physical,filter));
    assert(calls==0);
    assert(!VerifyExact(112,238,13,candidates,257,physical,filter));
    assert(calls==0);
    const auto reject=[](int,int){ return false; };
    assert(!VerifyExact(112,238,13,candidates,2,physical,reject));
    assert(!VerifyExact(112,238,13,candidates,2,reject,filter));
}
