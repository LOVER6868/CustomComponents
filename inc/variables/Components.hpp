#pragma once
#include "variables/Storage.hpp"
#include "core/ManagedQuery.hpp"
#include "engine/AbilityConversion.hpp"
namespace CustomComponents::Variables {
inline auto QueryType(){return BNM::CompileTimeClassBuilder("PvZCards.Engine","EntityQuery","EngineLib.dll").Build();}
struct Runtime:Obj {
    unsigned char inherited[0x40]{};
    Str* Variable=nullptr;
    Str* Operation=nullptr;
    Str* ValueType=nullptr;
    Str* Expression=nullptr;
    Str* ValueSource=nullptr;
    Str* Owner=nullptr;
    Str* ActiveZones=nullptr;
    int Value=0;
    Obj* Query=nullptr;Obj* OwnerQuery=nullptr;
    BNM_CustomClass(Runtime,BNM::CompileTimeClassBuilder("PvZCards.Engine.Effects","SetVariableEffect","EngineLib.dll").Build(),BNM::CompileTimeClassBuilder("PvZCards.Engine.Effects","GrantAbilityEffect","EngineLib.dll").Build(),{});
    void Construct(){BNM_CallCustomMethodOrigin(Construct,this);}
    BNM_CustomMethod(Construct,false,BNM::Defaults::Get<void>(),".ctor");
    BNM_CustomField(Variable,BNM::Defaults::Get<Str*>(),"Variable");
    BNM_CustomField(Operation,BNM::Defaults::Get<Str*>(),"Operation");
    BNM_CustomField(ValueType,BNM::Defaults::Get<Str*>(),"ValueType");
    BNM_CustomField(Expression,BNM::Defaults::Get<Str*>(),"Expression");
    BNM_CustomField(ValueSource,BNM::Defaults::Get<Str*>(),"ValueSource");
    BNM_CustomField(Owner,BNM::Defaults::Get<Str*>(),"Owner");
    BNM_CustomField(ActiveZones,BNM::Defaults::Get<Str*>(),"ActiveZones");
    BNM_CustomField(Value,BNM::Defaults::Get<int>(),"Value");
    BNM_CustomField(Query,QueryType(),"Query");
    BNM_CustomField(OwnerQuery,QueryType(),"OwnerQuery");
    void DeepCopyFrom(Obj* source,Obj* instances){BNM_CallCustomMethodOrigin(DeepCopyFrom,this,source,instances);auto* from=reinterpret_cast<Runtime*>(source);
        Reference(this,&Variable,from?from->Variable:nullptr);
        Reference(this,&Operation,from?from->Operation:nullptr);
        Reference(this,&ValueType,from?from->ValueType:nullptr);
        Reference(this,&Expression,from?from->Expression:nullptr);
        Reference(this,&ValueSource,from?from->ValueSource:nullptr);
        Reference(this,&Owner,from?from->Owner:nullptr);
        Reference(this,&ActiveZones,from?from->ActiveZones:nullptr);
        Value=from?from->Value:0;
        Core::ManagedQuery::CopyOwnedField(this,BNM::Class(this).GetField("Query").cast<Obj*>(),from?from->Query:nullptr,instances);Core::ManagedQuery::CopyOwnedField(this,BNM::Class(this).GetField("OwnerQuery").cast<Obj*>(),from?from->OwnerQuery:nullptr,instances);}
    void HandleReturnToPool(Obj* instances){
        Reference(this,&Variable,nullptr);
        Reference(this,&Operation,nullptr);
        Reference(this,&ValueType,nullptr);
        Reference(this,&Expression,nullptr);
        Reference(this,&ValueSource,nullptr);
        Reference(this,&Owner,nullptr);
        Reference(this,&ActiveZones,nullptr);
        Value=0;
        Core::ManagedQuery::ClearOwnedField(this,BNM::Class(this).GetField("Query").cast<Obj*>(),instances);Core::ManagedQuery::ClearOwnedField(this,BNM::Class(this).GetField("OwnerQuery").cast<Obj*>(),instances);BNM_CallCustomMethodOrigin(HandleReturnToPool,this,instances);}
    BNM_CustomMethod(DeepCopyFrom,false,BNM::Defaults::Get<void>(),"DeepCopyFrom",BNM::Defaults::Get<Obj*>(),BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
    BNM_CustomMethod(HandleReturnToPool,false,BNM::Defaults::Get<void>(),"HandleReturnToPool",BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
};
struct Descriptor:Obj {
    unsigned char inherited[0x10]{};
    Str* Variable=nullptr;
    Str* Operation=nullptr;
    Str* ValueType=nullptr;
    Str* Expression=nullptr;
    Str* ValueSource=nullptr;
    Str* Owner=nullptr;
    Str* ActiveZones=nullptr;
    int Value=0;
    Obj* Query=nullptr;Obj* OwnerQuery=nullptr;
    BNM_CustomClass(Descriptor,BNM::CompileTimeClassBuilder("PvZCards.Engine.Components","SetVariableEffectDescriptor","EngineLib.dll").Build(),BNM::CompileTimeClassBuilder("PvZCards.Engine.Components","GrantAbilityEffectDescriptor","EngineLib.dll").Build(),{});
    BNM_CustomField(Variable,BNM::Defaults::Get<Str*>(),"Variable");
    BNM_CustomField(Operation,BNM::Defaults::Get<Str*>(),"Operation");
    BNM_CustomField(ValueType,BNM::Defaults::Get<Str*>(),"ValueType");
    BNM_CustomField(Expression,BNM::Defaults::Get<Str*>(),"Expression");
    BNM_CustomField(ValueSource,BNM::Defaults::Get<Str*>(),"ValueSource");
    BNM_CustomField(Owner,BNM::Defaults::Get<Str*>(),"Owner");
    BNM_CustomField(ActiveZones,BNM::Defaults::Get<Str*>(),"ActiveZones");
    BNM_CustomField(Value,BNM::Defaults::Get<int>(),"Value");
    BNM_CustomField(Query,QueryType(),"Query");
    BNM_CustomField(OwnerQuery,QueryType(),"OwnerQuery");
    Obj* Removal(Obj* instances,Obj*){Obj* effect=Core::ManagedPool::Acquire(instances,BNM::Class("PvZCards.Engine.Effects","SetVariableEffect"));if(effect)Reference(effect,&reinterpret_cast<Runtime*>(effect)->Operation,BNM::CreateMonoString("NoOp"));return effect;}
    BNM_CustomMethod(Removal,false,BNM::CompileTimeClassBuilder("PvZCards.Engine","Effect","EngineLib.dll").Build(),"ToRemovalEffect",BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build(),BNM::CompileTimeClassBuilder("PvZCards.Engine","Entity","EngineLib.dll").Build());
    void DeepCopyFrom(Obj* source,Obj* instances){BNM_CallCustomMethodOrigin(DeepCopyFrom,this,source,instances);auto* from=reinterpret_cast<Descriptor*>(source);
        Reference(this,&Variable,from?from->Variable:nullptr);
        Reference(this,&Operation,from?from->Operation:nullptr);
        Reference(this,&ValueType,from?from->ValueType:nullptr);
        Reference(this,&Expression,from?from->Expression:nullptr);
        Reference(this,&ValueSource,from?from->ValueSource:nullptr);
        Reference(this,&Owner,from?from->Owner:nullptr);
        Reference(this,&ActiveZones,from?from->ActiveZones:nullptr);
        Value=from?from->Value:0;
        Core::ManagedQuery::CopyOwnedField(this,BNM::Class(this).GetField("Query").cast<Obj*>(),from?from->Query:nullptr,instances);Core::ManagedQuery::CopyOwnedField(this,BNM::Class(this).GetField("OwnerQuery").cast<Obj*>(),from?from->OwnerQuery:nullptr,instances);}
    void HandleReturnToPool(Obj* instances){
        Reference(this,&Variable,nullptr);
        Reference(this,&Operation,nullptr);
        Reference(this,&ValueType,nullptr);
        Reference(this,&Expression,nullptr);
        Reference(this,&ValueSource,nullptr);
        Reference(this,&Owner,nullptr);
        Reference(this,&ActiveZones,nullptr);
        Value=0;
        Core::ManagedQuery::ClearOwnedField(this,BNM::Class(this).GetField("Query").cast<Obj*>(),instances);Core::ManagedQuery::ClearOwnedField(this,BNM::Class(this).GetField("OwnerQuery").cast<Obj*>(),instances);BNM_CallCustomMethodOrigin(HandleReturnToPool,this,instances);}
    BNM_CustomMethod(DeepCopyFrom,false,BNM::Defaults::Get<void>(),"DeepCopyFrom",BNM::Defaults::Get<Obj*>(),BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
    BNM_CustomMethod(HandleReturnToPool,false,BNM::Defaults::Get<void>(),"HandleReturnToPool",BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
};
inline bool Convert(Obj* object,Obj* instances,Obj*,Obj*& result){
    if(!object || !object->klass || std::string_view(object->klass->name)!="SetVariableEffectDescriptor")return false;
    result=Core::ManagedPool::Acquire(instances,BNM::Class("PvZCards.Engine.Effects","SetVariableEffect"));if(!result)return true;
    auto* from=reinterpret_cast<Descriptor*>(object);auto* to=reinterpret_cast<Runtime*>(result);
    Reference(to,&to->Variable,from->Variable);
    Reference(to,&to->Operation,from->Operation);
    Reference(to,&to->ValueType,from->ValueType);
    Reference(to,&to->Expression,from->Expression);
    Reference(to,&to->ValueSource,from->ValueSource);
    Reference(to,&to->Owner,from->Owner);
    Reference(to,&to->ActiveZones,from->ActiveZones);
    to->Value=from->Value;
    if(!Core::ManagedQuery::CopyOwnedField(to,BNM::Class(to).GetField("Query").cast<Obj*>(),from->Query,instances) || !Core::ManagedQuery::CopyOwnedField(to,BNM::Class(to).GetField("OwnerQuery").cast<Obj*>(),from->OwnerQuery,instances)){Core::ManagedPool::Return(instances,result);result=nullptr;}return true;
}
inline bool InitializeDescriptors(){
    using K=Bridge::FieldKind;
    if(!CustomComponentRegistry::Register({"SetVariableEffect","PvZCards.Engine.Effects","EngineLib.dll","GrantAbilityEffect","PvZCards.Engine.Effects","EngineLib.dll"}))return false;
    if(!CustomComponentRegistry::Register({"SetVariableEffectDescriptor","PvZCards.Engine.Components","EngineLib.dll","GrantAbilityEffectDescriptor","PvZCards.Engine.Components","EngineLib.dll"}))return false;
    for(const auto& pair:{std::pair{"PvZCards.Engine.Components","SetVariableEffectDescriptor"},std::pair{"PvZCards.Engine.Effects","SetVariableEffect"}}){
        const char* base=std::string_view(pair.second)=="SetVariableEffect"?"GrantAbilityEffect":"GrantAbilityEffectDescriptor";
        if(!Core::ComponentLifecycle::Patch(BNM::Class(pair.first,pair.second),BNM::Class(pair.first,base)))return false;
    }
    if(!Core::ComponentLifecycle::PatchVirtual(BNM::Class("PvZCards.Engine.Components","SetVariableEffectDescriptor"),BNM::Class("PvZCards.Engine.Components","GrantAbilityEffectDescriptor"),BNM::Class("PvZCards.Engine.Components","RemovableEffectDescriptor"),"ToRemovalEffect",2))return false;
    return Engine::AbilityConversion::Add(Convert) && Bridge::Register("PvZCards.Engine.Components","SetVariableEffectDescriptor","PvZCards.Engine.Components","GrantAbilityEffectDescriptor",{{"Variable",K::String},{"Operation",K::String},{"ValueType",K::String},{"Expression",K::String},{"ValueSource",K::String},{"Owner",K::String},{"ActiveZones",K::String},{"Value",K::Int32}},false);
}
}
