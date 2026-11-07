#pragma once
#include <BNM/ClassesManagement.hpp>
#include <BNM/Defaults.hpp>
#include "abilities/GrantableAbilityRuntime.hpp"
#include "abilities/GrantModesPolicy.hpp"
#include "core/CustomComponentRegistry.hpp"
#include "engine/MatchAccess.hpp"
namespace CustomComponents::Abilities::MarkerGrants {
using Obj=Engine::Match::Obj;
struct State:Obj {
    unsigned char inherited[8]{};
    bool Innate=false;
    bool Initialized=false;
    BNM_CustomClass(State,BNM::CompileTimeClassBuilder("PvZCards.Engine.Components","MarkerGrantState","EngineLib.dll").Build(),BNM::CompileTimeClassBuilder("PvZCards.Engine.Components","GrantableAbility","EngineLib.dll").Build(),{});
    BNM_CustomField(Innate,BNM::Defaults::Get<bool>(),"Innate");
    BNM_CustomField(Initialized,BNM::Defaults::Get<bool>(),"Initialized");
    void DeepCopyFrom(Obj* source,Obj* instances) {
        BNM_CallCustomMethodOrigin(DeepCopyFrom,this,source,instances);
        auto* state=reinterpret_cast<State*>(source);Innate=state && state->Innate;Initialized=state && state->Initialized;
    }
    BNM_CustomMethod(DeepCopyFrom,false,BNM::Defaults::Get<void>(),"DeepCopyFrom",BNM::Defaults::Get<Obj*>(),BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
    void HandleReturnToPool(Obj* instances) {
        Innate=false;Initialized=false;BNM_CallCustomMethodOrigin(HandleReturnToPool,this,instances);
    }
    BNM_CustomMethod(HandleReturnToPool,false,BNM::Defaults::Get<void>(),"HandleReturnToPool",BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
};
struct MultishotState:Obj {
    BNM_CustomClass(MultishotState,BNM::CompileTimeClassBuilder("PvZCards.Engine.Components","GrantedMultishot","EngineLib.dll").Build(),BNM::CompileTimeClassBuilder("PvZCards.Engine.Components","MarkerGrantState","EngineLib.dll").Build(),{});
};
struct AdjacentState:Obj {
    BNM_CustomClass(AdjacentState,BNM::CompileTimeClassBuilder("PvZCards.Engine.Components","GrantedAdjacentAttacks","EngineLib.dll").Build(),BNM::CompileTimeClassBuilder("PvZCards.Engine.Components","MarkerGrantState","EngineLib.dll").Build(),{});
};
inline Obj* Get(Obj* entity,const char* tracker,const char* marker,bool create) {
    static auto stateType=BNM::Class("PvZCards.Engine.Components","MarkerGrantState");
    auto type=BNM::Class("PvZCards.Engine.Components",tracker);
    Obj* component=Engine::Match::Component(entity,type.GetMonoType(),create);
    if(component && create) {
        auto initialized=stateType.GetField("Initialized").cast<bool>();auto innate=stateType.GetField("Innate").cast<bool>();
        if(!initialized[component].Get()){innate[component].Set(Engine::EntityAccess::Component(entity,marker)!=nullptr);initialized[component].Set(true);}
    }
    return component;
}
inline void* Multishot(void* entity,bool create){return Get(static_cast<Obj*>(entity),"GrantedMultishot","Multishot",create);}
inline void* Adjacent(void* entity,bool create){return Get(static_cast<Obj*>(entity),"GrantedAdjacentAttacks","AttacksOnlyInAdjacentLanes",create);}
inline bool Active(Obj* state) {
    if(!state)return false;auto field=BNM::Class("PvZCards.Engine.Components","MarkerGrantState").GetField("Innate").cast<bool>();
    return GrantModes::KeepMarker(field[state].Get(),GrantableAbilityRuntime::Active(state));
}
inline void Sync(Obj* entity,Obj* state,const char* marker) {
    if(!entity || !state)return;
    auto type=BNM::Class("PvZCards.Engine.Components",marker);
    if(Active(state))Engine::Match::Component(entity,type.GetMonoType(),true);
    else if(Engine::Match::Component(entity,type.GetMonoType())) {
        auto remove=BNM::Class("PvZCards.Engine","Entity").GetMethod("RemoveComponent",1).cast<Obj*>();
        remove[entity](type.GetMonoType());
    }
}
inline bool Initialize() {
    auto state=BNM::Class("PvZCards.Engine.Components","MarkerGrantState");auto base=BNM::Class("PvZCards.Engine.Components","GrantableAbility");
    for(const char* name:{"DeepCopyFrom","HandleReturnToPool"}) {
        const int count=std::string_view(name)=="DeepCopyFrom"?2:1;auto method=state.GetMethod(name,count);auto stock=base.GetMethod(name,count);
        auto iface=BNM::Class("PvZCards.Engine",count==2?"DeepCopyable":"Poolable");auto im=iface.GetMethod(name,count);
        auto* klass=state.GetClass();if(!klass || !method.IsValid() || !stock.IsValid() || !im.IsValid())return false;
        int slot=-1;for(int i=0;i<klass->interface_offsets_count;++i)if(klass->interfaceOffsets[i].interfaceType==iface.GetClass())slot=klass->interfaceOffsets[i].offset+im.GetInfo()->slot;
        const int concrete=stock.GetInfo()->slot;if(slot<0 || slot>=klass->vtable_count || concrete>=klass->vtable_count)return false;
        method.GetInfo()->slot=concrete;for(int index:{concrete,slot})klass->vtable[index]={method.GetInfo()->methodPointer,method.GetInfo()};
        // Derived tracker metadata may already have copied the inherited slots.
        for(const char* child:{"GrantedMultishot","GrantedAdjacentAttacks"}) {
            auto* c=BNM::Class("PvZCards.Engine.Components",child).GetClass();if(!c)return false;
            for(int index:{concrete,slot}){if(index>=c->vtable_count)return false;c->vtable[index]=klass->vtable[index];}
        }
    }
    for(const char* name:{"MarkerGrantState","GrantedMultishot","GrantedAdjacentAttacks"})
        if(!CustomComponentRegistry::Register({name,"PvZCards.Engine.Components","EngineLib.dll","GrantableAbility","PvZCards.Engine.Components","EngineLib.dll"}))return false;
    return true;
}
}
