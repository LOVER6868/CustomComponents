#pragma once
#include <BNM/Class.hpp>
#include <BNM/Method.hpp>
#include <BNM/Exceptions.hpp>
#include "core/ManagedReference.hpp"
#include "log.h"
namespace CustomComponents::Core::ManagedQuery {
using Obj=ManagedRuntime::Obj;
inline BNM::Method<Obj*> CopyMethod() {
    static auto method=BNM::Class("PvZCards.Engine","EntityQueryUtils").GetMethod("DeepCopy",
        {BNM::Class("PvZCards.Engine","EntityQuery").GetCompileTimeClass(),BNM::Class("PvZCards.Engine","InstanceManager").GetCompileTimeClass()}).cast<Obj*>();
    return method;
}
inline BNM::Method<Obj*> ReturnMethod() {
    static auto method=BNM::Class("PvZCards.Engine","EntityQueryUtils").GetMethod("ReturnToPool",
        {BNM::Class("PvZCards.Engine","EntityQuery").GetCompileTimeClass(),BNM::Class("PvZCards.Engine","InstanceManager").GetCompileTimeClass()}).cast<Obj*>();
    return method;
}
// The field must own its query. This is intentionally separate from primitive
// field serialization; copying a reference is not a managed-query deep copy.
inline bool CopyOwnedField(Obj* owner,BNM::Field<Obj*> field,Obj* source,Obj* instances) {
    if(!owner || !field.IsValid() || !instances || !CopyMethod().IsValid() || !ReturnMethod().IsValid()) return false;
    Obj* copied=nullptr;
    auto error=BNM::TryInvoke([&]{if(source)copied=CopyMethod()(source,instances);});
    if(error.IsValid() || (source && !copied)) return false;
    ManagedRuntime::Root lifetime(copied);
    Obj* previous=field[owner].Get();
    if(!ManagedRuntime::SetReference(owner,field,copied)) {
        if(copied) BNM::TryInvoke([&]{ReturnMethod()(copied,instances);});
        return false;
    }
    if(previous && previous!=copied) {
        error=BNM::TryInvoke([&]{ReturnMethod()(previous,instances);});
        if(error.IsValid()) LOG_ERR("CustomComponents/ManagedQuery: previous owned query could not return to its pool");
    }
    return true;
}
inline bool ClearOwnedField(Obj* owner,BNM::Field<Obj*> field,Obj* instances) {
    return CopyOwnedField(owner,field,nullptr,instances);
}
}

