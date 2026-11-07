#pragma once
#include "core/HookRegistry.hpp"
#include "core/Observers.hpp"
#include <BNM/Class.hpp>
#include <BNM/Method.hpp>
#include <BNM/Field.hpp>
namespace CustomComponents::Engine::PlayTransport {
using Obj=BNM::IL2CPP::Il2CppObject;
inline Core::Observers<Obj*> beforeSubmit,beforeSerialize;
inline Core::Observers<Obj*,Obj*> afterSerialize;
inline Obj*(*decorate)(Obj*,Obj*)=nullptr;
inline Obj*(*originalSerialize)(Obj*,Obj*,const void*)=nullptr;
inline void(*originalRemote)(Obj*,Obj*,const void*)=nullptr;
static Obj* Serialize(Obj* self,Obj* play,const void* method){beforeSerialize.Notify(play);Obj* bytes=originalSerialize(self,play,method);if(decorate)bytes=decorate(play,bytes);afterSerialize.Notify(play,bytes);return bytes;}
static void Remote(Obj* self,Obj* play,const void* method){beforeSubmit.Notify(play);originalRemote(self,play,method);}
inline bool Initialize(){return Core::Hooks::Install(BNM::Class("PvZCards.Multiplayer.Networking","GameCommandSerializer").GetMethod("SerializePlay",1),Serialize,originalSerialize) && Core::Hooks::Install(BNM::Class("PvZCards.Multiplayer.Networking","RemotePlayService").GetMethod("Process",1),Remote,originalRemote);}
}
