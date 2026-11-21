#pragma once
#include "history/DomainRecords.hpp"
#include "core/NativeConstruction.hpp"
#include "core/ComponentLifecycle.hpp"
#include "core/CustomFieldBridgeSystem.hpp"
#include "engine/EffectContinuations.hpp"
#include "engine/BoardPlacement.hpp"
#include "engine/SimulationChanges.hpp"
#include "triggers/CustomTriggerSystem.hpp"

namespace CustomComponents::Zones::PreDiscard {
using Obj=BNM::IL2CPP::Il2CppObject;
struct Trigger:Obj {
    BNM_CustomClass(Trigger,BNM::CompileTimeClassBuilder("PvZCards.Engine.Components","PreDiscardFromPlayTrigger","EngineLib.dll").Build(),BNM::CompileTimeClassBuilder("PvZCards.Engine.Components","MarkerComponent","EngineLib.dll").Build(),{},BNM::CompileTimeClassBuilder("PvZCards.Engine","Trigger","EngineLib.dll").Build());
    void Construct(){Core::NativeConstruction::Base(this);}
    BNM_CustomMethod(Construct,false,BNM::Defaults::Get<void>(),".ctor");
};
struct PendingDeath:Obj {
    BNM_CustomClass(PendingDeath,BNM::CompileTimeClassBuilder("PvZCards.Engine.Components","PreDiscardPending","EngineLib.dll").Build(),BNM::CompileTimeClassBuilder("PvZCards.Engine.Components","MarkerComponent","EngineLib.dll").Build(),{});
    void Construct(){Core::NativeConstruction::Base(this);}
    BNM_CustomMethod(Construct,false,BNM::Defaults::Get<void>(),".ctor");
};
struct Cause:Obj {
    int SourceId=-1,EffectEntityId=-1;
    BNM_CustomClass(Cause,BNM::CompileTimeClassBuilder("PvZCards.Engine.Components","PendingDeathCause","EngineLib.dll").Build(),BNM::CompileTimeClassBuilder("PvZCards.Engine.Components","MarkerComponent","EngineLib.dll").Build(),{});
    BNM_CustomField(SourceId,BNM::Defaults::Get<int>(),"SourceId");
    BNM_CustomField(EffectEntityId,BNM::Defaults::Get<int>(),"EffectEntityId");
    void Construct(){Core::NativeConstruction::Base(this);SourceId=-1;EffectEntityId=-1;}
    BNM_CustomMethod(Construct,false,BNM::Defaults::Get<void>(),".ctor");
    void DeepCopyFrom(Obj* source,Obj*){auto* other=reinterpret_cast<Cause*>(source);SourceId=other?other->SourceId:-1;EffectEntityId=other?other->EffectEntityId:-1;}
    void HandleReturnToPool(Obj*){SourceId=-1;EffectEntityId=-1;}
    BNM_CustomMethod(DeepCopyFrom,false,BNM::Defaults::Get<void>(),"DeepCopyFrom",BNM::Defaults::Get<Obj*>(),BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
    BNM_CustomMethod(HandleReturnToPool,false,BNM::Defaults::Get<void>(),"HandleReturnToPool",BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
};
struct Step:Obj {
    std::uint8_t inherited[0x38]{};int Stage=0,Health=1;
    BNM_CustomClass(Step,BNM::CompileTimeClassBuilder("PvZCards.Engine.Effects","PreDiscardStepEffect","EngineLib.dll").Build(),BNM::CompileTimeClassBuilder("PvZCards.Engine.Effects","StateCheckEffect","EngineLib.dll").Build(),{});
    BNM_CustomField(Stage,BNM::Defaults::Get<int>(),"Stage");
    BNM_CustomField(Health,BNM::Defaults::Get<int>(),"Health");
    void Construct(){Core::NativeConstruction::Base(this);}
    BNM_CustomMethod(Construct,false,BNM::Defaults::Get<void>(),".ctor");
    void DeepCopyFrom(Obj* other,Obj* instances){BNM_CallCustomMethodOrigin(DeepCopyFrom,this,other,instances);for(auto name:{"Stage","Health"}){auto field=BNM::Class(this).GetField(name).cast<int>();if(field.IsValid())Core::Metadata::Bind(field,static_cast<Obj*>(this)).Set(other?Core::Metadata::Bind(field,other).Get():0);}}
    void HandleReturnToPool(Obj* instances){for(auto name:{"Stage","Health"}){auto field=BNM::Class(this).GetField(name).cast<int>();if(field.IsValid())Core::Metadata::Bind(field,static_cast<Obj*>(this)).Set(std::string_view(name)=="Health"?1:0);}BNM_CallCustomMethodOrigin(HandleReturnToPool,this,instances);}
    BNM_CustomMethod(DeepCopyFrom,false,BNM::Defaults::Get<void>(),"DeepCopyFrom",BNM::Defaults::Get<Obj*>(),BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
    BNM_CustomMethod(HandleReturnToPool,false,BNM::Defaults::Get<void>(),"HandleReturnToPool",BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
};
struct CancelDescriptor:Obj {
    std::uint8_t inherited[0x10]{};int Health=1;
    BNM_CustomClass(CancelDescriptor,BNM::CompileTimeClassBuilder("PvZCards.Engine.Components","CancelDeathEffectDescriptor","EngineLib.dll").Build(),BNM::CompileTimeClassBuilder("PvZCards.Engine.Components","SetStatEffectDescriptor","EngineLib.dll").Build(),{});
    BNM_CustomField(Health,BNM::Defaults::Get<int>(),"Health");
    void Construct(){Core::NativeConstruction::Base(this);auto field=BNM::Class(this).GetField("Health").cast<int>();if(field.IsValid())Core::Metadata::Bind(field,static_cast<Obj*>(this)).Set(1);}
    BNM_CustomMethod(Construct,false,BNM::Defaults::Get<void>(),".ctor");
    void DeepCopyFrom(Obj* other,Obj* instances){BNM_CallCustomMethodOrigin(DeepCopyFrom,this,other,instances);auto field=BNM::Class(this).GetField("Health").cast<int>();if(field.IsValid())Core::Metadata::Bind(field,static_cast<Obj*>(this)).Set(other?Core::Metadata::Bind(field,other).Get():1);}
    void HandleReturnToPool(Obj* instances){auto field=BNM::Class(this).GetField("Health").cast<int>();if(field.IsValid())Core::Metadata::Bind(field,static_cast<Obj*>(this)).Set(1);BNM_CallCustomMethodOrigin(HandleReturnToPool,this,instances);}
    BNM_CustomMethod(DeepCopyFrom,false,BNM::Defaults::Get<void>(),"DeepCopyFrom",BNM::Defaults::Get<Obj*>(),BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
    BNM_CustomMethod(HandleReturnToPool,false,BNM::Defaults::Get<void>(),"HandleReturnToPool",BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
    Obj* ToEffect(Obj* instances,Obj*){
        Obj* effect=Core::ManagedPool::Acquire(instances,BNM::Class("PvZCards.Engine.Effects","PreDiscardStepEffect"));
        auto stage=BNM::Class("PvZCards.Engine.Effects","PreDiscardStepEffect").GetField("Stage").cast<int>();
        auto health=BNM::Class("PvZCards.Engine.Effects","PreDiscardStepEffect").GetField("Health").cast<int>();
        auto input=BNM::Class(this).GetField("Health").cast<int>();
        if(!effect || !stage.IsValid() || !health.IsValid() || !input.IsValid())return nullptr;
        Core::Metadata::Bind(stage,effect).Set(2);Core::Metadata::Bind(health,effect).Set(std::max(1,Core::Metadata::Bind(input,static_cast<Obj*>(this)).Get()));return effect;
    }
    BNM_CustomMethod(ToEffect,false,BNM::CompileTimeClassBuilder("PvZCards.Engine","Effect","EngineLib.dll").Build(),"ToEffect",BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build(),BNM::CompileTimeClassBuilder("PvZCards.Engine","Entity","EngineLib.dll").Build());
};
inline BNM::Field<int> target,source,origin,stage,requested;
inline BNM::Field<bool> validation;
inline BNM::Field<Obj*> discarderModel;
inline void(*originalDiscard)(Obj*,Obj*,const void*)=nullptr;
inline bool Listeners(Obj* model){
    auto api=Engine::EntityAccess::Api();auto* entities=Core::Metadata::Bind(api.entities,model).Get();if(!Engine::EntityAccess::Valid(entities))return false;
    for(int i=0;i<entities->size;++i)if(Engine::EntityAccess::Component(entities->items->m_Items[i],"PreDiscardFromPlayTrigger"))return true;
    return false;
}
inline void Remove(Obj* card,const char* name){
    static auto remove=BNM::Class("PvZCards.Engine","Entity").GetMethod("RemoveComponent",1).cast<Obj*>();
    auto type=BNM::Class("PvZCards.Engine.Components",name);
    if(card && type.IsValid() && remove.IsValid() && Engine::EntityAccess::Component(card,name))if(Obj* component=Core::Metadata::Bind(remove,card)(type.GetMonoType()))Core::ManagedPool::Return(Engine::Match::Instances(card),component);
}
inline Obj* NewStep(Obj* instances,Obj* cause,int card,int phase){
    Obj* effect=Core::ManagedPool::Acquire(instances,BNM::Class("PvZCards.Engine.Effects","PreDiscardStepEffect"));
    if(!effect)return nullptr;
    static auto copy=BNM::Class("PvZCards.Engine","Effect").GetMethod("DeepCopyFrom",2).cast<void>();
    if(cause && copy.IsValid())Core::Metadata::Bind(copy,effect)(cause,instances);
    Core::Metadata::Bind(target,effect).Set(card);Core::Metadata::Bind(stage,effect).Set(phase);Core::Metadata::Bind(validation,effect).Set(false);return effect;
}
inline void Fatal(const Engine::EffectGuards::Context& context){
    Obj* effect=context.effect;if(!effect || !effect->klass)return;
    auto name=std::string_view(effect->klass->name);if(name!="DamageEffect" && name!="BuffEffect" && name!="DestroyCardEffect" && name!="RedirectedDamageEffect")return;
    Obj* model=Core::ModelRegistry::Get(Engine::RouterContext::RouterModel(context.router));
    Obj* card=Engine::EntityAccess::Find(model,Core::Metadata::Bind(target,effect).Get());
    if(!Engine::EntityAccess::Component(card,"LaneElement"))return;
    Obj* health=Engine::EntityAccess::Component(card,"Health");
    static auto calculate=BNM::Class("PvZCards.Engine.Components","Health").GetMethod("CalculateHealth",0).cast<int>();
    if(!Engine::EntityAccess::Component(card,"MarkedForDeath") && (!health || !calculate.IsValid() || Core::Metadata::Bind(calculate,health)()>0))return;
    if(!Listeners(model))return;
    Obj* saved=Engine::Match::Component(card,BNM::Class("PvZCards.Engine.Components","PendingDeathCause").GetMonoType(),true);
    if(!saved)return;
    auto a=BNM::Class(saved).GetField("SourceId").cast<int>();auto b=BNM::Class(saved).GetField("EffectEntityId").cast<int>();
    if(a.IsValid() && b.IsValid()){Core::Metadata::Bind(a,saved).Set(Core::Metadata::Bind(source,effect).Get());Core::Metadata::Bind(b,saved).Set(Core::Metadata::Bind(origin,effect).Get());}
}
inline void Discard(Obj* system,Obj* card,const void* method){
    Obj* model=system?Core::Metadata::Bind(discarderModel,system).Get():nullptr;
    if(!model || !Engine::EntityAccess::Component(card,"MarkedForDeath") || !Listeners(model)){originalDiscard(system,card,method);return;}
    if(Engine::EntityAccess::Component(card,"PreDiscardPending"))return;
    Obj* instances=Engine::Match::Instances(card);const int id=Engine::Match::Id(card);
    Obj* effect=NewStep(instances,Engine::EffectGuards::current.effect,id,0);Core::ManagedRuntime::Root root(effect);
    if(!effect){originalDiscard(system,card,method);return;}
    if(Obj* saved=Engine::EntityAccess::Component(card,"PendingDeathCause")){
        auto a=BNM::Class(saved).GetField("SourceId").cast<int>();auto b=BNM::Class(saved).GetField("EffectEntityId").cast<int>();
        if(a.IsValid() && b.IsValid()){Core::Metadata::Bind(source,effect).Set(Core::Metadata::Bind(a,saved).Get());Core::Metadata::Bind(origin,effect).Set(Core::Metadata::Bind(b,saved).Get());}
    }
    if(!Engine::Match::Component(card,BNM::Class("PvZCards.Engine.Components","PreDiscardPending").GetMonoType(),true) || !Engine::EffectContinuations::Schedule(model,instances,{effect})){Remove(card,"PreDiscardPending");originalDiscard(system,card,method);}
}
inline bool Prepare(const Engine::EffectGuards::Context& context){
    Obj* effect=context.effect;if(!effect || !effect->klass || std::string_view(effect->klass->name)!="PreDiscardStepEffect")return true;
    Obj* model=Core::ModelRegistry::Get(Engine::RouterContext::RouterModel(context.router));const int id=Core::Metadata::Bind(target,effect).Get();
    Obj* card=Engine::EntityAccess::Find(model,id);Obj* instances=Engine::Match::Instances(card);if(!card || !instances)return false;
    const int phase=Core::Metadata::Bind(stage,effect).Get();
    if(phase==0){
        if(!Engine::EntityAccess::Component(card,"MarkedForDeath") || !Engine::EntityAccess::Component(card,"LaneElement")){Remove(card,"PreDiscardPending");return true;}
        const bool fired=Systems::CustomTriggerSystem::FireInModel("PreDiscardFromPlayTrigger",effect,model);
        LOG_INFO("CustomComponents/PreDiscard/BEGIN: card=%d trigger=%d",id,int(fired));
        Obj* continuation=NewStep(instances,effect,id,1);Core::ManagedRuntime::Root root(continuation);
        if(!continuation || !Engine::EffectContinuations::Schedule(model,instances,{continuation}))LOG_ERR("CustomComponents/PreDiscard: continuation failed card=%d",id);
    }else if(phase==1){
        Remove(card,"PreDiscardPending");Remove(card,"PendingDeathCause");
        if(Engine::EntityAccess::Component(card,"MarkedForDeath") && Engine::EntityAccess::Component(card,"LaneElement")){
            auto service=Engine::Match::Service(instances,BNM::Class("PvZCards.Core.Systems.Utilities","CardInPlayDiscarder"));if(service)originalDiscard(service->Get(),card,nullptr);
        }
    }else if(phase==2){
        if(!Engine::EntityAccess::Component(card,"PreDiscardPending") || !Engine::EntityAccess::Component(card,"LaneElement"))return true;
        const bool markedBefore=Engine::EntityAccess::Component(card,"MarkedForDeath")!=nullptr;
        Remove(card,"MarkedForDeath");
        bool recorded=false;
        Obj* health=Engine::EntityAccess::Component(card,"Health");
        static auto damage=BNM::Class("PvZCards.Engine.Components","Health").GetField("CurrentDamage").cast<int>();
        static auto maxField=BNM::Class("PvZCards.Engine.Components","Health").GetField("MaxHealth").cast<Obj*>();
        static auto calculate=BNM::Class("PvZCards.Engine.Utilities","ModifiableIntCalculator").GetMethod("CalculateValue",1).cast<int>();
        if(health && damage.IsValid() && maxField.IsValid() && calculate.IsValid()){
            Obj* value=Core::Metadata::Bind(maxField,health).Get();const int maximum=value?calculate(value):0;
            const int restored=std::clamp(Core::Metadata::Bind(requested,effect).Get(),1,std::max(1,maximum));
            const int before=Core::Metadata::Bind(damage,health).Get();const int after=maximum-restored;Core::Metadata::Bind(damage,health).Set(after);
            if(before>after){
                Obj* heal=Core::ManagedPool::Acquire(instances,BNM::Class("PvZCards.Engine.Effects","HealEffect"));Core::ManagedRuntime::Root root(heal);
                static auto copy=BNM::Class("PvZCards.Engine","Effect").GetMethod("DeepCopyFrom",2).cast<void>();
                static auto record=BNM::Class("PvZCards.Core.Systems","HealSystem").GetMethod("RecordTargetHealed",2).cast<void>();
                auto service=Engine::Match::Service(instances,BNM::Class("PvZCards.Core.Systems","HealSystem"));
                if(heal && copy.IsValid() && record.IsValid() && service){Core::Metadata::Bind(copy,heal)(effect,instances);Core::Metadata::Bind(record,service->Get())(heal,before-after);}
                if(heal)Core::ManagedPool::Return(instances,heal);
            }
            History::Domain::Emit(model,History::Kind::DeathCancelled,
                History::DeathPayload{id,before,after,restored,markedBefore && !Engine::EntityAccess::Component(card,"MarkedForDeath")},id);
            recorded=true;
            Engine::SimulationChanges::Changed(model);
            LOG_INFO("CustomComponents/PreDiscard/CANCEL: card=%d health=%d",id,restored);
        }
        if(!recorded && markedBefore && !Engine::EntityAccess::Component(card,"MarkedForDeath"))
            History::Domain::Emit(model,History::Kind::DeathCancelled,History::DeathPayload{id,-1,-1,-1,true},id);
    }
    return true;
}
inline bool Initialize(){
    for(auto name:{"PreDiscardFromPlayTrigger","PreDiscardPending","PendingDeathCause"})if(!CustomComponentRegistry::Register({name,"PvZCards.Engine.Components","EngineLib.dll","MarkerComponent","PvZCards.Engine.Components","EngineLib.dll"}))return false;
    auto marker=BNM::Class("PvZCards.Engine.Components","MarkerComponent");auto stepType=BNM::Class("PvZCards.Engine.Effects","PreDiscardStepEffect");
    auto cancel=BNM::Class("PvZCards.Engine.Components","CancelDeathEffectDescriptor");
    if(!Core::ComponentLifecycle::Patch(BNM::Class("PvZCards.Engine.Components","PendingDeathCause"),marker) || !CustomComponentRegistry::Register({"PreDiscardStepEffect","PvZCards.Engine.Effects","EngineLib.dll","StateCheckEffect","PvZCards.Engine.Effects","EngineLib.dll"}) || !Core::ComponentLifecycle::Patch(stepType,BNM::Class("PvZCards.Engine.Effects","StateCheckEffect")) || !CustomComponentRegistry::Register({"CancelDeathEffectDescriptor","PvZCards.Engine.Components","EngineLib.dll","SetStatEffectDescriptor","PvZCards.Engine.Components","EngineLib.dll"}) || !Core::ComponentLifecycle::Patch(cancel,BNM::Class("PvZCards.Engine.Components","SetStatEffectDescriptor")))return false;
    if(!Core::ComponentLifecycle::PatchVirtual(cancel, BNM::Class("PvZCards.Engine.Components","SetStatEffectDescriptor"), BNM::Class("PvZCards.Engine.Components","EffectDescriptor"), "ToEffect", 2))return false;
    using K=Systems::CustomFieldBridgeSystem::FieldKind;
    if(!Systems::CustomFieldBridgeSystem::Register("PvZCards.Engine.Components","CancelDeathEffectDescriptor","PvZCards.Engine.Components","SetStatEffectDescriptor",{{"Health",K::Int32}},false))return false;
    auto base=BNM::Class("PvZCards.Engine","Effect");target=base.GetField("TargetId").cast<int>();source=base.GetField("SourceId").cast<int>();origin=base.GetField("EffectEntityId").cast<int>();validation=base.GetField("RequiresTargetValidation").cast<bool>();stage=stepType.GetField("Stage").cast<int>();requested=stepType.GetField("Health").cast<int>();
    auto discarder=BNM::Class("PvZCards.Core.Systems.Utilities","CardInPlayDiscarder");discarderModel=discarder.GetField("<Entities>k__BackingField").cast<Obj*>();
    return target.IsValid() && source.IsValid() && origin.IsValid() && validation.IsValid() && stage.IsValid() && requested.IsValid() && discarderModel.IsValid() && Engine::EffectContinuations::Initialize() && Core::Hooks::Install(discarder.GetMethod("CreateAndQueueDiscardEffect",1),Discard,originalDiscard) && Engine::EffectGuards::RegisterPreparation(Prepare,-27) && Engine::EffectGuards::after.Add(Fatal);
}
}
