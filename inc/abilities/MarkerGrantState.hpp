#pragma once
#include <cstdint>
#include <string_view>
#include <BNM/ClassesManagement.hpp>
#include <BNM/Defaults.hpp>
#include "abilities/GrantableAbilityRuntime.hpp"
#include "abilities/GrantModesPolicy.hpp"
#include "core/CustomComponentRegistry.hpp"
#include "core/ComponentLifecycle.hpp"
#include "engine/MatchAccess.hpp"

namespace CustomComponents::Abilities::MarkerGrants {
using Obj=Engine::Match::Obj;

// These are mod-owned bookkeeping components, each derived directly from the
// stock GrantableAbility lifecycle. No invented intermediate game class.
struct MultishotState:Obj {
    std::uint8_t inherited[8]{};
    bool Innate=false;
    bool Initialized=false;
    BNM_CustomClass(MultishotState,BNM::CompileTimeClassBuilder("PvZCards.Engine.Components","GrantedMultishot","EngineLib.dll").Build(),BNM::CompileTimeClassBuilder("PvZCards.Engine.Components","GrantableAbility","EngineLib.dll").Build(),{});
    BNM_CustomField(Innate,BNM::Defaults::Get<bool>(),"Innate");
    BNM_CustomField(Initialized,BNM::Defaults::Get<bool>(),"Initialized");
    void DeepCopyFrom(Obj* source,Obj* instances){BNM_CallCustomMethodOrigin(DeepCopyFrom,this,source,instances);auto* other=reinterpret_cast<MultishotState*>(source);Innate=other&&other->Innate;Initialized=other&&other->Initialized;}
    void HandleReturnToPool(Obj* instances){Innate=false;Initialized=false;BNM_CallCustomMethodOrigin(HandleReturnToPool,this,instances);}
    BNM_CustomMethod(DeepCopyFrom,false,BNM::Defaults::Get<void>(),"DeepCopyFrom",BNM::Defaults::Get<Obj*>(),BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
    BNM_CustomMethod(HandleReturnToPool,false,BNM::Defaults::Get<void>(),"HandleReturnToPool",BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
};
struct AdjacentState:Obj {
    std::uint8_t inherited[8]{};
    bool Innate=false;
    bool Initialized=false;
    BNM_CustomClass(AdjacentState,BNM::CompileTimeClassBuilder("PvZCards.Engine.Components","GrantedAdjacentAttacks","EngineLib.dll").Build(),BNM::CompileTimeClassBuilder("PvZCards.Engine.Components","GrantableAbility","EngineLib.dll").Build(),{});
    BNM_CustomField(Innate,BNM::Defaults::Get<bool>(),"Innate");
    BNM_CustomField(Initialized,BNM::Defaults::Get<bool>(),"Initialized");
    void DeepCopyFrom(Obj* source,Obj* instances){BNM_CallCustomMethodOrigin(DeepCopyFrom,this,source,instances);auto* other=reinterpret_cast<AdjacentState*>(source);Innate=other&&other->Innate;Initialized=other&&other->Initialized;}
    void HandleReturnToPool(Obj* instances){Innate=false;Initialized=false;BNM_CallCustomMethodOrigin(HandleReturnToPool,this,instances);}
    BNM_CustomMethod(DeepCopyFrom,false,BNM::Defaults::Get<void>(),"DeepCopyFrom",BNM::Defaults::Get<Obj*>(),BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
    BNM_CustomMethod(HandleReturnToPool,false,BNM::Defaults::Get<void>(),"HandleReturnToPool",BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
};

inline Obj* Get(Obj* entity,const char* tracker,const char* marker,bool create) {
    auto type=BNM::Class("PvZCards.Engine.Components",tracker);
    Obj* component=Engine::Match::Component(entity,type.GetMonoType(),create);
    if(component&&create){auto initialized=type.GetField("Initialized").cast<bool>();auto innate=type.GetField("Innate").cast<bool>();if(initialized.IsValid()&&innate.IsValid()&&!initialized[component].Get()){innate[component].Set(Engine::EntityAccess::Component(entity,marker)!=nullptr);initialized[component].Set(true);}}
    return component;
}
inline void* Multishot(void* entity,bool create){return Get(static_cast<Obj*>(entity),"GrantedMultishot","Multishot",create);}
inline void* Adjacent(void* entity,bool create){return Get(static_cast<Obj*>(entity),"GrantedAdjacentAttacks","AttacksOnlyInAdjacentLanes",create);}
inline bool Active(Obj* state){if(!state)return false;auto field=BNM::Class(state).GetField("Innate").cast<bool>();return field.IsValid()&&GrantModes::KeepMarker(field[state].Get(),GrantableAbilityRuntime::Active(state));}
inline void Sync(Obj* entity,Obj* state,const char* marker){if(!entity||!state)return;auto type=BNM::Class("PvZCards.Engine.Components",marker);if(Active(state))Engine::Match::Component(entity,type.GetMonoType(),true);else if(Engine::Match::Component(entity,type.GetMonoType())){auto remove=BNM::Class("PvZCards.Engine","Entity").GetMethod("RemoveComponent",1).cast<Obj*>();if(remove.IsValid())remove[entity](type.GetMonoType());}}
inline bool Initialize(){
    auto base=BNM::Class("PvZCards.Engine.Components","GrantableAbility");
    for(const char* name:{"GrantedMultishot","GrantedAdjacentAttacks"}){
        auto type=BNM::Class("PvZCards.Engine.Components",name);
        if(!type.IsValid()||!Core::ComponentLifecycle::Patch(type,base))return false;
        if(!CustomComponentRegistry::Register({name,"PvZCards.Engine.Components","EngineLib.dll","GrantableAbility","PvZCards.Engine.Components","EngineLib.dll"}))return false;
    }
    return true;
}
}
