#pragma once
#include <BNM/Class.hpp>
#include <BNM/Method.hpp>
#include "log.h"

namespace CustomComponents::Core::NativeConstruction {
// BNM gives new classes an origin for virtual overrides only. Constructors
// are nonvirtual, so CallCustomMethodOrigin inside a new .ctor is a no-op.
// Use the declaring custom class, not the object's dynamic type: a further
// derived class must not make its base constructor recurse into itself.
template<class Custom> inline void Base(Custom* self) {
    // sprig-lint: allow init-phase function-local static is initialized on the first managed constructor call, after BNM creates the class
    static auto constructor=BNM::Class(Custom::BNMCustomClass.myClass)
        .GetParent().GetMethod(".ctor",0).template cast<void>();
    if(!self || !constructor.IsValid()){
        LOG_ERR("CustomComponents/Construction: native base constructor unavailable");
        return;
    }
    auto invoke=constructor;
    invoke[reinterpret_cast<BNM::IL2CPP::Il2CppObject*>(self)]();
}
}
