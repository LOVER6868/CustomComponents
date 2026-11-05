#include <cassert>
#include "pick3/SelectionTransaction.hpp"
extern "C" int __android_log_print(int,const char*,const char*,...){return 0;}
namespace TX=CustomComponents::Pick3::Transaction;
namespace P=CustomComponents::Pick3::Picked;
int main(){
    P::ResetMatch();TX::Reset();
    std::uint64_t a=0,b=0;
    assert(P::Store(46,237,{{-1,665,-1}},"A",&a));
    P::SelectionSnapshot sa;assert(P::CopyExactSnapshot(46,237,a,sa,false)==P::SnapshotLookup::Found);
    const auto A=TX::Make(1,10,sa);assert(A);
    assert(P::Store(59,393,{{55,716,-1}},"B",&b));
    P::SelectionSnapshot sb;assert(P::CopyExactSnapshot(59,393,b,sb,false)==P::SnapshotLookup::Found);
    const auto B=TX::Make(1,11,sb);
    assert(TX::ClaimEvent(A));assert(!TX::ClaimEvent(A));assert(TX::ClaimEvent(B));
    assert(TX::ClaimEvent(TX::Make(2,10,sa))); // distinct simulation models
    assert(P::BindCreatedExact(46,237,a,665,31));
    assert(TX::BindProduced(A,665,31));
    const auto prediction=TX::Make(2,10,sa);
    assert(TX::BindProduced(prediction,665,99));
    assert(TX::LiveTarget(A,0)==31 && TX::LiveTarget(prediction,0)==99);
    assert(!P::BindCreatedExact(46,237,a,665,32)); // no same-GUID replacement
    TX::Context ca{46,10,31,-1},cb{59,11,55,-1};
    assert(TX::AttachListener(A,83,10,ca));
    assert(TX::AttachListener(B,84,11,cb));
    assert(!TX::TakeListener(2,83,10,ca));
    auto bound=TX::TakeListener(1,83,10,ca);assert(bound==A);
    bool out=true;
    {
        TX::Scoped scope(bound,83,46);
#if CUSTOMCOMPONENTS_PF3_TRANSACTION_ROUTING
        assert(TX::Resolve(1,46,31,false,out) && out);
        assert(TX::Resolve(1,83,55,false,out) && !out); // override latest B
        assert(TX::Resolve(1,83,665,true,out) && out);
        assert(TX::Resolve(1,83,32,false,out) && !out); // same GUID, other copy
#else
        assert(!TX::Resolve(1,83,31,false,out));
#endif
        assert(!TX::Resolve(2,83,31,false,out));
        assert(!TX::Resolve(1,999,31,false,out));
        TX::EffectKey descendant{1,90,400,10,31};
        assert(TX::AttachEffect(descendant,A));assert(TX::AttachEffect(descendant,A));
        {TX::Scoped nested(B,84);
#if CUSTOMCOMPONENTS_PF3_TRANSACTION_ROUTING
          assert(TX::Resolve(1,84,55,false,out) && out);
#endif
        }
        assert(TX::active.value==A);
        auto child=TX::TakeEffect(descendant);assert(child==A);
        assert(!TX::TakeEffect(descendant));
    }
    assert(!TX::Resolve(1,83,31,false,out));
    // Repeated same-key generations must remain independent.
    P::ObserveCompletion(46,237,true);
    std::uint64_t again=0;assert(P::Store(46,237,{{-1,665,-1}},"A2",&again));assert(again!=a);
    assert(P::BindCreatedExact(46,237,again,665,99));
    {TX::Scoped old(A,83);
#if CUSTOMCOMPONENTS_PF3_TRANSACTION_ROUTING
      assert(TX::Resolve(1,83,31,false,out) && out);
      assert(TX::Resolve(1,83,99,false,out) && !out);
#endif
    }
    // Ambiguous copied contexts must not guess registration order.
    assert(TX::AttachListener(A,85,10,ca));assert(TX::AttachListener(B,85,10,ca));
    assert(!TX::TakeListener(1,85,10,ca));assert(!TX::TakeListener(1,85,10,ca));
    TX::EffectKey collision{1,91,401,10,31};
    assert(TX::AttachEffect(collision,A));assert(TX::AttachEffect(collision,B));
    assert(!TX::TakeEffect(collision));
    // Failure/cancellation cannot produce a new transaction.
    P::Erase(46,237);
    P::SelectionSnapshot failed;assert(P::CopyExactSnapshot(46,237,again,failed,false)==P::SnapshotLookup::Rejected);
    failed=sa;failed.observation=P::SnapshotObservation::CallbackFailureObserved;
    assert(!TX::Make(1,10,failed));
    {TX::Scoped stale(A,83);TX::Reset();assert(!TX::Resolve(1,83,31,false,out));}
    assert(!TX::ClaimEvent(A));assert(TX::listeners.empty() && TX::queued.empty());
}
