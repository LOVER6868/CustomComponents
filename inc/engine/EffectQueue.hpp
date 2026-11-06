#pragma once
#include <BNM/Class.hpp>
#include "core/HookRegistry.hpp"
#include "core/Observers.hpp"
namespace CustomComponents::Engine::EffectQueue {
using Obj=BNM::IL2CPP::Il2CppObject;
inline Core::Observers<Obj*,Obj*,int,Obj*> beforePush,afterPush;
inline void(*originalPush)(Obj*,Obj*,int,Obj*,const void*)=nullptr;
static void Push(Obj* queue,Obj* effect,int group,Obj* list,const void* method) {
    beforePush.Notify(queue,effect,group,list);
    originalPush(queue,effect,group,list,method);
    afterPush.Notify(queue,effect,group,list);
}
inline bool Initialize() {
    return Core::Hooks::Install(BNM::Class("PvZCards.Engine","EffectQueue").GetMethod("PushEffectToList",3),Push,originalPush);
}
}
