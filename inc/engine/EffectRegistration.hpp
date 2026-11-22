#pragma once
#include <BNM/Class.hpp>
#include "core/HookRegistry.hpp"
#include "core/Observers.hpp"
namespace CustomComponents::Engine::EffectRegistration {
using Obj=BNM::IL2CPP::Il2CppObject;
inline Core::Observers<Obj*,Obj*,Obj*,const void*> afterRegister;
inline void(*original)(void*,void*,void*,const void*)=nullptr;
inline void Alias(Obj* router,Obj* type,Obj* handler,const void* method){original(router,type,handler,method);}
static void Register(void* router,void* type,void* handler,const void* method) {
    original(router,type,handler,method);afterRegister.Notify(static_cast<Obj*>(router),static_cast<Obj*>(type),static_cast<Obj*>(handler),method);
}
inline bool Initialize(){return Core::Hooks::Install(BNM::Class("PvZCards.Engine","EffectRouter").GetMethod("RegisterForEffect",2),Register,original);}
}
