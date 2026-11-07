#pragma once
#include "level/LevelWire.hpp"
#include "core/WeakObjectSettings.hpp"
#include "core/HookRegistry.hpp"
#include "engine/PlayTransport.hpp"
#include <BNM/BasicMonoStructures.hpp>
namespace CustomComponents::Level::Plays {
using Obj=BNM::IL2CPP::Il2CppObject;
inline Core::WeakObjectSettings<Wire::Choice> choices,effects;
inline Core::WeakObjectSettings<bool> frontendChoices;
inline void(*originalCopy)(Obj*,Obj*,Obj*,const void*)=nullptr;
inline void(*originalPool)(Obj*,Obj*,const void*)=nullptr;
inline Obj*(*originalClone)(Obj*,const void*)=nullptr;
inline Obj*(*originalCloneWithInstances)(Obj*,Obj*,const void*)=nullptr;
inline bool(*originalCompare)(Obj*,Obj*,const void*)=nullptr;
inline void(*originalEffectCopy)(Obj*,Obj*,Obj*,const void*)=nullptr;
inline void(*originalEffectPool)(Obj*,Obj*,const void*)=nullptr;
inline Obj*(*originalDeserialize)(Obj*,Obj*,Obj*,const void*)=nullptr;
inline Obj*(*originalDeserializeTyped)(Obj*,Obj*,Obj*,Obj*,const void*)=nullptr;
static void Copy(Obj* self,Obj* source,Obj* instances,const void* method){auto choice=choices.Get(source);auto local=frontendChoices.Get(source);originalCopy(self,source,instances,method);choices.Set(self,choice,choice.present);frontendChoices.Set(self,local,local);}
static void Pool(Obj* self,Obj* instances,const void* method){choices.Set(self,{},false);frontendChoices.Set(self,false,false);originalPool(self,instances,method);}
static Obj* Clone(Obj* self,const void* method){auto choice=choices.Get(self);auto local=frontendChoices.Get(self);Obj* result=originalClone(self,method);choices.Set(result,choice,choice.present);frontendChoices.Set(result,local,local);return result;}
static Obj* CloneWithInstances(Obj* self,Obj* instances,const void* method){auto choice=choices.Get(self);auto local=frontendChoices.Get(self);Obj* result=originalCloneWithInstances(self,instances,method);choices.Set(result,choice,choice.present);frontendChoices.Set(result,local,local);return result;}
static bool Compare(Obj* self,Obj* other,const void* method){auto a=choices.Get(self),b=choices.Get(other);return (!a.present || !b.present || (a.valid==b.valid && a.entry==b.entry)) && originalCompare(self,other,method);}
static void EffectCopy(Obj* self,Obj* source,Obj* instances,const void* method){auto choice=effects.Get(source);originalEffectCopy(self,source,instances,method);effects.Set(self,choice,choice.present);}
static void EffectPool(Obj* self,Obj* instances,const void* method){effects.Set(self,{},false);originalEffectPool(self,instances,method);}
inline void Receive(Obj* bytes,Obj* result) {
    if(!result || !result->klass || std::string_view(result->klass->name)!="GameCommandDto")return;
    auto play=BNM::Class(result).GetField("Play").cast<Obj*>();Obj* command=play[result].Get();if(!command)return;
    auto* array=reinterpret_cast<BNM::Structures::Mono::Array<std::uint8_t>*>(bytes);
    Wire::Choice choice{true,true,-1};if(array && array->capacity<=16*1024*1024){auto decoded=Wire::Decode(array->m_Items,array->capacity);if(decoded.present)choice=decoded;}
    choices.Set(command,choice);
    frontendChoices.Set(command,false,false);
    if(choice.entry>=0 || !choice.valid){static std::atomic<unsigned> received{0};if(received.fetch_add(1)<64)LOG_INFO("CustomComponents/Level/WIRE_RECEIVE: entry=%d valid=%d",choice.entry,int(choice.valid));}
}
static Obj* Deserialize(Obj* self,Obj* bytes,Obj* fill,const void* method){Obj* result=originalDeserialize(self,bytes,fill,method);Receive(bytes,result);return result;}
static Obj* DeserializeTyped(Obj* self,Obj* bytes,Obj* fill,Obj* type,const void* method){Obj* result=originalDeserializeTyped(self,bytes,fill,type,method);Receive(bytes,result);return result;}
inline Obj* Decorate(Obj* play,Obj* bytes) {
    frontendChoices.Set(play,false,false);
    auto choice=choices.Get(play);if(!choice.present || !choice.valid || !bytes)return bytes;
    auto* array=reinterpret_cast<BNM::Structures::Mono::Array<std::uint8_t>*>(bytes);if(array->capacity>16*1024*1024)return nullptr;
    Core::ManagedRuntime::Root sourceRoot(bytes);if(!sourceRoot.Get())return nullptr;
    auto extension=Wire::Encode(choice.entry);if(extension.empty())return nullptr;
    if(choice.entry>=0){static std::atomic<unsigned> sent{0};if(sent.fetch_add(1)<64)LOG_INFO("CustomComponents/Level/WIRE_SEND: entry=%d version=1",choice.entry);}
    auto* result=BNM::Class("System","Byte").NewArray<std::uint8_t>(array->capacity+extension.size());if(!result)return nullptr;
    std::copy_n(array->m_Items,array->capacity,result->m_Items);std::copy(extension.begin(),extension.end(),result->m_Items+array->capacity);return reinterpret_cast<Obj*>(result);
}
inline bool Initialize() {
    auto play=BNM::Class("PvZCards.Engine.Dtos","Play");auto effect=BNM::Class("PvZCards.Engine.Effects","PlayCardEffect");
    auto platform=BNM::Class("PvZCards.Matchmaking","ModelSerializationPlatform");auto generic=platform.GetMethod("Deserialize",2).GetGeneric({BNM::Class("System","Object").GetCompileTimeClass()});
    Engine::PlayTransport::decorate=Decorate;
    return Core::Hooks::Install(play.GetMethod("DeepCopyFrom",2),Copy,originalCopy) && Core::Hooks::Install(play.GetMethod("HandleReturnToPool",1),Pool,originalPool) &&
        Core::Hooks::Install(play.GetMethod("Clone",0),Clone,originalClone) && Core::Hooks::Install(play.GetMethod("Clone",1),CloneWithInstances,originalCloneWithInstances) && Core::Hooks::Install(play.GetMethod("CompareTo",1),Compare,originalCompare) &&
        Core::Hooks::Install(effect.GetMethod("DeepCopyFrom",2),EffectCopy,originalEffectCopy) && Core::Hooks::Install(effect.GetMethod("HandleReturnToPool",1),EffectPool,originalEffectPool) &&
        Core::Hooks::Install(generic,Deserialize,originalDeserialize) && Core::Hooks::Install(platform.GetMethod("Deserialize",3),DeserializeTyped,originalDeserializeTyped);
}
}
