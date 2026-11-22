#pragma once
#include "history/DomainRecords.hpp"
#include <BNM/ClassesManagement.hpp>
#include "abilities/TriggeredGrantPolicy.hpp"
#include "core/CustomFieldBridgeSystem.hpp"
#include "core/WeakObjectSettings.hpp"
#include "core/ComponentLifecycle.hpp"
#include "core/CustomComponentRegistry.hpp"
#include "core/NativeConstruction.hpp"
#include "engine/AbilityDurationEvents.hpp"
#include "engine/EffectDurations.hpp"
#include "engine/EffectContinuations.hpp"
#include "engine/ZoneChanges.hpp"
#include "engine/SimulationChanges.hpp"
#include "engine/BoardPlacement.hpp"
namespace CustomComponents::Abilities::TriggeredGrants {
using Obj=Engine::Match::Obj;
using Str=BNM::Structures::Mono::String;
namespace Bridge=Systems::CustomFieldBridgeSystem;
struct Options {int duration=0;Policy::Operation operation=Policy::Operation::Maximum;bool enabled=false;};
inline Core::WeakObjectSettings<Options> descriptors,effects;
struct Ledger : Obj {
    Str* Data=nullptr;
    BNM_CustomClass(Ledger,BNM::CompileTimeClassBuilder("PvZCards.Engine.Components","TriggeredAbilityGrantState","EngineLib.dll").Build(),BNM::CompileTimeClassBuilder("PvZCards.Engine.Components","MarkerComponent","EngineLib.dll").Build(),{});
    BNM_CustomField(Data,BNM::Defaults::Get<Str*>(),"Data");
    void Construct(){Core::NativeConstruction::Base(this);}
    BNM_CustomMethod(Construct,false,BNM::Defaults::Get<void>(),".ctor");
    void Assign(Str* value){if(Core::ManagedRuntime::writeBarrier)Core::ManagedRuntime::writeBarrier(this,reinterpret_cast<void**>(&Data),value);else Data=value;}
    void DeepCopyFrom(Obj* source,Obj*){Assign(source?reinterpret_cast<Ledger*>(source)->Data:nullptr);}
    void HandleReturnToPool(Obj*){Assign(nullptr);}
    BNM_CustomMethod(DeepCopyFrom,false,BNM::Defaults::Get<void>(),"DeepCopyFrom",BNM::Defaults::Get<Obj*>(),BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
    BNM_CustomMethod(HandleReturnToPool,false,BNM::Defaults::Get<void>(),"HandleReturnToPool",BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
};
inline BNM::Class descriptorType,runtimeType;
inline BNM::Field<int> abilityGuid,effectSource,effectTarget,counterCount;
inline BNM::Field<Policy::Value> counterValue,effectValue;
inline BNM::Field<BNM::Structures::Mono::List<int>*> entityIds;
inline BNM::Field<Obj*> creatorModel;
inline BNM::Field<int> abilityOwner;
inline BNM::Method<void> setValue;
inline BNM::Method<Obj*> getCounter;
inline auto State(Obj* target,bool create=false){
    Obj* existing=Engine::EntityAccess::Component(target,"TriggeredAbilityGrantState");
    return reinterpret_cast<Ledger*>(existing || !create?existing:Engine::Match::Component(target,BNM::Class("PvZCards.Engine.Components","TriggeredAbilityGrantState").GetMonoType(),true));
}
inline auto Read(Ledger* state){return state && state->Data?Policy::Decode(state->Data->str()):std::vector<Policy::Row>{};}
inline void Save(Ledger* state,const std::vector<Policy::Row>& rows){if(state)state->Assign(rows.empty()?nullptr:BNM::CreateMonoString(Policy::Encode(rows)));}
inline void RecordGrant(Obj* card,const std::vector<Policy::Row>& before,const std::vector<Policy::Row>& after,int guid,int source,int duration,int operation,bool expired=false){
    if(Core::Performance::aiDepth)return;
    std::vector<int> guids;
    for(const auto& rows:{before,after})for(const auto& row:rows)
        if((guid<0 || row.guid==guid) && std::find(guids.begin(),guids.end(),row.guid)==guids.end())guids.push_back(row.guid);
    for(int changed:guids){
        std::vector<Policy::Row> oldRows,newRows;
        for(const auto& row:before)if(row.guid==changed)oldRows.push_back(row);
        for(const auto& row:after)if(row.guid==changed)newRows.push_back(row);
        History::AbilityPayload payload;payload.ability=changed;payload.source=source;payload.duration=duration;
        payload.encoding="triggered-policy-v1";payload.operation=operation;payload.triggered=true;payload.expired=expired;
        payload.before=Policy::Encode(oldRows);payload.after=Policy::Encode(newRows);
        History::Domain::Ability(card,std::move(payload));
    }
}
inline Obj* Counter(Obj* card,int guid){Obj* grants=Engine::EntityAccess::Component(card,"GrantedTriggeredAbilities");return grants?Core::Metadata::Bind(getCounter,grants)(guid):nullptr;}
inline void Refresh(Obj* model,Obj* card,int guid,Policy::Value baseline,const std::vector<Policy::Row>& rows){
    Obj* counter=Counter(card,guid);if(!counter)return;
    Core::Metadata::Bind(counterValue,counter).Set(Policy::Evaluate(rows,guid,baseline));
    auto setter=Engine::Match::Service(Engine::Match::Instances(card),BNM::Class("PvZCards.Core.Utilities","GrantableTriggeredAbilityValueSetter"));
    if(setter)Core::Metadata::Bind(setValue,setter->Get())(counter);
    Engine::SimulationChanges::Changed(model);
}
#include "abilities/detail/TriggeredGrantSerialization.inl"
#include "abilities/detail/TriggeredGrantRuntime.inl"
inline bool Initialize(){
    descriptorType=BNM::Class("PvZCards.Engine.Effects","GrantTriggeredAbilityEffectDescriptor");
    runtimeType=BNM::Class("PvZCards.Engine.Effects","GrantTriggeredAbilityEffect");
    const auto counter=BNM::Class("PvZCards.Engine.Components","AbilityCounter");
    const auto effect=BNM::Class("PvZCards.Engine","Effect");
    abilityGuid=runtimeType.GetField("AbilityGuid").cast<int>();effectValue=runtimeType.GetField("AbilityValue").cast<Policy::Value>();
    effectSource=effect.GetField("SourceId").cast<int>();effectTarget=effect.GetField("TargetId").cast<int>();
    counterCount=counter.GetField("Count").cast<int>();counterValue=counter.GetField("Value").cast<Policy::Value>();entityIds=counter.GetField("EntityIds").cast<BNM::Structures::Mono::List<int>*>();
    getCounter=BNM::Class("PvZCards.Engine.Components","GrantedTriggeredAbilities").GetMethod("GetCounter",1).cast<Obj*>();
    const auto creator=BNM::Class("PvZCards.Core.Utilities","GrantableTriggeredAbilityCreator");creatorModel=creator.GetField("<Model>k__BackingField").cast<Obj*>();
    abilityOwner=BNM::Class("PvZCards.Engine.Components","EffectEntity").GetField("SourceId").cast<int>();
    setValue=BNM::Class("PvZCards.Core.Utilities","GrantableTriggeredAbilityValueSetter").GetMethod("SetAbilityValue",1).cast<void>();
    const auto ledger=BNM::Class("PvZCards.Engine.Components","TriggeredAbilityGrantState");
    if(!descriptorType.IsValid() || !runtimeType.IsValid() || !abilityGuid.IsValid() || !effectValue.IsValid() || !effectSource.IsValid() || !effectTarget.IsValid() || !counterCount.IsValid() || !counterValue.IsValid() || !entityIds.IsValid() || !getCounter.IsValid() || !creatorModel.IsValid() || !abilityOwner.IsValid() || !setValue.IsValid())return false;
    return CustomComponentRegistry::Register({"TriggeredAbilityGrantState","PvZCards.Engine.Components","EngineLib.dll","MarkerComponent","PvZCards.Engine.Components","EngineLib.dll"}) &&
        Core::ComponentLifecycle::Patch(ledger,BNM::Class("PvZCards.Engine.Components","MarkerComponent")) &&
        InitializeSerialization() && Engine::EffectContinuations::Initialize() &&
        Engine::AbilityDurationEvents::expire.Add(Expire) && Engine::ZoneChanges::beforeLaneRemove.Add(Leaving) &&
        Core::Hooks::Install(creator.GetMethod("CreateAbilityFor",2),Create,originalCreate) &&
        Core::Hooks::Install(BNM::Class("PvZCards.Core.Systems","GrantTriggeredAbilitySystem").GetMethod("UpdateCounter",2),Update,originalUpdate) &&
        Core::Hooks::Install(BNM::Class("PvZCards.Core.Systems","RemoveTriggeredAbilitySystem").GetMethod("ProcessEffect",1),Remove,originalRemove);
}
}
