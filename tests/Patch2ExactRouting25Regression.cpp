// Host-only: exact caller identity, ambiguity, in-flight, cancellation and
// independent/cross-effect query protection. No Unity runtime is used.
#include <cassert>
#include "pick3/Patch2ExactRouting.hpp"
extern "C" int __android_log_print(int,const char*,const char*,...) { return 0; }
using namespace CustomComponents::Pick3;
using namespace CustomComponents::Pick3::ExactRouting;
int main() {
    Picked::ResetMatch();
    auto* context = reinterpret_cast<Obj*>(0x1000);
    QueryCaller::active = {context,nullptr,59,1,1};
    bool match=false;
    assert(Picked::Store(59,406,{{30,710,-1}},"TX_SUBMIT"));
    // Staged selection is readable during a verified stock find.
    active = {59,406,1,1,false,true,"TEST",1};
    assert(Evaluate(context,59,710,false,match));
#if CUSTOMCOMPONENTS_PF3_EXACT_ROUTING
    assert(match);
#else
    assert(!match);
#endif
    // Selection B replaces the legacy current view, but A remains exact.
    Picked::ObserveCompletion(59,406,true);
    assert(Picked::Store(59,524,{{41,695,-1}},"RX_LIVE"));
    assert(!Picked::Matches(59,710,true));
    match=false;
    assert(Evaluate(context,59,710,false,match));
#if CUSTOMCOMPONENTS_PF3_EXACT_ROUTING
    assert(match);
#else
    assert(!match);
#endif
    // The later group has its own generation, not the previous pick.
    active={59,524,2,1,false,true,"TEST",1};
    assert(Evaluate(context,59,695,false,match));
#if CUSTOMCOMPONENTS_PF3_EXACT_ROUTING
    assert(match);
#else
    assert(!match);
#endif
    // A repeated generation in the exact group is refused.
    assert(Picked::Store(59,406,{{22,712,-1}},"TX_SUBMIT"));
    active={59,406,0,2,false,true,"TEST",1};
    match=true;
    assert(!Evaluate(context,59,712,true,match));
    assert(match);
    // The same effect ID alone, or a finder context mismatch, is not enough.
    active={59,524,2,1,false,true,"TEST",1};
    assert(!Evaluate(context,137,695,true,match));
    assert(!Evaluate(reinterpret_cast<Obj*>(0x2000),59,695,true,match));
    active={59,524,2,1,true,true,"TEST",1};
    assert(!Evaluate(context,59,695,true,match));
    // Cancelled/failed choices never become exact snapshots.
    assert(Picked::Store(31,18,{{-1,902,-1}},"RX_DB"));
    active={31,18,4,1,false,true,"TEST",1};
    QueryCaller::active={context,nullptr,31,1,1};
    assert(Evaluate(context,31,902,false,match));
#if CUSTOMCOMPONENTS_PF3_EXACT_ROUTING
    assert(match);
#else
    assert(!match);
#endif
    Picked::ObserveCompletion(31,18,false);
    match=true;
    assert(!Evaluate(context,31,902,true,match));
    assert(match);
    active={};
    assert(!Evaluate(context,31,902,true,match));
    // Independent EntityQuery is left out of exact GUID routing entirely.
    assert(!Picked::MatchesLatestLiveCard(22,712)); // Latest is GUID-only database selection: no live ID.
    Picked::ObserveStockModelBoundary(59,524,false,true,false);
    Picked::ObserveStockModelBoundary(59,524,true,true,false);
    const auto history=Picked::CopySnapshotHistory();
    assert(history.size()==4 && history[1].stockEnterObserved==1 && history[1].stockReturnObserved==1);
    assert(Picked::Store(67,21,{{55,765,-1}},"TX_SUBMIT"));
    assert(Picked::MatchesLatestLiveCard(55,765));
    assert(!Picked::MatchesLatestLiveCard(22,712));
    Picked::ResetMatch();
    assert(Picked::CopySnapshotHistory().empty());
    return 0;
}
