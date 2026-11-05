// Host-only: two query domains, live identity, verified queued ancestry, and rollback.
#include <cassert>
#include "pick3/Patch2ExactRouting.hpp"
extern "C" int __android_log_print(int,const char*,const char*,...) { return 0; }
using namespace CustomComponents::Pick3;
using namespace CustomComponents::Pick3::ExactRouting;
int main() {
    Picked::ResetMatch();
    auto* ctx=reinterpret_cast<Obj*>(0x1000);
    assert(Picked::Store(46,263,{{31,698,-1}},"TX_SUBMIT"));
    assert(Picked::Store(59,393,{{55,716,-1}},"TX_SUBMIT"));
    // Global latest cannot reliably identify the original card.
    assert(!Picked::MatchesLatestLiveCard(31,698));
    QueryCaller::active={ctx,nullptr,46,1,1};
    active={46,263,1,1,false,true,"TEST",1};
    bool out=false;
    assert(EvaluateEntity(ctx,46,31,698,false,out));
#if CUSTOMCOMPONENTS_PF3_EXACT_ROUTING
    assert(out);
#else
    assert(!out);
#endif
    out=true;
    assert(EvaluateEntity(ctx,46,55,716,true,out));
#if CUSTOMCOMPONENTS_PF3_EXACT_ROUTING
    assert(!out);
#else
    assert(out);
#endif
    // Same GUID as selected but different live entity: not a match.
    out=false;
    assert(EvaluateEntity(ctx,46,99,698,false,out));
    assert(!out);
    // DB GUID alone must not match a live entity in the entity domain.
    assert(Picked::Store(71,401,{{-1,903,-1}},"RX_DB"));
    QueryCaller::active={ctx,nullptr,71,1,2};
    active={71,401,3,1,false,true,"TEST",1};
    assert(EvaluateEntity(ctx,71,99,903,false,out));
    assert(!out);
    // Bound database creation may be matched by actual produced entity.
    assert(Picked::BindCreated(71,903,142));
    assert(EvaluateEntity(ctx,71,142,903,false,out));
#if CUSTOMCOMPONENTS_PF3_EXACT_ROUTING
    assert(out);
#else
    assert(!out);
#endif
    // No finder scope or unrelated context: fail closed.
    active={};out=false;
    assert(!EvaluateEntity(ctx,46,31,698,false,out));
    // Transient cross-effect linkage, including expiry and ambiguity.
    assert(Picked::DeferredLink::Enqueue(83,10,31,698,46,263));
    assert(!Picked::DeferredLink::MatchesEntity(83,31));
    {
      Picked::DeferredLink::ScopedExecution scope(83,10,31);
      assert(scope.bound);
      assert(Picked::DeferredLink::MatchesEntity(83,31));
      bool linked=false;
      assert(Picked::DeferredLink::Resolve(83,31,false,linked) && linked);
      assert(Picked::DeferredLink::Resolve(83,55,false,linked) && !linked);
      assert(Picked::DeferredLink::Resolve(83,698,true,linked) && linked);
      assert(Picked::DeferredLink::Resolve(83,716,true,linked) && !linked);
      assert(!Picked::DeferredLink::MatchesEntity(83,55));
      assert(!Picked::DeferredLink::MatchesEntity(84,31));
    }
    assert(!Picked::DeferredLink::MatchesEntity(83,31));
    assert(Picked::DeferredLink::Enqueue(84,10,31,698,46,263));
    assert(Picked::DeferredLink::Enqueue(84,10,31,698,46,263));
    { Picked::DeferredLink::ScopedExecution ambiguous(84,10,31);assert(!ambiguous.bound); }
    Picked::ResetMatch();
    assert(Picked::DeferredLink::pending.empty());
    return 0;
}
