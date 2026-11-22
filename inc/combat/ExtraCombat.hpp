#include "core/NativeConstruction.hpp"
#pragma once
#include "history/DomainRecords.hpp"
#include "combat/EffectBatch.hpp"

// Extra-combat engine for selected fighter pairs.
// Uses stock damage generation and effect queues.
// Relies on stock damage generation / effect queue, not direct Health modification.

#include <algorithm>
#include <charconv>
#include <map>
#include <cstdint>
#include <mutex>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <BNM/ClassesManagement.hpp>
#include <BNM/Class.hpp>
#include <BNM/Defaults.hpp>
#include <BNM/Field.hpp>
#include <BNM/Method.hpp>
#include <BNM/BasicMonoStructures.hpp>

#include "core/CustomComponentRegistry.hpp"
#include "core/ComponentLifecycle.hpp"
#include "core/CustomFieldBridgeSystem.hpp"
#include "core/ManagedPool.hpp"
#include "core/ManagedRuntime.hpp"
#include "core/ModelRegistry.hpp"
#include "core/SceneViewContext.hpp"
#include "engine/AbilityConversion.hpp"
#include "engine/BoardPlacement.hpp"
#include "engine/EffectGuards.hpp"
#include "engine/EffectRegistration.hpp"
#include "engine/EntityAccess.hpp"
#include "engine/FighterPair.hpp"
#include "engine/MatchAccess.hpp"
#include "engine/RouterContext.hpp"
#include "engine/SimulationChanges.hpp"
#include "engine/ContinuousEffects.hpp"
#include "engine/EffectContinuations.hpp"
#include "engine/EffectCreationEvents.hpp"
#include "abilities/LaneAttacks.hpp"
#include "presentation/CombatFacingPresentation.hpp"
#include "triggers/CustomTriggerSystem.hpp"
#include "log.h"

namespace CustomComponents::ExtraCombat {
using Obj = BNM::IL2CPP::Il2CppObject;
using ObjList = BNM::Structures::Mono::List<Obj*>;

// Cache immutable metadata, then bind a local handle for each access. The
// staged pipeline reads these fields many times on both simulation threads.
inline BNM::Field<int> IntField(bool carrier,const char* name){
    using Entry=std::pair<std::string_view,BNM::Field<int>>;
    static const auto base=[] {
        std::vector<Entry> fields;auto type=BNM::Class("PvZCards.Engine","Effect");
        for(auto name:{"PlayerId","SourceId","TargetId","SecondaryTargetId","EffectEntityId","SpringboardId","EffectGroupId","SortingIndex"})fields.emplace_back(name,type.GetField(name).cast<int>());
        return fields;
    }();
    static const auto extra=[] {
        std::vector<Entry> fields;auto type=BNM::Class("PvZCards.Engine.Effects","ExtraCombatStepEffect");
        for(auto name:{"Stage","First","Second","FirstLane","SecondLane","OriginalSource","OriginalEffectEntity","LaneMode","DeathsRequested"})fields.emplace_back(name,type.GetField(name).cast<int>());
        return fields;
    }();
    for(const auto& entry:carrier?extra:base)if(entry.first==name)return entry.second;
    return {};
}

// This marker can appear in normal effect-entity JSON, like any stock trigger.
struct ExtraCombatTrigger : Obj {
    BNM_CustomClass(ExtraCombatTrigger,
        BNM::CompileTimeClassBuilder("PvZCards.Engine.Components", "ExtraCombatTrigger", "EngineLib.dll").Build(),
        BNM::CompileTimeClassBuilder("PvZCards.Engine.Components", "MarkerComponent", "EngineLib.dll").Build(),
        {},
        BNM::CompileTimeClassBuilder("PvZCards.Engine", "Trigger", "EngineLib.dll").Build());
};

// Managed data is essential: these fields survive stock pool copies, prediction,
// and queued effects. No native map keyed by managed object pointers.
struct CombatStepEffect : Obj {
    std::uint8_t inherited[0x38]{};
    // These members declare the managed fields for BNM_CustomField only.
    // Never access them directly: StateCheckEffect is a managed base class,
    // so its IL2CPP layout does not match this C++ declaration.
    int Stage = 0;
    int First = -1;
    int Second = -1;
    int FirstLane = -1;
    int SecondLane = -1;
    int OriginalSource = -1;
    int OriginalEffectEntity = -1;
    int LaneMode = 0;
    int DeathsRequested=0;
    std::uint64_t HistoryId=0;
    BNM::Structures::Mono::String* RemainingTargets = nullptr;

    BNM_CustomClass(CombatStepEffect,
        BNM::CompileTimeClassBuilder("PvZCards.Engine.Effects", "ExtraCombatStepEffect", "EngineLib.dll").Build(),
        BNM::CompileTimeClassBuilder("PvZCards.Engine.Effects", "StateCheckEffect", "EngineLib.dll").Build(), {});
    // InstanceManager acquires this custom runtime effect through reflection.
    BNM_CustomField(HistoryId,BNM::Defaults::Get<std::uint64_t>(),"HistoryId");
    std::uint64_t HistoryRecord() const {
        static auto field=BNM::Class("PvZCards.Engine.Effects","ExtraCombatStepEffect").GetField("HistoryId").cast<std::uint64_t>();
        return field.IsValid()?Core::Metadata::Bind(field,reinterpret_cast<Obj*>(const_cast<CombatStepEffect*>(this))).Get():0;
    }
    void SetHistoryRecord(std::uint64_t id){
        static auto field=BNM::Class("PvZCards.Engine.Effects","ExtraCombatStepEffect").GetField("HistoryId").cast<std::uint64_t>();
        if(field.IsValid())Core::Metadata::Bind(field,static_cast<Obj*>(this)).Set(id);
    }
    BNM_CustomField(Stage, BNM::Defaults::Get<int>(), "Stage");
    BNM_CustomField(DeathsRequested,BNM::Defaults::Get<int>(),"DeathsRequested");
    BNM_CustomField(LaneMode, BNM::Defaults::Get<int>(), "LaneMode");
    BNM_CustomField(First, BNM::Defaults::Get<int>(), "First");
    BNM_CustomField(Second, BNM::Defaults::Get<int>(), "Second");
    BNM_CustomField(FirstLane, BNM::Defaults::Get<int>(), "FirstLane");
    BNM_CustomField(SecondLane, BNM::Defaults::Get<int>(), "SecondLane");
    BNM_CustomField(OriginalSource, BNM::Defaults::Get<int>(), "OriginalSource");
    BNM_CustomField(OriginalEffectEntity, BNM::Defaults::Get<int>(), "OriginalEffectEntity");

