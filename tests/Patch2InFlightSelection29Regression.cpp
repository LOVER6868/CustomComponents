// Host regression: clang++ -std=c++20 -Iinc -I<android_shim> tests/Patch2InFlightSelection29Regression.cpp -o /tmp/test29
#include <cassert>
#include <cstdarg>
#include "pick3/PickedFromThreeSelection.hpp"
extern "C" int __android_log_print(int,const char*,const char*,...) { return 0; }
using namespace CustomComponents::Pick3::Picked;
int main() {
    ResetMatch();
    int model=1, context=2, otherContext=3;
    assert(!InFlight::Matches(&context,122,695));
    // Exact source/effect/target accepted by stock, not placed in history.
    InFlight::Identity accepted{&model,&context,122,121,35,695,true};
    {
        InFlight::Scoped pending(accepted);
        assert(InFlight::Matches(&context,122,695));
        assert(!InFlight::Matches(&context,122,710));
        assert(!InFlight::Matches(&context,123,695));
        assert(!InFlight::Matches(&otherContext,122,695));
        assert(!InFlight::Matches(nullptr,122,695));
        assert(!Matches(122,695,true)); // no permanent legacy publication
        assert(CopySnapshotHistory().empty()); // no fabricated model commit
        { InFlight::Scoped nested({&model,&otherContext,200,190,99,710,true});
          assert(InFlight::Matches(&otherContext,200,710));
          assert(!InFlight::Matches(&context,122,695));
        }
        assert(InFlight::Matches(&context,122,695));
        { InFlight::Scoped unrelated({});
          assert(!InFlight::Matches(&context,122,695));
        }
        assert(InFlight::Matches(&context,122,695));
    }
    assert(!InFlight::Matches(&context,122,695));
    assert(CopySnapshotHistory().empty());
    assert(!Matches(122,695,true));
    // A later ordinary committed-selection staging remains independent.
    assert(Store(122,208,{{35,695,-1}},"TEST"));
    assert(Matches(122,695,true));
    ResetMatch();
    return 0;
}
