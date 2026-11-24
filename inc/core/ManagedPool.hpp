#pragma once
#include <memory>
#include <BNM/Class.hpp>
#include <BNM/Exceptions.hpp>
#include <BNM/Method.hpp>
#include "core/ManagedRuntime.hpp"
namespace CustomComponents::Core::ManagedPool {
using Obj=ManagedRuntime::Obj;
inline Obj* Acquire(Obj* instances,BNM::Class type) {
    static auto acquire=BNM::Class("PvZCards.Engine","InstanceManager").GetMethod("Acquire",2).cast<Obj*>();
    auto method=acquire;
    return instances && type.IsValid() && method.IsValid()
        ? method[instances](reinterpret_cast<Obj*>(type.GetMonoType()),true) : nullptr;
}
inline void Return(Obj* instances,Obj* object) {
    static auto giveBack=BNM::Class("PvZCards.Engine","InstanceManager").GetMethod("Return",1).cast<void>();
    auto method=giveBack;
    if(instances && object && method.IsValid()) method[instances](object);
}
// Model-side InstanceManager is separate from Entity.InstanceManager. Use
// the specific prediction or live model which owns the pooled query.
inline Obj* ModelInstances(Obj* model) {
    static auto field=BNM::Class("PvZCards.Engine","EntityModel")
        .GetField("InstanceManager").cast<Obj*>();
    return model && field.IsValid()?field[model].Get():nullptr;
}
// Scoped native ownership: return only after the last user of this root drops
// it. Keep both the pool and its loaned object rooted while the loan is active.
inline std::shared_ptr<ManagedRuntime::Root> AcquireScoped(Obj* instances,BNM::Class type) {
    if(!instances)return {};
    Obj* object=Acquire(instances,type);
    if(!object)return {};
    auto poolRoot=std::make_shared<ManagedRuntime::Root>(instances);
    auto* objectRoot=new ManagedRuntime::Root(object);
    if(!poolRoot->Get() || !objectRoot->Get()) {
        delete objectRoot;
        Return(instances,object);
        return {};
    }
    return std::shared_ptr<ManagedRuntime::Root>(objectRoot,
        [poolRoot](ManagedRuntime::Root* owned) {
            Obj* instance=poolRoot->Get();
            Obj* item=owned->Get();
            if(instance && item)BNM::TryInvoke([&]{Return(instance,item);});
            delete owned;
        });
}
}