    BNM_CustomField(RemainingTargets, BNM::Defaults::Get<BNM::Structures::Mono::String*>(), "RemainingTargets");
    auto Remaining() const {
        static auto field=BNM::Class("PvZCards.Engine.Effects","ExtraCombatStepEffect").GetField("RemainingTargets").cast<BNM::Structures::Mono::String*>();
        auto* self=reinterpret_cast<Obj*>(const_cast<CombatStepEffect*>(this));
        return field.IsValid()?Core::Metadata::Bind(field,self).Get():nullptr;
    }
    void SetRemaining(BNM::Structures::Mono::String* value) {
        static auto field=BNM::Class("PvZCards.Engine.Effects","ExtraCombatStepEffect").GetField("RemainingTargets").cast<BNM::Structures::Mono::String*>();
        if(!field.IsValid())return;
        auto* slot=Core::Metadata::Bind(field,static_cast<Obj*>(this)).GetPointer();
        if(Core::ManagedRuntime::writeBarrier)Core::ManagedRuntime::writeBarrier(this,reinterpret_cast<void**>(slot),value);
        else *slot=value;
    }
    int Read(const char* name, int fallback=-1) const {
        auto field=IntField(true,name);
        auto* self=reinterpret_cast<Obj*>(const_cast<CombatStepEffect*>(this));
        return field.IsValid()?field[self].Get():fallback;
    }
    void Write(const char* name,int value) {
        auto field=IntField(true,name);
        if(field.IsValid())field[reinterpret_cast<Obj*>(this)].Set(value);
    }

    void Construct() {
        Core::NativeConstruction::Base(this);
        ResetEncounter();
    }
    BNM_CustomMethod(Construct, false, BNM::Defaults::Get<void>(), ".ctor");

    void ResetEncounter() {
        SetHistoryRecord(0);
        Write("DeathsRequested",0);
        Write("LaneMode",0);
        SetRemaining(nullptr);
        Write("Stage",0); Write("First",-1); Write("Second",-1);
        Write("FirstLane",-1); Write("SecondLane",-1);
        Write("OriginalSource",-1); Write("OriginalEffectEntity",-1);
    }

    void DeepCopyFrom(Obj* source, Obj* instances) {
        BNM_CallCustomMethodOrigin(DeepCopyFrom, this, source, instances);
        if (!source) return;
        auto* other=reinterpret_cast<CombatStepEffect*>(source);
        SetRemaining(other->Remaining());
        SetHistoryRecord(other->HistoryRecord());
        for(auto name:{"Stage","First","Second","FirstLane","SecondLane",
                       "OriginalSource","OriginalEffectEntity","LaneMode","DeathsRequested"})
            Write(name,other->Read(name));
    }
    BNM_CustomMethod(DeepCopyFrom, false, BNM::Defaults::Get<void>(), "DeepCopyFrom",
        BNM::Defaults::Get<Obj*>(),
        BNM::CompileTimeClassBuilder("PvZCards.Engine", "InstanceManager", "EngineLib.dll").Build());

