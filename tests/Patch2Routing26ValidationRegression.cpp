// CustomComponents 2.6.1: positive detection, older-group reads and stock lifecycle.
#include <cassert>
#include "pick3/Patch2ExactRouting.hpp"
extern "C" int __android_log_print(int,const char*,const char*,...) { return 0; }
using namespace CustomComponents::Pick3;
using namespace CustomComponents::Pick3::ExactRouting;
int main() {
    Picked::ResetMatch();
    auto* ctx=reinterpret_cast<Obj*>(0x1000);
    QueryCaller::active={ctx,nullptr,59,1,1};
    assert(Picked::Store(59,406,{{30,710,-1}},"TX_SUBMIT"));
    Picked::ObserveStockModelBoundary(59,406,false,true,false);
    Picked::ObserveStockModelBoundary(59,406,true,true,false);
    Picked::ObserveCompletion(59,406,true);
    assert(Picked::Store(59,524,{{41,695,-1}},"RX_LIVE"));
    active=Identify(59,406,true,false,"TEST"); active.depth=1;
    bool out=false;
    // First group stays available after the newer selection replaced the
    // legacy current-view record. This is a shadow difference in default mode.
    assert(Evaluate(ctx,59,710,false,out));
#if CUSTOMCOMPONENTS_PF3_EXACT_ROUTING
    assert(out);
#else
    assert(!out);
#endif
    assert(olderReads.load()==1 && positiveMatches.load()==1 && differences.load()==1);
    assert(Evaluate(ctx,59,999,false,out));
    assert(olderReads.load()==2 && positiveMatches.load()==1);
    active=Identify(59,524,true,false,"TEST"); active.depth=1;
    assert(Evaluate(ctx,59,695,false,out));
    assert(positiveMatches.load()==2);
    Picked::ObserveCompletion(59,524,false);
    assert(!Evaluate(ctx,59,695,true,out));
    assert(lookupFailures.load()==1 && out); // failure retains legacy result
    active={}; QueryCaller::active={};
    Picked::ResetMatch();
}
