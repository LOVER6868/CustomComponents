#pragma once
#include <BNM/Class.hpp>
#include <BNM/Method.hpp>
#include "core/ManagedRuntime.hpp"
namespace CustomComponents::Core::ManagedPool {
using Obj=ManagedRuntime::Obj;
inline Obj* Acquire(Obj* instances,BNM::Class type) {
    static auto acquire=BNM::Class("PvZCards.Engine","InstanceManager").GetMethod("Acquire",
        {BNM::Class("System","Type").GetCompileTimeClass(),BNM::Class("System","Boolean").GetCompileTimeClass()}).cast<Obj*>();
    auto method=acquire;
    return instances && type.IsValid() && method.IsValid()
        ? method[instances](reinterpret_cast<Obj*>(type.GetMonoType()),true) : nullptr;
}
inline void Return(Obj* instances,Obj* object) {
    static auto giveBack=BNM::Class("PvZCards.Engine","InstanceManager").GetMethod("Return",1).cast<void>();
    auto method=giveBack;
    if(instances && object && method.IsValid()) method[instances](object);
}
}
