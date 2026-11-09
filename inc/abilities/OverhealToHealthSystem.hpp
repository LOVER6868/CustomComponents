#pragma once
#include <algorithm>
#include <string_view>
#include "abilities/OverhealToHealth.hpp"
#include "core/HookRegistry.hpp"
#include "core/ManagedPool.hpp"
#include "core/ManagedRuntime.hpp"
#include "engine/EntityAccess.hpp"
#include "engine/BoardPlacement.hpp"
#include "engine/MatchAccess.hpp"
#include "engine/SimulationChanges.hpp"
#include "log.h"

namespace CustomComponents::Abilities::OverhealToHealth {
using Obj=BNM::IL2CPP::Il2CppObject;
using HealFn=void(*)(Obj*,Obj*,const void*);
inline HealFn originalHeal=nullptr;

// A normal stock HealEffect enters MultiplierSystem BEFORE HealSystem. The
// amount on the effect is therefore the final requested amount. HealSystem
// clamps it and records only the amount actually restored.
inline bool EnqueueStockBuff(Obj* target,Obj* heal,int bonus){
    if(!target || !heal || bonus<=0)return false;
    Obj* instances=Engine::Match::Instances(target);
    auto effectType=BNM::Class("PvZCards.Engine","Effect");
    auto buffType=BNM::Class("PvZCards.Engine.Effects","BuffEffect");
    if(!instances || !effectType.IsValid() || !buffType.IsValid())return false;
    Obj* queue=Engine::Match::Provide(instances,BNM::Class("PvZCards.Engine","EffectCollection").GetMonoType());
    if(!queue)return false;
    auto add=BNM::Method<void>{};
    for(auto method:BNM::Class(queue).GetMethods()){
        auto* info=method.GetInfo();
        if(!info || std::string_view(info->name)!="AddAfterCurrentGroup" || info->parameters_count!=1)continue;
        auto* klass=BNM::Class(info->parameters[0]).GetClass();
        if(klass && klass->rank==1){add=method.cast<void>();break;}
    }
    if(!add.IsValid())return false;
    Obj* buff=Core::ManagedPool::Acquire(instances,buffType);
    if(!buff)return false;
    Core::ManagedRuntime::Root buffRoot(buff);
    // BuffEffect uses Effect.Data keys for its attack/health deltas. Calling
    // stock setters is essential for buff history, colors and triggers.
    auto health=buffType.GetMethod("set_HealthDelta",1).cast<void>();
    auto attack=buffType.GetMethod("set_AttackDelta",1).cast<void>();
    auto duration=buffType.GetField("BuffDuration").cast<int>();
    auto effectFields=effectType;
    if(!health.IsValid() || !attack.IsValid() || !duration.IsValid()){
        Core::ManagedPool::Return(instances,buff);return false;
    }
    auto* array=effectType.NewArray<Obj*>(1);
    Core::ManagedRuntime::Root arrayRoot(reinterpret_cast<Obj*>(array));
    if(!array || !arrayRoot.Get()){
        Core::ManagedPool::Return(instances,buff);return false;
    }
    bool queued=false;
    auto error=BNM::TryInvoke([&]{
        for(const char* member:{"PlayerId","SourceId","TargetId","SpringboardId","EffectEntityId","EffectGroupId","SortingIndex"}){
            auto f=effectFields.GetField(member).cast<int>();
            if(f.IsValid())f[buff].Set(f[heal].Get());
        }
        attack[buff](0);
        health[buff](bonus);
        duration[buff].Set(0); // Stock EffectDuration.Permanent
        if(Core::ManagedRuntime::writeBarrier)
            Core::ManagedRuntime::writeBarrier(array,reinterpret_cast<void**>(&array->m_Items[0]),buff);
        else array->m_Items[0]=buff;
        add[queue](array);
        queued=true;
    });
    if(error.IsValid() || !queued){
        Core::ManagedPool::Return(instances,buff);
        return false;
    }
    return true;
}

static void Heal(Obj* system,Obj* effect,const void* method){
    auto systemType=BNM::Class("PvZCards.Core.Systems","HealSystem");
    auto modelField=systemType.GetField("<Model>k__BackingField").cast<Obj*>();
    auto healType=BNM::Class("PvZCards.Engine.Effects","HealEffect");
    auto amount=healType.GetMethod("get_HealAmount",0).cast<int>();
    auto targetId=BNM::Class("PvZCards.Engine","Effect").GetField("TargetId").cast<int>();
    auto damage=BNM::Class("PvZCards.Engine.Components","Health").GetField("CurrentDamage").cast<int>();
    auto cancelled=BNM::Class("PvZCards.Engine","Effect").GetMethod("IsCancelled",0).cast<bool>();
    Obj* model=(system && modelField.IsValid())?modelField[system].Get():nullptr;
    Obj* target=(model && effect && targetId.IsValid())?Engine::EntityAccess::Find(model,targetId[effect].Get()):nullptr;
    Obj* health=target?Engine::EntityAccess::Component(target,"Health"):nullptr;
    const bool eligible=health && damage.IsValid() && amount.IsValid() &&
        Active(target) && !Engine::BoardPlacement::Active(target,"Unhealable") &&
        (!cancelled.IsValid() || !cancelled[effect]());
    const int requested=eligible?amount[effect]():0;
    const int before=eligible?std::max(0,damage[health].Get()):0;
    originalHeal(system,effect,method);
    if(!eligible || requested<=0 || (cancelled.IsValid() && cancelled[effect]()))return;
    Obj* afterTarget=Engine::EntityAccess::Find(model,targetId[effect].Get());
    if(!afterTarget || afterTarget!=target)return;
    Obj* afterHealth=Engine::EntityAccess::Component(afterTarget,"Health");
    if(!afterHealth)return;
    const int restored=std::clamp(before-std::max(0,damage[afterHealth].Get()),0,before);
    const int extra=requested-restored;
    if(extra>0){
        if(EnqueueStockBuff(afterTarget,effect,extra)){
            Engine::SimulationChanges::Changed(model);
            LOG_INFO("CustomComponents/OverhealToHealth: target=%d requested=%d restored=%d buff=%d",
                targetId[effect].Get(),requested,restored,extra);
        }else LOG_WARN("CustomComponents/OverhealToHealth: stock buff enqueue failed target=%d extra=%d",targetId[effect].Get(),extra);
    }
}
inline bool InitializeSystem(){
    return Core::Hooks::Install(BNM::Class("PvZCards.Core.Systems","HealSystem").GetMethod("ProcessEffect",1),Heal,originalHeal);
}
}
