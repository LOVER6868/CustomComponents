#pragma once
#include <BNM/Class.hpp>
#include "core/CandidateRules.hpp"
#include "core/HookRegistry.hpp"
namespace CustomComponents::Engine::DeckCreation {
using Obj=BNM::IL2CPP::Il2CppObject;
struct Context {Obj* system;Obj* effect;};
inline Core::CandidateRules<Context> preparation;
inline bool Prepare(Obj* system,Obj* effect){return Core::CandidateRules<Context>::Allows(preparation.Read(),{system,effect},0);}
struct DescriptorAdapter {
    BNM::Class type;
    Obj*(*convert)(Obj*,Obj*,Obj*);
    void(*copy)(Obj*,Obj*,Obj*);
    void(*release)(Obj*,Obj*);
};
inline Core::AtomicSnapshot<std::vector<DescriptorAdapter>> adapters{std::make_shared<const std::vector<DescriptorAdapter>>()};
inline std::mutex adapterLock;
inline bool Register(DescriptorAdapter adapter) {
    if(!adapter.type.IsValid() || !adapter.convert || !adapter.copy || !adapter.release)return false;
    std::lock_guard<std::mutex> guard(adapterLock);auto prior=adapters.load();
    for(const auto& entry:*prior)if(entry.type.GetClass()==adapter.type.GetClass())return entry.convert==adapter.convert && entry.copy==adapter.copy && entry.release==adapter.release;
    auto next=std::make_shared<std::vector<DescriptorAdapter>>(*prior);next->push_back(adapter);adapters.store(next);return true;
}
inline const DescriptorAdapter* Find(Obj* descriptor,const std::shared_ptr<const std::vector<DescriptorAdapter>>& snapshot) {
    const DescriptorAdapter* result=nullptr;
    for(const auto& entry:*snapshot)if(descriptor && BNM::IsA(descriptor,entry.type)) {
        if(result){LOG_ERR("CustomComponents/DeckCreation: conflicting descriptor adapters");return nullptr;}result=&entry;
    }
    return result;
}
inline bool Convert(Obj* descriptor,Obj* instances,Obj* effectEntity,Obj*& result) {
    auto snapshot=adapters.load();if(auto* entry=Find(descriptor,snapshot)){result=entry->convert(descriptor,instances,effectEntity);return true;}return false;
}
inline void(*originalCopy)(Obj*,Obj*,Obj*,const void*)=nullptr;
inline void(*originalPool)(Obj*,Obj*,const void*)=nullptr;
inline void(*originalCtor)(Obj*,const void*)=nullptr;
static void Copy(Obj* self,Obj* source,Obj* instances,const void* method) {
    originalCopy(self,source,instances,method);auto snapshot=adapters.load();if(auto* entry=Find(self,snapshot))entry->copy(self,source,instances);
}
static void Pool(Obj* self,Obj* instances,const void* method) {
    auto snapshot=adapters.load();if(auto* entry=Find(self,snapshot))entry->release(self,instances);originalPool(self,instances,method);
}
static void Construct(Obj* self,const void* method) {
    originalCtor(self,method);auto snapshot=adapters.load();if(!Find(self,snapshot))return;
    auto base=BNM::Class("PvZCards.Engine.Components","CreateCardInDeckEffectDescriptor");
    base.GetField("AmountToCreate").cast<int>()[self].Set(1);base.GetField("DeckPosition").cast<int>()[self].Set(0);
}
inline bool Initialize() {
    auto base=BNM::Class("PvZCards.Engine.Components","CreateCardInDeckEffectDescriptor");
    bool copy=Core::Hooks::Install(base.GetMethod("DeepCopyFrom",2),Copy,originalCopy);
    bool pool=Core::Hooks::Install(base.GetMethod("HandleReturnToPool",1),Pool,originalPool);
    bool ctor=Core::Hooks::Install(base.GetMethod(".ctor",0),Construct,originalCtor);
    return copy && pool && ctor;
}
}
