#pragma once
#include <BNM/ClassesManagement.hpp>
#include <BNM/Defaults.hpp>
#include "abilities/GrantableAbilityRuntime.hpp"
#include "core/ComponentLifecycle.hpp"
#include "core/CustomComponentRegistry.hpp"
#include "core/CustomFieldBridgeSystem.hpp"
#include "engine/MatchAccess.hpp"
namespace CustomComponents::Abilities::SelfTeleport {
using Obj=BNM::IL2CPP::Il2CppObject;
inline constexpr int Id=26;
struct Component:Obj {
    unsigned char inherited[8]{};bool Innate=false;
    BNM_CustomClass(Component,BNM::CompileTimeClassBuilder("PvZCards.Engine.Components","SelfTeleport","EngineLib.dll").Build(),BNM::CompileTimeClassBuilder("PvZCards.Engine.Components","GrantableAbility","EngineLib.dll").Build(),{});
    BNM_CustomField(Innate,BNM::Defaults::Get<bool>(),"Innate");
    void DeepCopyFrom(Obj* source,Obj* instances){BNM_CallCustomMethodOrigin(DeepCopyFrom,this,source,instances);Innate=source && reinterpret_cast<Component*>(source)->Innate;}
    void HandleReturnToPool(Obj* instances){Innate=false;BNM_CallCustomMethodOrigin(HandleReturnToPool,this,instances);}
    BNM_CustomMethod(DeepCopyFrom,false,BNM::Defaults::Get<void>(),"DeepCopyFrom",BNM::Defaults::Get<Obj*>(),BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
    BNM_CustomMethod(HandleReturnToPool,false,BNM::Defaults::Get<void>(),"HandleReturnToPool",BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
};
inline void* Get(void* entity,bool create){return Engine::Match::Component(static_cast<Obj*>(entity),BNM::Class("PvZCards.Engine.Components","SelfTeleport").GetMonoType(),create);}
inline bool Direct(Obj* card){
    auto* c=reinterpret_cast<Component*>(Get(card,false));if(!c)return false;if(c->Innate)return true;
    auto group=GrantableAbilityRuntime::countersField;auto entries=GrantableAbilityRuntime::entriesField;auto* counters=group[c].Get();auto* list=counters?static_cast<BNM::Structures::Mono::List<GrantableAbilityRuntime::CounterEntry>*>(entries[counters].Get()):nullptr;
    if(!list || !list->items || list->size<0 || list->size>int(list->items->capacity))return false;
    for(int i=0;i<list->size;++i)if(list->items->m_Items[i].sourceId>=0 && list->items->m_Items[i].value>0)return true;return false;
}
inline void Defaults(Obj* object,Obj* properties){if(!object || !object->klass || std::string_view(object->klass->name)!="SelfTeleport")return;if(!Systems::CustomFieldBridgeSystem::DictionaryValue(properties,"Innate"))reinterpret_cast<Component*>(object)->Innate=true;}
inline bool Initialize(){using K=Systems::CustomFieldBridgeSystem::FieldKind;
    return CustomComponentRegistry::Register({"SelfTeleport","PvZCards.Engine.Components","EngineLib.dll","GrantableAbility","PvZCards.Engine.Components","EngineLib.dll"}) && Core::ComponentLifecycle::Patch(BNM::Class("PvZCards.Engine.Components","SelfTeleport"),BNM::Class("PvZCards.Engine.Components","GrantableAbility")) && Systems::CustomFieldBridgeSystem::Register("PvZCards.Engine.Components","SelfTeleport","PvZCards.Engine.Components","GrantableAbility",{{"Innate",K::Bool}},false) && Systems::CustomFieldBridgeSystem::beforeProperties.Add(Defaults);
}
}
