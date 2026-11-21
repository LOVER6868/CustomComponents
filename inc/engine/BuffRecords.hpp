#pragma once
#include "engine/BoardPlacement.hpp"
#include "engine/MatchAccess.hpp"
#include "core/HookRegistry.hpp"

namespace CustomComponents::Engine::BuffRecords {
using Obj=EntityAccess::Obj;
inline void(*originalRecord)(Obj*,Obj*,int,int,const void*)=nullptr;
static void Record(Obj* system,Obj* effect,int healthDelta,int attackDelta,const void* method){
    static auto targetField=BNM::Class("PvZCards.Engine","Effect").GetField("TargetId").cast<int>();
    static auto modelField=BNM::Class("PvZCards.Core.Systems","BuffSystem").GetField("<Entities>k__BackingField").cast<Obj*>();
    Obj* model=system && modelField.IsValid()?Core::Metadata::Bind(modelField,system).Get():nullptr;
    Obj* target=model && effect && targetField.IsValid()?EntityAccess::Find(model,Core::Metadata::Bind(targetField,effect).Get()):nullptr;
    // BuffRecord presentation expects a CardTarget; heroes have a different
    // view contract. Record their health change with native hero-safe history,
    // without executing another heal or changing gameplay trigger semantics.
    if(!target || !EntityAccess::Component(target,"Player")){
        originalRecord(system,effect,healthDelta,attackDelta,method);return;
    }
    Obj* instances=Match::Instances(target);
    static auto healClass=BNM::Class("PvZCards.Engine.Effects","HealEffect");
    static auto copy=BNM::Class("PvZCards.Engine","Effect").GetMethod("DeepCopyFrom",2).cast<void>();
    static auto amount=healClass.GetMethod("set_HealAmount",1).cast<void>();
    static auto record=BNM::Class("PvZCards.Core.Systems","HealSystem").GetMethod("RecordTargetHealed",2).cast<void>();
    auto service=instances?Match::Service(instances,BNM::Class("PvZCards.Core.Systems","HealSystem")):nullptr;
    if(!instances || !service || !copy.IsValid() || !amount.IsValid() || !record.IsValid()){
        LOG_ERR("CustomComponents/BuffRecords: hero health history unavailable");return;
    }
    Obj* heal=Core::ManagedPool::Acquire(instances,healClass);Core::ManagedRuntime::Root root(heal);
    if(!root.Get())return;
    Core::Metadata::Bind(copy,heal)(effect,instances);
    Core::Metadata::Bind(amount,heal)(healthDelta);
    Core::Metadata::Bind(record,service->Get())(heal,healthDelta);
    Core::ManagedPool::Return(instances,heal);
    LOG_INFO("CustomComponents/BuffRecords/HERO: target=%d healthDelta=%d attackDelta=%d",Match::Id(target),healthDelta,attackDelta);
}
inline bool Initialize(){return Core::Hooks::Install(BNM::Class("PvZCards.Core.Systems","BuffSystem").GetMethod("RecordBuff",3),Record,originalRecord);}
}
