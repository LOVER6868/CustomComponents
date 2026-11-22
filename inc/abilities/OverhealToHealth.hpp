#pragma once
#include <BNM/Class.hpp>
#include <BNM/ClassesManagement.hpp>
#include <BNM/Defaults.hpp>
#include <BNM/Method.hpp>
#include "abilities/GrantableAbilityRuntime.hpp"
#include "core/CustomComponentRegistry.hpp"
#include "core/ComponentLifecycle.hpp"
#include "core/CustomFieldBridgeSystem.hpp"
#include "engine/MatchAccess.hpp"

namespace CustomComponents::Abilities::OverhealToHealth {
using Obj=BNM::IL2CPP::Il2CppObject;
inline constexpr int Id=27;
struct Component:Obj {
    unsigned char inherited[8]{};
    bool Innate=false;
    BNM_CustomClass(Component,
        BNM::CompileTimeClassBuilder("PvZCards.Engine.Components","OverhealToHealth","EngineLib.dll").Build(),
        BNM::CompileTimeClassBuilder("PvZCards.Engine.Components","GrantableAbility","EngineLib.dll").Build(),{});
    BNM_CustomField(Innate,BNM::Defaults::Get<bool>(),"Innate");
    void DeepCopyFrom(Obj* from,Obj* instances){
        BNM_CallCustomMethodOrigin(DeepCopyFrom,this,from,instances);
        Innate=from && reinterpret_cast<Component*>(from)->Innate;
    }
    void HandleReturnToPool(Obj* instances){
        Innate=false; BNM_CallCustomMethodOrigin(HandleReturnToPool,this,instances);
    }
    BNM_CustomMethod(DeepCopyFrom,false,BNM::Defaults::Get<void>(),"DeepCopyFrom",BNM::Defaults::Get<Obj*>(),
        BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
    BNM_CustomMethod(HandleReturnToPool,false,BNM::Defaults::Get<void>(),"HandleReturnToPool",
        BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
};
inline void* Get(void* entity,bool create){
    return Engine::Match::Component(static_cast<Obj*>(entity),
        BNM::Class("PvZCards.Engine.Components","OverhealToHealth").GetMonoType(),create);
}
inline bool Active(Obj* entity){
    auto* component=reinterpret_cast<Component*>(Get(entity,false));
    return component && (component->Innate || GrantableAbilityRuntime::Active(component));
}
inline void Defaults(Obj* object,Obj* properties){
    if(!object || !object->klass || !object->klass->name ||
        std::string_view(object->klass->name)!="OverhealToHealth")return;
    if(!Systems::CustomFieldBridgeSystem::DictionaryValue(properties,"Innate"))
        reinterpret_cast<Component*>(object)->Innate=true;
}
inline bool Initialize(){
    using K=Systems::CustomFieldBridgeSystem::FieldKind;
    return CustomComponentRegistry::Register({"OverhealToHealth","PvZCards.Engine.Components","EngineLib.dll",
        "GrantableAbility","PvZCards.Engine.Components","EngineLib.dll"}) &&
        Core::ComponentLifecycle::Patch(BNM::Class("PvZCards.Engine.Components","OverhealToHealth"),
            BNM::Class("PvZCards.Engine.Components","GrantableAbility")) &&
        Systems::CustomFieldBridgeSystem::Register("PvZCards.Engine.Components","OverhealToHealth",
            "PvZCards.Engine.Components","GrantableAbility",{{"Innate",K::Bool}},false) &&
        Systems::CustomFieldBridgeSystem::beforeProperties.Add(Defaults);
}
}