    void HandleReturnToPool(Obj* instances) {
        BNM_CallCustomMethodOrigin(HandleReturnToPool, this, instances);
        ResetEncounter();
    }
    BNM_CustomMethod(HandleReturnToPool, false, BNM::Defaults::Get<void>(), "HandleReturnToPool",
        BNM::CompileTimeClassBuilder("PvZCards.Engine", "InstanceManager", "EngineLib.dll").Build());
};

struct CombatDescriptor : Obj {
    std::uint8_t inherited[8]{};
    BNM_CustomClass(CombatDescriptor,
        BNM::CompileTimeClassBuilder("PvZCards.Engine.Components", "CombatEffectDescriptor", "EngineLib.dll").Build(),
        BNM::CompileTimeClassBuilder("PvZCards.Engine.Components", "AttackInLaneEffectDescriptor", "EngineLib.dll").Build(), {});
    Obj* ToEffect(Obj* instances, Obj*) {
        Obj* effect=Core::ManagedPool::Acquire(instances,
            BNM::Class("PvZCards.Engine.Effects", "ExtraCombatStepEffect"));
        static std::atomic<unsigned> samples{0};
        if(samples.fetch_add(1)<16)
            LOG_INFO("CustomComponents/ExtraCombat: descriptor converted runtime=%p",effect);
        return effect;
    }
    BNM_CustomMethod(ToEffect, false,
        BNM::CompileTimeClassBuilder("PvZCards.Engine", "Effect", "EngineLib.dll").Build(), "ToEffect",
        BNM::CompileTimeClassBuilder("PvZCards.Engine", "InstanceManager", "EngineLib.dll").Build(),
        BNM::CompileTimeClassBuilder("PvZCards.Engine", "Entity", "EngineLib.dll").Build());
};

// Mirrors the IL2CPP definitions for this APK, without relying on guessed
// managed offsets for these two value types.
struct AttackData { int DamageAmount; bool IsOverridden; std::uint8_t padding[3]{}; };
struct LaneCombatData {
    Obj* Fighter;
    int OpponentPlayerId;
    std::uint32_t padding{};
    Obj* Lane;
    int LaneId;
    AttackData Attack;
};
static_assert(sizeof(AttackData)==8);
static_assert(sizeof(LaneCombatData)==40);

inline bool enabled = false;

inline auto EffectType() { return BNM::Class("PvZCards.Engine", "Effect"); }
inline int Field(Obj* object, const char* name, int fallback=-1) {
    auto f=IntField(false,name);
    return object && f.IsValid() ? f[object].Get() : fallback;
}
inline void SetField(Obj* object, const char* name, int value) {
    auto f=IntField(false,name);
    if(object && f.IsValid())f[object].Set(value);
}
inline bool LiveFighter(Obj* model, int id) {
    Obj* entity=Engine::EntityAccess::Find(model,id);
    if(!entity || !Engine::CardTypes::Fighter(entity) ||
       !Engine::EntityAccess::Component(entity,"LaneElement"))return false;
    Obj* health=Engine::EntityAccess::Component(entity,"Health");
    if(!health)return false;
    static auto calculate=BNM::Class("PvZCards.Engine.Components","Health")
        .GetMethod("CalculateHealth",0).cast<int>();
    if(!calculate.IsValid())return false;
    int hp=0;
    auto exception=BNM::TryInvoke([&]{hp=calculate[health]();});
    return !exception.IsValid() && hp>0;
}
inline int Lane(Obj* model, int id) {
    Obj* fighter=Engine::EntityAccess::Find(model,id);
    Obj* element=Engine::EntityAccess::Component(fighter,"LaneElement");
    static auto ids=BNM::Class("PvZCards.Engine.Components","LaneElement")
        .GetField("LaneIds").cast<BNM::Structures::Mono::List<int>*>();
    auto* lanes=element && ids.IsValid()?ids[element].Get():nullptr;
    if(!lanes || !lanes->items || lanes->size!=1)return -1;
    return lanes->items->m_Items[0];
}
inline int EnvironmentLane(Obj* environment) {
    Obj* element=Engine::EntityAccess::Component(environment,"LaneElement");
    static auto ids=BNM::Class("PvZCards.Engine.Components","LaneElement")
        .GetField("LaneIds").cast<BNM::Structures::Mono::List<int>*>();
    auto* lanes=element && ids.IsValid()?ids[element].Get():nullptr;
    return lanes && lanes->items && lanes->size==1?lanes->items->m_Items[0]:-1;
}
inline bool CanFight(Obj* model, int first, int second) {
    return Engine::FighterPair::Legal(model,first,second) &&
        Lane(model,first)>=0 && Lane(model,second)>=0;
}

// Both fighters and every environment in their original lanes are listeners.
// Stock filters decide eligibility; team-up partners do not join the encounter.
struct Listener {int card=-1;int lane=-1;};
inline std::vector<Listener> LaneListeners(Obj*,const CombatStepEffect*);
inline std::vector<Listener> Listeners(Obj* model, const CombatStepEffect* step) {
    if(step->Read("LaneMode",0))return LaneListeners(model,step);
    std::vector<Listener> out;
    std::unordered_set<int> seen;
    const int first=step->Read("First"),second=step->Read("Second");
    const int firstLane=step->Read("FirstLane"),secondLane=step->Read("SecondLane");
    for(auto fighterId:{first,second}){
        Obj* fighter=Engine::EntityAccess::Find(model,fighterId);
        const int lane=fighterId==first?firstLane:secondLane;
        if(fighter && lane>=0 && seen.insert(fighterId).second)
            out.push_back({fighterId,lane});
    }
    auto entities=Engine::EntityAccess::Api().entities;
    auto* all=entities[model].Get();
    if(!Engine::EntityAccess::Valid(all))return out;
    for(int i=0;i<all->size;++i){
        Obj* candidate=all->items->m_Items[i];
        if(!Engine::EntityAccess::Component(candidate,"Environment"))continue;
        int lane=EnvironmentLane(candidate);
        // Environments belong to lanes, not to the participating fighter's
        // faction. Their own filters decide which side they affect.
        const bool adjacentSide=lane==firstLane || lane==secondLane;
        const int id=Engine::Match::Id(candidate);
        if(adjacentSide && id>=0 && seen.insert(id).second)
            out.push_back({id,lane});
    }
    return out;
}

inline Obj* Queue(Obj* model, Obj* instances) {
    Obj* queue=Engine::Match::Provide(instances,
        BNM::Class("PvZCards.Engine","EffectCollection").GetMonoType());
    Obj* injected=Engine::Match::Provide(instances,
        BNM::Class("PvZCards.Engine","EntityModel").GetMonoType());
    return queue && injected==model?queue:nullptr;
}
inline bool Enqueue(Obj* model, Obj* instances, const EffectBatch& effects) {
    if(effects.empty())return true;
    Obj* queue=Queue(model,instances);
    if(!queue)return false;
    BNM::Method<void> add;
    for(auto method:BNM::Class(queue).GetMethods()){
        auto* info=method.GetInfo();
        if(!info || std::string_view(info->name)!="AddAfterCurrentGroup" || info->parameters_count!=1)continue;
        auto* param=BNM::Class(info->parameters[0]).GetClass();
        if(param && param->rank==1){add=method.cast<void>();break;}
    }
    if(!add.IsValid())return false;
    auto* arr=EffectType().NewArray<Obj*>(effects.size());
    Core::ManagedRuntime::Root root(reinterpret_cast<Obj*>(arr));
    if(!arr || !root.Get())return false;
    for(size_t i=0;i<effects.size();++i){
        if(Core::ManagedRuntime::writeBarrier)
            Core::ManagedRuntime::writeBarrier(arr,reinterpret_cast<void**>(&arr->m_Items[i]),effects[i]);
        else arr->m_Items[i]=effects[i];
    }
    auto error=BNM::TryInvoke([&]{add[queue](arr);});
    return !error.IsValid();
}
inline Obj* Next(Obj* instances, const CombatStepEffect* previous, int stage) {
    Obj* object=Core::ManagedPool::Acquire(instances,
        BNM::Class("PvZCards.Engine.Effects","ExtraCombatStepEffect"));
    if(!object)return nullptr;
    auto* next=reinterpret_cast<CombatStepEffect*>(object);
    next->Write("Stage",stage);
    next->SetRemaining(previous->Remaining());
    next->SetHistoryRecord(previous->HistoryRecord());
    for(auto name:{"First","Second","FirstLane","SecondLane",
                   "OriginalSource","OriginalEffectEntity","LaneMode","DeathsRequested"})
        next->Write(name,previous->Read(name));
    for(auto name:{"PlayerId","SourceId","TargetId","SecondaryTargetId", "SpringboardId", "EffectEntityId", "EffectGroupId", "SortingIndex"})
        SetField(object,name,Field(const_cast<CombatStepEffect*>(previous),name));
    return object;
}
inline void RecordEncounter(Obj* model,CombatStepEffect* step,const char* phase,
                            const std::vector<Obj*>& attacks={}){
    if(!model || Core::Performance::aiDepth)return;
    History::CombatPayload payload;payload.phase=phase;payload.first=step->Read("First");payload.second=step->Read("Second");
    payload.firstLane=step->Read("FirstLane");payload.secondLane=step->Read("SecondLane");payload.laneMode=step->Read("LaneMode",0)!=0;
    static auto amount=BNM::Class("PvZCards.Engine.Effects","DamageEffect").GetMethod("get_DamageAmount",0).cast<int>();
    for(Obj* attack:attacks){
        if(!attack || !attack->klass || std::string_view(attack->klass->name)=="ExtraCombatStepEffect")continue;
        payload.effectTypes.emplace_back(attack->klass->name);
        payload.attackers.push_back(Field(attack,"SourceId"));payload.defenders.push_back(Field(attack,"TargetId"));
        if(std::string_view(attack->klass->name)=="DamageEffect" && amount.IsValid())payload.amounts.push_back(Core::Metadata::Bind(amount,attack)());
        else payload.amounts.push_back(std::nullopt);
    }
    const auto id=History::Domain::Emit(model,History::Kind::CombatEncounter,std::move(payload),step->Read("First"),step->HistoryRecord());
    if(!step->HistoryRecord())step->SetHistoryRecord(id);
}
inline bool Stage(Obj* model, Obj* instances, CombatStepEffect* current, int next) {
    Obj* carrier=Next(instances,current,next);
    if(!carrier)return false;
    Core::ManagedRuntime::Root root(carrier);
    if(Engine::EffectContinuations::Schedule(model,instances,{carrier}))return true;
    RecordEncounter(model,current,"cancelled-enqueue");
    Core::ManagedPool::Return(instances,carrier);
    return false;
}

// CustomTriggerSystem's targeted validator consumes Effect source and target.
// For lane combat events, the event lane is the trigger target; original source
// remains attached to the staged carrier outside the temporary dispatch call.
inline bool Dispatch(Obj* model, CombatStepEffect* step, const char* marker) {
    bool success=true;
    const int source=Field(step,"SourceId"),target=Field(step,"TargetId");
    const int effectEntity=Field(step,"EffectEntityId");
    for(auto listener:Listeners(model,step)){
        // Preserve the encounter lane even if a before-combat ability moved a fighter.
        if(!Engine::EntityAccess::Find(model,listener.card))continue;
        // LaneCombatPhaseSystem creates a fresh lane event and sets TargetId
        // only. A lane is the event target, not a synthetic attacking source.
        SetField(step,"SourceId",-1);
        SetField(step,"TargetId",listener.lane);
        // An EffectEntityId of -1 means an event generated by the engine, not
        // an effect sourced from the listener. Original provenance is kept.
        SetField(step,"EffectEntityId",-1);
        const bool fired=Systems::CustomTriggerSystem::FireForEntity(marker,listener.card,step,false);
        LOG_INFO("CustomComponents/ExtraCombat: trigger=%s listener=%d lane=%d result=%d moveTrigger=0",
            marker,listener.card,listener.lane,int(fired));
        if(!fired)
            success=false;
    }
    SetField(step,"SourceId",source);SetField(step,"TargetId",target);
    SetField(step,"EffectEntityId",effectEntity);
    return success;
}

inline bool BuildAttackEffects(Obj* model, Obj* instances, int attackerId, int defenderId,
                               EffectBatch& effects){
    if(!CanFight(model,attackerId,defenderId))return false;
    Obj* fighter=Engine::EntityAccess::Find(model,attackerId);
    Obj* defender=Engine::EntityAccess::Find(model,defenderId);
    const int defenderLane=Lane(model,defenderId);
    Obj* lane=Engine::EntityAccess::Find(model,defenderLane);
    Obj* laneComponent=Engine::EntityAccess::Component(lane,"Lane");
    if(!laneComponent || !defender)return false;
    const int defenderOwner=Engine::BoardPlacement::Owner(defender);
    if(defenderOwner<0)return false;

    auto attackType=BNM::Class("PvZCards.Core.Systems.Utilities","AttackValueProvider");
    auto builderType=BNM::Class("PvZCards.Core.Systems.Utilities","DamageListBuilder");
    auto creatorType=BNM::Class("PvZCards.Core.Systems.Utilities","FighterDamageEffectCreator");
    auto getAttack=attackType.GetMethod("GetAttackDataForCard",1).cast<AttackData>();
    auto primaryEffects=creatorType.GetMethod("AddPrimaryDamageEffects",3).cast<void>();
    auto secondaryEffects=creatorType.GetMethod("AddSecondaryDamageEffects",2).cast<void>();
    auto finish=builderType.GetMethod("ToEffectsList",0).cast<Obj*>();
    if(!getAttack.IsValid() || !primaryEffects.IsValid() || !secondaryEffects.IsValid() || !finish.IsValid())return false;
    auto provider=Engine::Match::Service(instances,attackType);
    auto creator=Engine::Match::Service(instances,creatorType);
    auto builder=Engine::Match::Service(instances,builderType);
    if(!provider || !creator || !builder)return false;
    Obj* providerObj=provider->Get(),*creatorObj=creator->Get(),*builderObj=builder->Get();
    if(!providerObj || !creatorObj || !builderObj)return false;

    ObjList* opponents=BNM::Class("PvZCards.Engine","Entity").NewList<Obj*>();
    Core::ManagedRuntime::Root opponentRoot(reinterpret_cast<Obj*>(opponents));
    if(!opponents || !opponentRoot.Get())return false;
    auto add=BNM::Class(opponents).GetMethod("Add",1).cast<void>();
    if(!add.IsValid())return false;
    auto error=BNM::TryInvoke([&]{add[opponents](defender);});
    if(error.IsValid())return false;
    AttackData attack{};
    error=BNM::TryInvoke([&]{attack=getAttack[providerObj](fighter);});
    if(error.IsValid())return false;
    // Calling native FighterDamageEffectCreator's two halves reuses stock
    // Splash, adjacent-lane and all-lane attack generation. The opponent list
    // is precisely the selected fighter, even for friendly combat.
    LaneCombatData context{fighter,defenderOwner,0,laneComponent,defenderLane,attack};
    error=BNM::TryInvoke([&]{
        primaryEffects[creatorObj](context,opponents,builderObj);
        secondaryEffects[creatorObj](context,builderObj);
    });
    if(error.IsValid())return false;
    Obj* generated=nullptr;
    error=BNM::TryInvoke([&]{generated=finish[builderObj]();});
    if(error.IsValid() || !generated)return false;
    Core::ManagedRuntime::Root generatedRoot(generated);
    auto* list=reinterpret_cast<ObjList*>(generated);
    if(!Engine::EntityAccess::Valid(list))return false;
    effects.reserve(list->size);
    for(int i=0;i<list->size;++i){
        Obj* damage=list->items->m_Items[i];if(!damage)continue;
        // Preserve stock-generated effect provenance. Overwriting its effect
        // entity/group here would suppress or misattribute damage triggers.
        effects.push_back(damage);
    }
    LOG_INFO("CustomComponents/ExtraCombat: built attack %d -> %d lane=%d effects=%zu",attackerId,defenderId,defenderLane,static_cast<std::size_t>(list->size));
    return true;
}

#include "combat/detail/LaneEncounters.inl"

inline void ContinueTargets(Obj* model,Obj* instances,CombatStepEffect* step) {
    if(step->Read("LaneMode",0))return;
    auto* text=step->Remaining();
    if(!text)return;
    const auto targets=text->str();
    std::size_t begin=0;
    while(begin<targets.size()) {
        const auto end=targets.find(',',begin);
        const auto part=targets.substr(begin,end==std::string::npos?targets.size()-begin:end-begin);
        int target=-1;
        const auto parsed=std::from_chars(part.data(),part.data()+part.size(),target);
        begin=end==std::string::npos?targets.size():end+1;
        const int savedFirst=step->Read("First");
        const int first=savedFirst>=0?savedFirst:Field(step,"TargetId");
        if(parsed.ec!=std::errc{} || !CanFight(model,first,target))continue;
        Obj* next=Next(instances,step,0);Core::ManagedRuntime::Root root(next);
        if(!next)return;
        SetField(next,"TargetId",first);SetField(next,"SecondaryTargetId",target);
        auto* carrier=reinterpret_cast<CombatStepEffect*>(next);
        carrier->SetHistoryRecord(0); // Each sequential pair gets a distinct encounter identity.
        carrier->Write("DeathsRequested",0);
        carrier->SetRemaining(begin<targets.size()?BNM::CreateMonoString(targets.substr(begin)):nullptr);
        if(!Engine::EffectContinuations::Schedule(model,instances,{next}))LOG_ERR("CustomComponents/ExtraCombat: next target enqueue failed");
        return;
    }
}

// Native manual-primary/all-secondary selection produces one carrier per pair.
// Keep one carrier and serialize its remaining selections in lane order. This
// prevents independent start stages interleaving before either encounter ends.
inline void GroupTargets(Obj* entity,Obj* context,Obj* result) {
    auto* effects=reinterpret_cast<ObjList*>(result);
    Obj* model=Engine::BoardPlacement::Model(context);
    Obj* instances=Engine::Match::Instances(entity);
    if(!model || !instances || !Engine::EntityAccess::Valid(effects))return;
    std::map<int,std::vector<int>> groups;
    for(int i=0;i<effects->size;++i){
        Obj* effect=effects->items->m_Items[i];
        if(effect && effect->klass && std::string_view(effect->klass->name)=="ExtraCombatStepEffect")groups[Field(effect,"TargetId")].push_back(i);
    }
    auto remove=BNM::Class(result).GetMethod("RemoveAt",1).cast<void>();
    if(!remove.IsValid())return;
    std::vector<int> removed;
    for(const auto& [first,indices]:groups){
        if(indices.size()<2)continue;
        std::vector<std::pair<int,int>> targets;
        targets.reserve(indices.size());
        for(int index:indices){
            const int target=Field(effects->items->m_Items[index],"SecondaryTargetId");
            if(target<0)continue;
            auto* lane=Engine::EntityAccess::Find(model,Lane(model,target));
            const int order=Field(Engine::EntityAccess::Component(lane,"Lane"),"CombatOrderIndex");
            targets.emplace_back(order>=0?order:INT32_MAX,target);
        }
        std::sort(targets.begin(),targets.end());
        targets.erase(std::unique(targets.begin(),targets.end()),targets.end());
        if(targets.empty())continue;
        if(reinterpret_cast<std::uintptr_t>(model)==Core::SceneViewContext::ModelIdentity())
            LOG_INFO("CustomComponents/ExtraCombat/MULTI: first=%d secondaryCount=%zu",first,targets.size());
        Obj* head=effects->items->m_Items[indices.front()];SetField(head,"SecondaryTargetId",targets.front().second);
        std::string pending;for(std::size_t i=1;i<targets.size();++i){if(!pending.empty())pending+=',';pending+=std::to_string(targets[i].second);}
        reinterpret_cast<CombatStepEffect*>(head)->SetRemaining(LaneEncounter(Engine::EntityAccess::Find(model,first)) || pending.empty()?nullptr:BNM::CreateMonoString(pending));
        removed.insert(removed.end(),indices.begin()+1,indices.end());
    }
    std::sort(removed.rbegin(),removed.rend());
    for(const int index:removed){Obj* effect=effects->items->m_Items[index];Core::Metadata::Bind(remove,result)(index);Core::ManagedPool::Return(instances,effect);}
}
inline bool Prepare(const Engine::EffectGuards::Context& ctx){
    Obj* effect=ctx.effect;
    if(!effect || !effect->klass || !effect->klass->name ||
        std::string_view(effect->klass->name)!="ExtraCombatStepEffect")return true;
    auto* step=reinterpret_cast<CombatStepEffect*>(effect);
    const int stage=step->Read("Stage",0);
    static std::atomic<unsigned> entries{0};
    if(entries.fetch_add(1)<64)
        LOG_INFO("CustomComponents/ExtraCombat: carrier entered stage=%d source=%d target=%d secondary=%d effectEntity=%d",
            stage,Field(effect,"SourceId"),Field(effect,"TargetId"),
            Field(effect,"SecondaryTargetId"),Field(effect,"EffectEntityId"));
    // Never pass this custom carrier into an unregistered stock EffectRouter.
    Obj* model=Core::ModelRegistry::Get(Engine::RouterContext::RouterModel(ctx.router));
    if(!model){LOG_ERR("CustomComponents/ExtraCombat: no model for router");return false;}
    // Gameplay is identical in live, prediction and remote replay models.
    // Only the presentation request is restricted to the current scene.
    auto validation=EffectType().GetField("RequiresTargetValidation").cast<bool>();
    if(validation.IsValid())Core::Metadata::Bind(validation,effect).Set(false);
    int source=Field(effect,"SourceId");
    Obj* sourceCard=Engine::EntityAccess::Find(model,source);
    Obj* instances=Engine::Match::Instances(sourceCard);
    // The initiating trick may leave play; the selected fighters remain valid
    // model anchors (even if later sent to discard during the encounter).
    if(!instances)instances=Engine::Match::Instances(Engine::EntityAccess::Find(model,
        stage==0?Field(effect,"TargetId"):step->Read("First")));
    if(!instances && stage>0)instances=Engine::Match::Instances(
        Engine::EntityAccess::Find(model,step->Read("Second")));
    if(!instances){
        static auto manager=BNM::Class("PvZCards.Engine","EntityModel").GetField("InstanceManager").cast<Obj*>();
        if(manager.IsValid())instances=Core::Metadata::Bind(manager,model).Get();
    }
    if(!instances){LOG_ERR("CustomComponents/ExtraCombat: no matching instances");return false;}
    if(stage==0 || stage==2){
        const auto boundary=Engine::ContinuousEffects::BeforeAction(model,instances,effect);
        if(boundary==Engine::ContinuousEffects::Boundary::Pending){
            if(!Stage(model,instances,step,stage))LOG_ERR("CustomComponents/ExtraCombat: continuous boundary continuation failed");
            return true;
        }
        if(boundary==Engine::ContinuousEffects::Boundary::Unavailable){
            LOG_ERR("CustomComponents/ExtraCombat: continuous boundary unavailable; combat cancelled");
            Presentation::CombatFacing::Queue(model,step->Read("First"),step->Read("Second"),false,step->HistoryRecord());
            return false;
        }
    }
    switch(stage){
    case 0: {
        // A single selected opponent means the source fighter attacks it.
        // Explicit two-fighter selections retain their authored order.
        if(!LiveFighter(model,Field(effect,"SecondaryTargetId")) &&
           LiveFighter(model,source) && source!=Field(effect,"TargetId")){
            SetField(effect,"SecondaryTargetId",Field(effect,"TargetId"));
            SetField(effect,"TargetId",source);
        }
        const bool laneMode=LaneEncounter(Engine::EntityAccess::Find(model,Field(effect,"TargetId")));
        if(laneMode?!LiveFighter(model,Field(effect,"TargetId")):!CanFight(model,Field(effect,"TargetId"),Field(effect,"SecondaryTargetId"))){
            ContinueTargets(model,instances,step);
            LOG_WARN("CustomComponents/ExtraCombat: selection rejected (two distinct in-play fighters required)");return false;
        }
        const int first=Field(effect,"TargetId"),second=laneMode?-1:Field(effect,"SecondaryTargetId");
        step->Write("LaneMode",laneMode?1:0);
        const int firstLane=Lane(model,first),secondLane=Lane(model,second);
        step->Write("First",first);step->Write("Second",second);
        step->Write("OriginalSource",source);step->Write("OriginalEffectEntity",Field(effect,"EffectEntityId"));
        step->Write("FirstLane",firstLane);step->Write("SecondLane",secondLane);
        RecordEncounter(model,step,"begin");
        LOG_INFO("CustomComponents/ExtraCombat: begin %d lane=%d vs %d lane=%d",first,firstLane,second,secondLane);
        Presentation::CombatFacing::Queue(model,first,second,true,step->HistoryRecord());
        if(!Stage(model,instances,step,1)){
            Presentation::CombatFacing::Queue(model,first,second,false,step->HistoryRecord());
            LOG_ERR("CustomComponents/ExtraCombat: failed to queue start");
        }
        break;
    }
    case 1: {
        RecordEncounter(model,step,"before-combat");
        Dispatch(model,step,"CombatStartTrigger");
        Dispatch(model,step,"LaneCombatStartTrigger");
        if(!Stage(model,instances,step,2)){
            Presentation::CombatFacing::Queue(model,step->Read("First"),step->Read("Second"),false,step->HistoryRecord());
            LOG_ERR("CustomComponents/ExtraCombat: failed to queue first strike");
        }
        break;
    }
    case 2: {
        // Before-combat effects have resolved. Retire their casualties through
        // the native discard pipeline before deciding who is still attacking.
        bool disposing=false;
        auto discarder=Engine::Match::Service(instances,BNM::Class("PvZCards.Core.Systems.Utilities","CardInPlayDiscarder"));
        static auto discard=BNM::Class("PvZCards.Core.Systems.Utilities","CardInPlayDiscarder").GetMethod("CreateAndQueueDiscardEffect",1).cast<void>();
        for(int id:{step->Read("First"),step->Read("Second")}){
            Obj* card=Engine::EntityAccess::Find(model,id);
            Obj* health=Engine::EntityAccess::Component(card,"Health");
            static auto calculate=BNM::Class("PvZCards.Engine.Components","Health").GetMethod("CalculateHealth",0).cast<int>();
            const bool dead=Engine::EntityAccess::Component(card,"MarkedForDeath") || (health && calculate.IsValid() && Core::Metadata::Bind(calculate,health)()<=0);
            if(card && dead && Engine::EntityAccess::Component(card,"LaneElement") && discarder && discard.IsValid()){
                const int bit=id==step->Read("First")?1:2;
                if(step->Read("DeathsRequested",0)&bit){
                    if(Engine::EntityAccess::Component(card,"PreDiscardPending")){disposing=true;continue;}
                    RecordEncounter(model,step,"cancelled-death-drain");return Stage(model,instances,step,4);
                }
                Core::Metadata::Bind(discard,discarder->Get())(card);step->Write("DeathsRequested",step->Read("DeathsRequested",0)|bit);disposing=true;
            }
        }
        if(disposing){RecordEncounter(model,step,"before-combat-deaths");return Stage(model,instances,step,2);}
        // Generate both sides before either damage effect resolves. Stock
        // combat is simultaneous: a fighter killed by the first generated
        // damage still gets its already-earned counterattack.
        EffectBatch effects;
        const int first=step->Read("First"),second=step->Read("Second");
        const bool firstAlive=LiveFighter(model,first),secondAlive=LiveFighter(model,second);
        bool built=true;
        if(step->Read("LaneMode",0))built=!firstAlive || BuildLaneEncounter(model,instances,step,effects);
        else if(firstAlive && secondAlive)built=CanFight(model,first,second) && BuildAttackEffects(model,instances,first,second,effects) && BuildAttackEffects(model,instances,second,first,effects);
        else if(firstAlive || secondAlive){
            Obj* bonus=Core::ManagedPool::Acquire(instances,BNM::Class("PvZCards.Engine.Effects","ExtraAttackEffect"));
            Core::ManagedRuntime::Root root(bonus);
            static auto copy=EffectType().GetMethod("DeepCopyFrom",2).cast<void>();
            built=bonus && copy.IsValid();
            if(built){
                Core::Metadata::Bind(copy,bonus)(effect,instances);
                SetField(bonus,"TargetId",firstAlive?first:second);SetField(bonus,"SecondaryTargetId",-1);SetField(bonus,"EffectEntityId",-1);
                if(validation.IsValid())Core::Metadata::Bind(validation,bonus).Set(false);
                effects.push_back(bonus);RecordEncounter(model,step,"survivor-bonus-attack");
            }
        }else RecordEncounter(model,step,"no-survivors");
        if(!built){
            Presentation::CombatFacing::Queue(model,first,second,false,step->HistoryRecord());
            RecordEncounter(model,step,"cancelled-generation");
            LOG_ERR("CustomComponents/ExtraCombat: primary attack generation failed");
            return false;
        }
        // AddAfterCurrentGroup inserts a whole group at the same boundary.
        // A separate Stage call would insert the continuation ahead of damage.
        Obj* continuation=Next(instances,step,3);
        Core::ManagedRuntime::Root continuationRoot(continuation);
        if(continuation)effects.push_back(continuation);
        if(!continuation || !Enqueue(model,instances,effects)){
            Presentation::CombatFacing::Queue(model,first,second,false,step->HistoryRecord());
            RecordEncounter(model,step,"cancelled-enqueue");
            LOG_ERR("CustomComponents/ExtraCombat: failed to queue combat-end");
        }
        else RecordEncounter(model,step,"attacks-queued",effects);
        break;
    }
    case 3: {
        RecordEncounter(model,step,"attacks-resolved");
        if(!Stage(model,instances,step,4)){
            Presentation::CombatFacing::Queue(model,step->Read("First"),step->Read("Second"),false,step->HistoryRecord());
            RecordEncounter(model,step,"cancelled-enqueue");
            LOG_ERR("CustomComponents/ExtraCombat: failed to queue combat-end");
        }
        break;
    }
    case 4: {
        Presentation::CombatFacing::Queue(model,step->Read("First"),step->Read("Second"),false,step->HistoryRecord());
        RecordEncounter(model,step,"after-combat");
        Dispatch(model,step,"LaneCombatEndTrigger");
        Dispatch(model,step,"CombatEndTrigger");
        if(!Stage(model,instances,step,5))LOG_ERR("CustomComponents/ExtraCombat: failed to queue extra-combat event");
        break;
    }
    case 5: {
        const bool triggerQueued=Dispatch(model,step,"ExtraCombatTrigger");
        RecordEncounter(model,step,"complete");
        ContinueTargets(model,instances,step);
        LOG_INFO("CustomComponents/ExtraCombat: encounter complete first=%d second=%d extraCombatTrigger=%d moveTrigger=0",
            step->Read("First"),step->Read("Second"),int(triggerQueued));
        break;
    }
    default: break;
    }
    // Let native effect processors flush queued trigger data and state checks
    // before the continuation. Suppressing the router delayed start abilities
    // until a later damage effect, effectively turning them into after effects.
    return true;
}

inline void RegisterCarrier(Obj* router,Obj* type,Obj* handler,const void* method){
    auto base=BNM::Class("PvZCards.Engine.Effects","StateCheckEffect");
    if(type==reinterpret_cast<Obj*>(base.GetMonoType()))
        Engine::EffectRegistration::Alias(router,reinterpret_cast<Obj*>(BNM::Class("PvZCards.Engine.Effects","ExtraCombatStepEffect").GetMonoType()),handler,method);
}

inline bool Convert(Obj* descriptor, Obj* instances, Obj*, Obj*& result){
    if(!descriptor || !descriptor->klass || !descriptor->klass->name ||
        std::string_view(descriptor->klass->name)!="CombatEffectDescriptor")return false;
    result=Core::ManagedPool::Acquire(instances,
        BNM::Class("PvZCards.Engine.Effects","ExtraCombatStepEffect"));
    static std::atomic<unsigned> samples{0};
    if(samples.fetch_add(1)<16)
        LOG_INFO("CustomComponents/ExtraCombat: conversion hook runtime=%p",result);
    if(!result)LOG_ERR("CustomComponents/ExtraCombat: descriptor conversion failed");
    return result!=nullptr;
}

inline bool Initialize(){
    if(!Engine::EffectContinuations::Initialize())return false;

    if(!CustomComponentRegistry::Register({"ExtraCombatTrigger","PvZCards.Engine.Components","EngineLib.dll",
        "MarkerComponent","PvZCards.Engine.Components","EngineLib.dll"}) ||
        !CustomComponentRegistry::Register({"CombatEffectDescriptor","PvZCards.Engine.Components","EngineLib.dll",
        "AttackInLaneEffectDescriptor","PvZCards.Engine.Components","EngineLib.dll"}) ||
        !CustomComponentRegistry::Register({"ExtraCombatStepEffect","PvZCards.Engine.Effects","EngineLib.dll",
        "StateCheckEffect","PvZCards.Engine.Effects","EngineLib.dll"}))return false;
    if(!Core::ComponentLifecycle::Patch(BNM::Class("PvZCards.Engine.Effects","ExtraCombatStepEffect"),
            BNM::Class("PvZCards.Engine.Effects","StateCheckEffect")))return false;
    if(!Systems::CustomFieldBridgeSystem::Register("PvZCards.Engine.Components","CombatEffectDescriptor",
        "PvZCards.Engine.Components","AttackInLaneEffectDescriptor",{},false))return false;
    if(!Core::ComponentLifecycle::PatchVirtual(
        BNM::Class("PvZCards.Engine.Components","CombatEffectDescriptor"),
        BNM::Class("PvZCards.Engine.Components","AttackInLaneEffectDescriptor"),
        BNM::Class("PvZCards.Engine.Components","EffectDescriptor"),"ToEffect",2))return false;
    if(!Engine::AbilityConversion::Add(Convert))return false;
    if(!Engine::EffectRegistration::Initialize() || !Engine::EffectRegistration::afterRegister.Add(RegisterCarrier))return false;
    const bool hooked=Engine::EffectCreationEvents::created.Add(GroupTargets) && Engine::EffectGuards::RegisterPreparation(Prepare,-26);
    LOG_INFO("CustomComponents/ExtraCombat: runtime %s",hooked?"enabled":"FAILED");
    return hooked;
}
} // namespace CustomComponents::ExtraCombat
