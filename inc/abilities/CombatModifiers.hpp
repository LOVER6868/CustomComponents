#pragma once
#include "history/DomainRecords.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <string_view>
#include <BNM/ClassesManagement.hpp>
#include <BNM/Defaults.hpp>
#include "abilities/GrantableAbilityRuntime.hpp"
#include "core/CustomComponentRegistry.hpp"
#include "core/ComponentLifecycle.hpp"
#include "core/CustomFieldBridgeSystem.hpp"
#include "core/ManagedRuntime.hpp"
#include "core/HookRegistry.hpp"
#include "engine/EntityAccess.hpp"
#include "engine/Events.hpp"
#include "engine/EffectLifecycleEvents.hpp"
#include "core/WeakObjectSettings.hpp"
#include "engine/MatchAccess.hpp"
#include "log.h"

namespace CustomComponents::Abilities::CombatModifiers {
using Obj=BNM::IL2CPP::Il2CppObject;
using K=Systems::CustomFieldBridgeSystem::FieldKind;
// A component placed directly on a card (with empty $data) is innate +1.
// Runtime-created components start at 0 and use their inherited counters.
#define CC_MODIFIER_COMPONENT(Name) \
struct Name:Obj { \
    unsigned char inherited[8]{}; int InnateValue=0; \
    BNM_CustomClass(Name,BNM::CompileTimeClassBuilder("PvZCards.Engine.Components",#Name,"EngineLib.dll").Build(),BNM::CompileTimeClassBuilder("PvZCards.Engine.Components","GrantableAbility","EngineLib.dll").Build(),{}); \
    BNM_CustomField(InnateValue,BNM::Defaults::Get<int>(),"InnateValue"); \
    void DeepCopyFrom(Obj* from,Obj* instances){BNM_CallCustomMethodOrigin(DeepCopyFrom,this,from,instances);InnateValue=from?reinterpret_cast<Name*>(from)->InnateValue:0;} \
    void HandleReturnToPool(Obj* instances){InnateValue=0;BNM_CallCustomMethodOrigin(HandleReturnToPool,this,instances);} \
    BNM_CustomMethod(DeepCopyFrom,false,BNM::Defaults::Get<void>(),"DeepCopyFrom",BNM::Defaults::Get<Obj*>(),BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build()); \
    BNM_CustomMethod(HandleReturnToPool,false,BNM::Defaults::Get<void>(),"HandleReturnToPool",BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build()); \
};
CC_MODIFIER_COMPONENT(DamageBuff)
CC_MODIFIER_COMPONENT(NonCombatDamageBuff)
CC_MODIFIER_COMPONENT(NonCombatTruestrike)
CC_MODIFIER_COMPONENT(Burn)
CC_MODIFIER_COMPONENT(BuffMultiplier)
#undef CC_MODIFIER_COMPONENT

inline constexpr int DamageBuffId=28,NonCombatDamageBuffId=29,NonCombatTruestrikeId=30,BurnId=31,BuffMultiplierId=32;
inline Obj* Get(Obj* entity,const char* name,bool create=false){
    auto type=BNM::Class("PvZCards.Engine.Components",name);
    return type.IsValid()?Engine::Match::Component(entity,type.GetMonoType(),create):nullptr;
}
inline void* GetDamageBuff(void* e,bool c){return Get(static_cast<Obj*>(e),"DamageBuff",c);}
inline void* GetNonCombatDamageBuff(void* e,bool c){return Get(static_cast<Obj*>(e),"NonCombatDamageBuff",c);}
inline void* GetNonCombatTruestrike(void* e,bool c){return Get(static_cast<Obj*>(e),"NonCombatTruestrike",c);}
inline void* GetBurn(void* e,bool c){return Get(static_cast<Obj*>(e),"Burn",c);}
inline void* GetBuffMultiplier(void* e,bool c){return Get(static_cast<Obj*>(e),"BuffMultiplier",c);}
inline int Amount(Obj* card,const char* name){
    Obj* component=Get(card,name);
    if(!component)return 0;
    auto field=BNM::Class("PvZCards.Engine.Components",name).GetField("InnateValue").cast<int>();
    const int innate=field.IsValid()?field[component].Get():0;
    const int granted=GrantableAbilityRuntime::SumValues(component);
    const auto sum=std::int64_t(innate)+granted;
    return int(std::clamp(sum,std::int64_t(INT32_MIN),std::int64_t(INT32_MAX)));
}
inline bool Enabled(Obj* card,const char* name){
    auto* component=Get(card,name);
    return component && (Amount(card,name)>0 || GrantableAbilityRuntime::Active(component));
}
inline void Defaults(Obj* obj,Obj* properties){
    if(!obj || !obj->klass || !obj->klass->name)return;
    const auto name=std::string_view(obj->klass->name);
    for(const char* known:{"DamageBuff","NonCombatDamageBuff","NonCombatTruestrike","Burn","BuffMultiplier"}) {
        if(name!=known)continue;
        if(!Systems::CustomFieldBridgeSystem::DictionaryValue(properties,"InnateValue")){
            auto field=BNM::Class(obj).GetField("InnateValue").cast<int>();
            if(field.IsValid())field[obj].Set(1);
        }
        break;
    }
}
inline bool Register(){
    auto base=BNM::Class("PvZCards.Engine.Components","GrantableAbility");
    for(const char* name:{"DamageBuff","NonCombatDamageBuff","NonCombatTruestrike","Burn","BuffMultiplier"}){
        if(!CustomComponentRegistry::Register({name,"PvZCards.Engine.Components","EngineLib.dll","GrantableAbility","PvZCards.Engine.Components","EngineLib.dll"}))return false;
        auto cls=BNM::Class("PvZCards.Engine.Components",name);
        if(!Core::ComponentLifecycle::Patch(cls,base) ||
           !Systems::CustomFieldBridgeSystem::Register("PvZCards.Engine.Components",name,"PvZCards.Engine.Components","GrantableAbility",{{"InnateValue",K::Int32}},false))return false;
    }
    return Systems::CustomFieldBridgeSystem::beforeProperties.Add(Defaults);
}

// The effect entity must actually contain a DamageEffectDescriptor. An
// EffectEntityId alone isn't proof that this is noncombat damage.
inline bool NonCombat(Obj* effect,Obj* model){
    static auto effectEntity=BNM::Class("PvZCards.Engine","Effect").GetField("EffectEntityId").cast<int>();
    if(!effect || !model || !effectEntity.IsValid())return false;
    const int id=effectEntity[effect].Get();
    if(id<0)return false;
    Obj* definition=Engine::EntityAccess::Find(model,id);
    return definition && Engine::EntityAccess::Component(definition,"DamageEffectDescriptor");
}
inline bool TargetsHero(Obj* effect,Obj* model){
    static auto targetId=BNM::Class("PvZCards.Engine","Effect").GetField("TargetId").cast<int>();
    if(!effect || !model || !targetId.IsValid())return false;
    Obj* target=Engine::EntityAccess::Find(model,targetId[effect].Get());
    return target && Engine::EntityAccess::Component(target,"Hero");
}
inline Obj* EffectModel(Obj* system,const char* systemName,const char* backingName){
    auto field=BNM::Class("PvZCards.Core.Systems",systemName).GetField(backingName).cast<Obj*>();
    return system && field.IsValid()?field[system].Get():nullptr;
}
inline Obj* EffectEntity(Obj* system,Obj* effect,const char* systemName,const char* backingName,const char* idField){
    auto field=BNM::Class("PvZCards.Engine","Effect").GetField(idField).cast<int>();
    Obj* model=EffectModel(system,systemName,backingName);
    return model && effect && field.IsValid()?Engine::EntityAccess::Find(model,field[effect].Get()):nullptr;
}
inline int Saturate(std::int64_t amount){return int(std::clamp(amount,std::int64_t(INT32_MIN),std::int64_t(INT32_MAX)));}

inline bool useTruestrikeIcons=false;
struct EffectTag{int type;int value;};
inline bool TagTruestrike(Obj* effect){
    auto field=BNM::Class("PvZCards.Engine.Effects","DamageEffect").GetField("Tags").cast<Obj*>();
    Obj* tags=effect && field.IsValid()?field[effect].Get():nullptr;
    if(!tags)return false;
    auto has=BNM::Class("PvZCards.Engine","EffectTagCollection").GetMethod("ContainsTag",1).cast<bool>();
    auto add=BNM::Class(tags).GetMethod("Add",1).cast<void>();
    if(!has.IsValid() || !add.IsValid())return false;
    if(has[tags](5))return true; // EffectTagType.Truestrike
    bool ok=false;
    auto exception=BNM::TryInvoke([&]{add[tags](EffectTag{5,0});ok=true;});
    return ok && !exception.IsValid();
}
inline Core::WeakObjectSettings<bool> sourceBonusApplied;
inline void (*originalDamageMultiplier)(Obj*,Obj*,const void*)=nullptr;
inline void DamageMultiplier(Obj* self,Obj* effect,const void* method){
    auto field=BNM::Class("PvZCards.Core.Systems","SourceDamageMultiplierSystem").GetField("<MultiplierCalculator>k__BackingField").cast<Obj*>();
    Obj* calculator=self && field.IsValid()?field[self].Get():nullptr;
    auto modelField=BNM::Class("PvZCards.Core.Systems.Utilities","MultiplierCalculator").GetField("<Model>k__BackingField").cast<Obj*>();
    Obj* model=calculator && modelField.IsValid()?modelField[calculator].Get():nullptr;
    auto sourceId=BNM::Class("PvZCards.Engine","Effect").GetField("SourceId").cast<int>();
    Obj* source=model && effect && sourceId.IsValid()?Engine::EntityAccess::Find(model,sourceId[effect].Get()):nullptr;
    auto get=BNM::Class("PvZCards.Engine.Effects","DamageEffect").GetMethod("get_DamageAmount",0).cast<int>();
    auto set=BNM::Class("PvZCards.Engine.Effects","DamageEffect").GetMethod("set_DamageAmount",1).cast<void>();
    if(source && get.IsValid() && set.IsValid() && get[effect]()>0){
        const bool fromAbility=NonCombat(effect,model);
        const std::int64_t additive=std::int64_t(Amount(source,"DamageBuff"))+(fromAbility?Amount(source,"NonCombatDamageBuff"):0);
        if(!sourceBonusApplied.Get(effect)){
            const int beforeBonus=get[effect]();
            if(additive)set[effect](std::max(0,Saturate(std::int64_t(get[effect]())+additive)));
            sourceBonusApplied.Set(effect,true);
            if(beforeBonus!=get[effect]()){
                static auto target=BNM::Class("PvZCards.Engine","Effect").GetField("TargetId").cast<int>();
                History::Domain::Damage(model,"sourceBonus","adjusted",target.IsValid()?Core::Metadata::Bind(target,effect).Get():-1,beforeBonus,get[effect]());
            }
            if(Diagnostics::HistoryTrace::Allow(true))LOG_INFO("CustomHistory/DAMAGE_BONUS: effect=%p source=%d bonus=%d amount=%d",effect,sourceId[effect].Get(),int(additive),get[effect]());
        }
        // Truestrike is a hero-blocking rule. Keep the damage bonus for every
        // target, but attach the stock tag only when the effect targets a Hero.
        if(fromAbility && TargetsHero(effect,model) && Enabled(source,"NonCombatTruestrike"))
            TagTruestrike(effect);
    }
    originalDamageMultiplier(self,effect,method);
}
inline void (*originalArmor)(Obj*,Obj*,const void*)=nullptr;
inline void ApplyBurn(Obj* self,Obj* effect,const void* method){
    Obj* target=EffectEntity(self,effect,"ArmorSystem","<Entities>k__BackingField","TargetId");
    auto get=BNM::Class("PvZCards.Engine.Effects","DamageEffect").GetMethod("get_DamageAmount",0).cast<int>();
    auto set=BNM::Class("PvZCards.Engine.Effects","DamageEffect").GetMethod("set_DamageAmount",1).cast<void>();
    if(target && get.IsValid() && set.IsValid() && get[effect]()>0){
        const int bonus=Amount(target,"Burn");
        if(bonus>0){const int before=get[effect]();set[effect](Saturate(std::int64_t(before)+bonus));
            History::Domain::Damage(History::Writer::Model(target),"burn","adjusted",Engine::Match::Id(target),before,get[effect]());}
    }
    originalArmor(self,effect,method);
}
// Called by the existing BuffSystem.ProcessEffect owner before the stock
// handler. There is deliberately no second native BuffSystem detour.
inline void MultiplyBuff(const Engine::Events::Effect& context){
    Obj* effect=context.effect;
    static auto targetId=BNM::Class("PvZCards.Engine","Effect").GetField("TargetId").cast<int>();
    Obj* target=context.model && effect && targetId.IsValid()
        ? Engine::EntityAccess::Find(context.model,targetId[effect].Get()):nullptr;
    const int extra=target?Amount(target,"BuffMultiplier"):0;
    if(extra>0){
        auto type=BNM::Class("PvZCards.Engine.Effects","BuffEffect");
        auto getA=type.GetMethod("get_AttackDelta",0).cast<int>();auto setA=type.GetMethod("set_AttackDelta",1).cast<void>();
        auto getH=type.GetMethod("get_HealthDelta",0).cast<int>();auto setH=type.GetMethod("set_HealthDelta",1).cast<void>();
        const int beforeA=getA.IsValid()?getA[effect]():0,beforeH=getH.IsValid()?getH[effect]():0;
        const auto factor=std::int64_t(extra)+1;
        if(getA.IsValid() && setA.IsValid())setA[effect](Saturate(std::int64_t(getA[effect]())*factor));
        if(getH.IsValid() && setH.IsValid())setH[effect](Saturate(std::int64_t(getH[effect]())*factor));
        if(getA.IsValid())History::Domain::Damage(context.model,"buffMultiplierAttack","adjusted",Engine::Match::Id(target),beforeA,getA[effect]());
        if(getH.IsValid())History::Domain::Damage(context.model,"buffMultiplierHealth","adjusted",Engine::Match::Id(target),beforeH,getH[effect]());
    }
}
inline void (*originalRecordTruestrike)(Obj*,Obj*,Obj*,const void*)=nullptr;
inline void (*originalProcessHeroDamage)(Obj*,Obj*,Obj*,const void*)=nullptr;
inline BNM::Method<void> stockRecordTruestrike;
inline void ProcessHeroDamage(Obj* self,Obj* effect,Obj* target,const void* method){
    Obj* model=EffectModel(self,"HeroBlockingSystem","<EntityModel>k__BackingField");
    Obj* source=EffectEntity(self,effect,"HeroBlockingSystem","<EntityModel>k__BackingField","SourceId");
    if(TargetsHero(effect,model) && NonCombat(effect,model) && Enabled(source,"NonCombatTruestrike") && stockRecordTruestrike.IsValid()){
        TagTruestrike(effect);
        History::Domain::Damage(model,"nonCombatTruestrike","unblockable",Engine::Match::Id(target),0,0);
        LOG_INFO("CustomComponents/NonCombatTruestrike: stock unblockable branch source=%d target=%d",Engine::Match::Id(source),Engine::Match::Id(target));
        auto record=stockRecordTruestrike;
        record[self](effect,target);
        return;
    }
    originalProcessHeroDamage(self,effect,target,method);
}
inline void RecordTruestrike(Obj* self,Obj* effect,Obj* target,const void* method){
    Obj* model=EffectModel(self,"HeroBlockingSystem","<EntityModel>k__BackingField");
    Obj* source=EffectEntity(self,effect,"HeroBlockingSystem","<EntityModel>k__BackingField","SourceId");
    const bool hideVisuals=!useTruestrikeIcons && TargetsHero(effect,model) &&
        NonCombat(effect,model) && Enabled(source,"NonCombatTruestrike");
    // Keep the full damage history. Only strip our Truestrike marker from the
    // newly written record, after the block decision was already made.
    Obj* recorder=nullptr;Obj* history=nullptr;Obj* records=nullptr;
    int oldCount=-1;
    auto block=BNM::Class("PvZCards.Core.Systems","HeroBlockingSystem").GetField("<BlockRecorder>k__BackingField").cast<Obj*>();
    auto historyField=BNM::Class("PvZCards.Core.Systems.Utilities","BlockRecorder").GetField("History").cast<Obj*>();
    auto recordsField=BNM::Class("PvZCards.Engine","History").GetField("Records").cast<Obj*>();
    if(hideVisuals){
        recorder=self && block.IsValid()?block[self].Get():nullptr;
        history=recorder && historyField.IsValid()?historyField[recorder].Get():nullptr;
        records=history && recordsField.IsValid()?recordsField[history].Get():nullptr;
        auto count=records?BNM::Class(records).GetMethod("get_Count",0).cast<int>():BNM::Method<int>{};
        if(count.IsValid())oldCount=count[records]();
    }
    originalRecordTruestrike(self,effect,target,method);
    if(!hideVisuals || !records || oldCount<0)return;
    auto list=BNM::Class(records);
    auto count=list.GetMethod("get_Count",0).cast<int>();
    auto get=list.GetMethod("get_Item",1).cast<Obj*>();
    if(!count.IsValid() || !get.IsValid())return;
    auto tagsField=BNM::Class("PvZCards.Engine.Records","DamageRecord").GetField("Tags").cast<Obj*>();
    for(int i=oldCount;i<count[records]();++i){
        Obj* record=get[records](i);
        if(!record || !record->klass || std::string_view(record->klass->name)!="DamageRecord")continue;
        Obj* tags=tagsField.IsValid()?tagsField[record].Get():nullptr;
        if(!tags)continue;
        auto t=BNM::Class(tags);
        auto n=t.GetMethod("get_Count",0).cast<int>();
        auto item=t.GetMethod("get_Item",1).cast<EffectTag>();
        auto erase=t.GetMethod("RemoveAt",1).cast<void>();
        if(!n.IsValid() || !item.IsValid() || !erase.IsValid())continue;
        for(int j=n[tags]()-1;j>=0;--j)if(item[tags](j).type==5)erase[tags](j);
    }
}
inline bool InitializeHooks(bool showIcons){
    useTruestrikeIcons=showIcons;
    stockRecordTruestrike=BNM::Class("PvZCards.Core.Systems","HeroBlockingSystem").GetMethod("RecordTruestrike",2).cast<void>();
    if(!stockRecordTruestrike.IsValid() || !Core::Hooks::Install(
        BNM::Class("PvZCards.Core.Systems","HeroBlockingSystem").GetMethod("ProcessDamage",2),
        ProcessHeroDamage,originalProcessHeroDamage))return false;
    // These damage-related hooks own three different stock entry points.
    // BuffSystem is owned by DiscardedSystem and publishes beforeBuff instead.
    const bool damage=Core::Hooks::Install(
        BNM::Class("PvZCards.Core.Systems","SourceDamageMultiplierSystem").GetMethod("ProcessEffect",1),
        DamageMultiplier,originalDamageMultiplier);
    const bool armor=Core::Hooks::Install(
        BNM::Class("PvZCards.Core.Systems","ArmorSystem").GetMethod("ProcessEffect",1),
        ApplyBurn,originalArmor);
    const bool truestrike=Core::Hooks::Install(
        BNM::Class("PvZCards.Core.Systems","HeroBlockingSystem").GetMethod("RecordTruestrike",2),
        RecordTruestrike,originalRecordTruestrike);
    if(!damage || !armor || !truestrike){
        LOG_ERR("CustomComponents/CombatModifiers: damage hooks failed (source=%d armor=%d truestrike=%d)",
            int(damage),int(armor),int(truestrike));
        return false;
    }
    if(!Engine::Events::buffHookReady){
        LOG_ERR("CustomComponents/CombatModifiers: BuffSystem owner unavailable; cannot subscribe beforeBuff");
        return false;
    }
    Engine::EffectLifecycleEvents::copied.Add([](Obj* source,Obj* copy){sourceBonusApplied.Set(copy,sourceBonusApplied.Get(source));});
    Engine::EffectLifecycleEvents::damageInherited.Add([](Obj* source,Obj* copy){sourceBonusApplied.Set(copy,sourceBonusApplied.Get(source));});
    Engine::EffectLifecycleEvents::returned.Add([](Obj* effect){sourceBonusApplied.Set(effect,false);});
    const bool buff=Engine::Events::beforeBuff.Add(MultiplyBuff);
    if(!buff)LOG_ERR("CustomComponents/CombatModifiers: beforeBuff subscription failed");
    else LOG_INFO("CustomComponents/CombatModifiers: damage hooks and shared pre-buff handler ready");
    return buff;
}
}
