#pragma once
#include <BNM/Field.hpp>
#include "core/ManagedRuntime.hpp"
namespace CustomComponents::Core::ManagedRuntime {
inline bool SetReference(Obj* object,BNM::Field<Obj*> field,Obj* value) {
    if(!object || !field.IsValid() || !writeBarrier) return false;
    auto* slot=field[object].GetPointer();
    if(!slot) return false;
    writeBarrier(object,reinterpret_cast<void**>(slot),value);
    return true;
}
}
