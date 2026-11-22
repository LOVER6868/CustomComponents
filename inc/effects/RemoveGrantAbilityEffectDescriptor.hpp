#include "engine/AbilityConversion.hpp"
#include "core/ComponentLifecycle.hpp"
#pragma once
#include <BNM/ClassesManagement.hpp>
#include <BNM/Defaults.hpp>
#include "core/CustomComponentRegistry.hpp"
#include "core/CustomFieldBridgeSystem.hpp"
#include "core/ManagedPool.hpp"
namespace CustomComponents::Effects::AbilityRemoval {
using Obj=BNM::IL2CPP::Il2CppObject;using Str=BNM::Structures::Mono::String;
inline constexpr int Complete=0x6f000005;
struct Descriptor:Obj {
    unsigned char inherited[16]{};Str* Mode=nullptr;
    BNM_CustomClass(Descriptor,BNM::CompileTimeClassBuilder("PvZCards.Engine.Components","RemoveGrantAbilityEffectDescriptor","EngineLib.dll").Build(),BNM::CompileTimeClassBuilder("PvZCards.Engine.Components","GrantAbilityEffectDescriptor","EngineLib.dll").Build(),{});
    BNM_CustomField(Mode,BNM::Defaults::Get<Str*>(),"Mode");
    void DeepCopyFrom(Obj* source,Obj* instances){BNM_CallCustomMethodOrigin(DeepCopyFrom,this,source,instances);auto* mode=source?reinterpret_cast<Descriptor*>(source)->Mode:nullptr;if(Core::ManagedRuntime::writeBarrier)Core::ManagedRuntime::writeBarrier(this,reinterpret_cast<void**>(&Mode),mode);else Mode=mode;}
    void HandleReturnToPool(Obj* instances){Mode=nullptr;BNM_CallCustomMethodOrigin(HandleReturnToPool,this,instances);}
    BNM_CustomMethod(DeepCopyFrom,false,BNM::Defaults::Get<void>(),"DeepCopyFrom",BNM::Defaults::Get<Obj*>(),BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
    BNM_CustomMethod(HandleReturnToPool,false,BNM::Defaults::Get<void>(),"HandleReturnToPool",BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
    Obj* ToEffect(Obj* instances,Obj*) {
        const auto mode=Mode?Mode->str():"All";
        if(mode!="All" && mode!="SourceAndDuration"){LOG_ERR("CustomComponents/RemoveAbility: invalid Mode");return nullptr;}
        auto from=BNM::Class("PvZCards.Engine.Components","GrantAbilityEffectDescriptor");auto type=from.GetField("GrantableAbilityType").cast<int>();auto duration=from.GetField("Duration").cast<int>();
        auto runtime=BNM::Class("PvZCards.Engine.Effects","RemoveGrantAbilityEffect");Obj* effect=Core::ManagedPool::Acquire(instances,runtime);if(!effect)return nullptr;
        runtime.GetField("GrantableAbilityType").cast<int>()[effect].Set(type[static_cast<Obj*>(this)].Get());
        runtime.GetField("Duration").cast<int>()[effect].Set(mode=="All"?Complete:duration[static_cast<Obj*>(this)].Get());return effect;
    }
    BNM_CustomMethod(ToEffect,false,BNM::CompileTimeClassBuilder("PvZCards.Engine","Effect","EngineLib.dll").Build(),"ToEffect",BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build(),BNM::CompileTimeClassBuilder("PvZCards.Engine","Entity","EngineLib.dll").Build());
};
inline bool Convert(Obj* descriptor,Obj* instances,Obj* entity,Obj*& result){if(!descriptor || !descriptor->klass || std::string_view(descriptor->klass->name)!="RemoveGrantAbilityEffectDescriptor")return false;result=reinterpret_cast<Descriptor*>(descriptor)->ToEffect(instances,entity);return true;}
inline bool Initialize() {
    using K=Systems::CustomFieldBridgeSystem::FieldKind;
    return Core::ComponentLifecycle::Patch(BNM::Class("PvZCards.Engine.Components","RemoveGrantAbilityEffectDescriptor"),BNM::Class("PvZCards.Engine.Components","GrantAbilityEffectDescriptor")) && Engine::AbilityConversion::Add(Convert) && CustomComponentRegistry::Register({"RemoveGrantAbilityEffectDescriptor","PvZCards.Engine.Components","EngineLib.dll","GrantAbilityEffectDescriptor","PvZCards.Engine.Components","EngineLib.dll"}) &&
        Systems::CustomFieldBridgeSystem::Register("PvZCards.Engine.Components","RemoveGrantAbilityEffectDescriptor","PvZCards.Engine.Components","GrantAbilityEffectDescriptor",{{"Mode",K::String}},false);
}
}
