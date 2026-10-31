#include <cstdio>
#include "diagnostics/PvpDeckPrepopulatePlan.hpp"
#include <cassert>
using CustomComponents::Diagnostics::PvpDeckPrepopulatePlan::Builder;
using CustomComponents::Diagnostics::PvpDeckPrepopulatePlan::AcceptFingerprint;

static Builder Build(bool differentGuids=false) {
    Builder p;
    constexpr int ids[4]={1,2,6,7};
    constexpr int sizes[4]={40,4,40,4};
    for(int n=0;n<4;n++) {
        assert(p.BeginPool(ids[n],sizes[n]));
        for(int i=0;i<sizes[n];i++)
            assert(p.AddEntry((differentGuids && n==2 && i==9)?999:51+i%7,-1));
    }
    return p;
}
int main() {
    auto x=Build(); auto y=Build();
    assert(x.Complete() && y.Complete() && x.fingerprint==y.fingerprint);
    auto z=Build(true);
    assert(z.Complete() && z.fingerprint!=x.fingerprint);
    assert(AcceptFingerprint("auto", x.fingerprint));
    assert(AcceptFingerprint("auto", z.fingerprint));
    // An exact pin is still a strict allowlist, not a new permissive mode.
    char pinned[17]{};
    std::snprintf(pinned,sizeof(pinned),"%016llx",static_cast<unsigned long long>(x.fingerprint));
    assert(AcceptFingerprint(pinned,x.fingerprint));
    assert(!AcceptFingerprint(pinned,z.fingerprint));
    assert(!AcceptFingerprint("",x.fingerprint));
    assert(!AcceptFingerprint("not-a-valid-hash",x.fingerprint));
    Builder wrongCount; assert(!wrongCount.BeginPool(1,39));
    Builder wrongId; assert(!wrongId.BeginPool(6,40));
    Builder stale; assert(stale.BeginPool(1,40)); assert(!stale.AddEntry(51,25));
    Builder invalid; assert(invalid.BeginPool(1,40)); assert(!invalid.AddEntry(0,-1));
    Builder incomplete; assert(incomplete.BeginPool(1,40)); assert(incomplete.AddEntry(51,-1)); assert(!incomplete.Complete());
    Builder overrun; assert(overrun.BeginPool(1,40)); for(int i=0;i<40;i++) assert(overrun.AddEntry(7,-1));
    assert(!overrun.AddEntry(7,-1));
}
