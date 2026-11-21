#pragma once
#include <BNM/Class.hpp>
#include <BNM/Method.hpp>
namespace CustomComponents::Presentation::NativeAnimation {
using Obj=BNM::IL2CPP::Il2CppObject;
inline bool Call(Obj* animation,const char* name){if(!animation)return false;
    auto method=BNM::Class(animation).GetMethod(name,0).cast<void>();if(!method.IsValid())return false;method[animation]();return true;}
inline bool Complete(Obj* animation){if(!animation)return false;
    auto method=BNM::Class(animation).GetMethod("IsCompleted",0).cast<bool>();return method.IsValid() && method[animation]();}
}
