// Host adapter test: fake GC handles, actual transaction/root/queue adapter.
#include <cassert>
#include <map>
#include <array>
#include <cstring>
#include "pick3/SelectionTransactionRuntime.hpp"
extern "C" int __android_log_print(int,const char*,const char*,...){return 0;}
namespace RT=CustomComponents::Pick3::TransactionRuntime;
namespace TX=CustomComponents::Pick3::Transaction;
namespace P=CustomComponents::Pick3::Picked;
using Obj=RT::Obj;
std::map<std::uint32_t,Obj*> handles;
std::uint32_t nextHandle=1;
std::uint32_t New(Obj* object,bool){auto h=nextHandle++;handles[h]=object;return h;}
Obj* Get(std::uint32_t h){auto i=handles.find(h);return i==handles.end()?nullptr:i->second;}
void Free(std::uint32_t h){assert(handles.erase(h)==1);}
struct Object {
    alignas(void*) std::array<std::uint8_t,128> bytes{};
    Obj* get(){return reinterpret_cast<Obj*>(bytes.data());}
    void setObject(std::size_t offset,Obj* value){std::memcpy(bytes.data()+offset,&value,sizeof(value));}
    void set(std::size_t offset,int value){std::memcpy(bytes.data()+offset,&value,sizeof(value));}
};
int produced=0;
void Accepted(Obj*,Obj*,TX::Ref value,TX::Context,int){
    assert(TX::LiveTarget(value,0)==31);++produced;
}
int main(){
    RT::GC::ConfigureGC(New,Get,Free);
    Object model1,model2,router1,router2,effect1,effect2;
    RT::RegisterRouter(router1.get(),model1.get());
    RT::RegisterRouter(router2.get(),model2.get());
    const auto m1=RT::ModelOf(model1.get()),m2=RT::ModelOf(model2.get());assert(m1 && m2 && m1!=m2);
    assert(RT::RouterModel(router1.get())==m1);
    std::uint64_t seq=0;assert(P::Store(46,237,{{-1,665,-1}},"A",&seq));
    assert(!RT::Capture(model1.get(),46,237,10,seq)); // staging alone proves no ancestry
    P::SelectionSnapshot selected;
    assert(P::CopyExactSnapshot(46,237,seq,selected,false)==P::SnapshotLookup::Found);
    TX::Make(m1,10,selected); // simulate the verified model-side acceptance adapter
    const auto value=RT::Capture(model1.get(),46,237,10,seq);assert(value);
    for(auto* effect:{&effect1,&effect2}) {
        effect->set(0x30,83);effect->set(0x3C,400);effect->set(0x24,10);effect->set(0x28,31);
    }
    {
        TX::Scoped scope(value,46);
        RT::AttachInsertedEffect(effect1.get());RT::AttachInsertedEffect(effect1.get());
        RT::AttachInsertedEffect(effect2.get());
    }
    assert(RT::effectRoots.size()==2); // same tuple, distinct physical effects
    {
        RT::ScopedRouter execution(router1.get(),effect1.get());assert(TX::active.value==value);
        { RT::ScopedRouter unrelated(router2.get(),effect2.get());assert(!TX::active.value); }
        assert(TX::active.value==value);
    }
    assert(!TX::active.value);
    // wrong model consumes and rejects its ticket rather than resurrecting it
    assert(!RT::TakeInsertedEffect(m1,effect2.get()).value);assert(RT::effectRoots.empty());
    Object data1,data2,copy1,copy2,eventContext;
    eventContext.set(0x18,46);eventContext.set(0x14,10);eventContext.set(0x10,31);eventContext.set(0x1C,-1);
    for(auto* data:{&data1,&data2,&copy1,&copy2}) {
        data->set(0x10,83);data->set(0x14,10);data->setObject(0x18,eventContext.get());
    }
    // Identical copied trigger contexts still have distinct provenance tickets.
    std::uint64_t seq2=0;assert(P::Store(47,238,{{42,700,-1}},"B",&seq2));
    assert(P::CopyExactSnapshot(47,238,seq2,selected,false)==P::SnapshotLookup::Found);
    TX::Make(m1,10,selected);
    const auto other=RT::Capture(model1.get(),47,238,10,seq2);
    RT::AttachTriggerData(data1.get(),value);RT::AttachTriggerData(data2.get(),other);
    RT::RebindTriggerData(m1,data1.get(),copy1.get());RT::RebindTriggerData(m1,data2.get(),copy2.get());
    assert(!RT::TakeTriggerData(m2,copy1.get(),83,10,{46,10,31,-1}));
    assert(RT::TakeTriggerData(m1,copy2.get(),83,10,{46,10,31,-1})==other);
    assert(RT::TakeTriggerData(m1,copy1.get(),83,10,{46,10,31,-1})==value);
    assert(RT::triggerRoots.empty());
    RT::AwaitCreation(value,{46,10,-1,-1},237);
    RT::onStockAccepted=Accepted;
    assert(P::BindCreatedExact(46,237,seq,665,31));
    assert(TX::BindProduced(value,665,31));
    RT::Produced(model2.get(),effect1.get(),46,237,seq);assert(produced==0);
    RT::Produced(model1.get(),effect1.get(),46,237,seq);assert(produced==1);
    RT::Produced(model1.get(),effect1.get(),46,237,seq);assert(produced==1);
    {RT::ScopedSubmission a({46,237,seq,{99}});
      {RT::ScopedSubmission b({59,393,2,{55}});assert(RT::submitted.effect==59);}
      assert(RT::submitted.effect==46);
    }
    assert(!RT::submitted.sequence);
    RT::Reset();assert(handles.empty());assert(!TX::Current(value));
    assert(!RT::RouterModel(router1.get()));
}
