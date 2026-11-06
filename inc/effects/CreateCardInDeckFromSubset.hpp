#include "effects/SubsetCreationPolicy.hpp"
#pragma once
#include <BNM/ClassesManagement.hpp>
#include <BNM/Defaults.hpp>
#include "core/ManagedQuery.hpp"
#include "engine/EffectRegistration.hpp"
#include "engine/DeckCreation.hpp"
#include "engine/MatchAccess.hpp"
#include "core/CustomComponentRegistry.hpp"
namespace CustomComponents::Effects::CreateDeckSubset {
using Obj=BNM::IL2CPP::Il2CppObject;
inline auto QueryType(){return BNM::CompileTimeClassBuilder("PvZCards.Engine","EntityQuery","EngineLib.dll").Build();}
struct Runtime:Obj {
    unsigned char inherited[0x48]{};Obj* SubsetQuery=nullptr;
    BNM_CustomClass(Runtime,BNM::CompileTimeClassBuilder("PvZCards.Engine.Effects","CreateCardInDeckFromSubsetEffect","EngineLib.dll").Build(),BNM::CompileTimeClassBuilder("PvZCards.Engine.Effects","CreateCardInDeckEffect","EngineLib.dll").Build(),{});
    BNM_CustomField(SubsetQuery,QueryType(),"SubsetQuery");
    void DeepCopyFrom(Obj* source,Obj* instances) {
        BNM_CallCustomMethodOrigin(DeepCopyFrom,this,source,instances);
        if(!Core::ManagedQuery::CopyOwnedField(this,BNM::Class(this).GetField("SubsetQuery").cast<Obj*>(),reinterpret_cast<Runtime*>(source)->SubsetQuery,instances))
        {
            Core::ManagedQuery::ClearOwnedField(this,BNM::Class(this).GetField("SubsetQuery").cast<Obj*>(),instances);
            BNM::Class("PvZCards.Engine.Effects","CreateCardInDeckEffect").GetField("AmountToCreate").cast<int>()[static_cast<Obj*>(this)].Set(0);
            LOG_ERR("CustomComponents/CreateDeckSubset: runtime query copy rejected");
        }
    }
    BNM_CustomMethod(DeepCopyFrom,false,BNM::Defaults::Get<void>(),"DeepCopyFrom",BNM::Defaults::Get<Obj*>(),BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
    void HandleReturnToPool(Obj* instances) {
        Core::ManagedQuery::ClearOwnedField(this,BNM::Class(this).GetField("SubsetQuery").cast<Obj*>(),instances);
        BNM_CallCustomMethodOrigin(HandleReturnToPool,this,instances);
    }
    BNM_CustomMethod(HandleReturnToPool,false,BNM::Defaults::Get<void>(),"HandleReturnToPool",BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
};
struct Descriptor:Obj {
    unsigned char inherited[0x10]{};Obj* SubsetQuery=nullptr;
    BNM_CustomClass(Descriptor,BNM::CompileTimeClassBuilder("PvZCards.Engine.Components","CreateCardInDeckFromSubsetEffectDescriptor","EngineLib.dll").Build(),BNM::CompileTimeClassBuilder("PvZCards.Engine.Components","CreateCardInDeckEffectDescriptor","EngineLib.dll").Build(),{});
    BNM_CustomField(SubsetQuery,QueryType(),"SubsetQuery");
    void Construct() {
        BNM_CallCustomMethodOrigin(Construct,this);
        auto type=BNM::Class("PvZCards.Engine.Components","CreateCardInDeckEffectDescriptor");
        type.GetField("AmountToCreate").cast<int>()[static_cast<Obj*>(this)].Set(1);type.GetField("DeckPosition").cast<int>()[static_cast<Obj*>(this)].Set(0);
    }
    BNM_CustomMethod(Construct,false,BNM::Defaults::Get<void>(),".ctor");
    Obj* ToEffect(Obj* instances,Obj*) {
        if(!instances || !SubsetQuery)return nullptr;
        Obj* effect=Core::ManagedPool::Acquire(instances,BNM::Class("PvZCards.Engine.Effects","CreateCardInDeckFromSubsetEffect"));
        Core::ManagedRuntime::Root root(effect);if(!root.Get())return nullptr;
        auto from=BNM::Class("PvZCards.Engine.Components","CreateCardInDeckEffectDescriptor");auto to=BNM::Class("PvZCards.Engine.Effects","CreateCardInDeckEffect");
        int amount=from.GetField("AmountToCreate").cast<int>()[static_cast<Obj*>(this)].Get(),position=from.GetField("DeckPosition").cast<int>()[static_cast<Obj*>(this)].Get();
        if(!SubsetCreationPolicy::Valid(amount,position,SubsetQuery!=nullptr) || !Core::ManagedQuery::CopyOwnedField(effect,BNM::Class(effect).GetField("SubsetQuery").cast<Obj*>(),SubsetQuery,instances)) {Core::ManagedPool::Return(instances,effect);return nullptr;}
        to.GetField("AmountToCreate").cast<int>()[effect].Set(amount);to.GetField("DeckPosition").cast<int>()[effect].Set(position);to.GetField("CardGuid").cast<int>()[effect].Set(-1);
        return effect;
    }
    BNM_CustomMethod(ToEffect,false,BNM::CompileTimeClassBuilder("PvZCards.Engine","Effect","EngineLib.dll").Build(),"ToEffect",BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build(),BNM::CompileTimeClassBuilder("PvZCards.Engine","Entity","EngineLib.dll").Build());
    static void Copy(Obj* destination,Obj* source,Obj* instances) {
        auto field=BNM::Class(destination).GetField("SubsetQuery").cast<Obj*>();
        if(!Core::ManagedQuery::CopyOwnedField(destination,field,reinterpret_cast<Descriptor*>(source)->SubsetQuery,instances)) {
            Core::ManagedQuery::ClearOwnedField(destination,field,instances);
            BNM::Class("PvZCards.Engine.Components","CreateCardInDeckEffectDescriptor").GetField("AmountToCreate").cast<int>()[destination].Set(0);
            LOG_ERR("CustomComponents/CreateDeckSubset: descriptor copy rejected");
        }
    }
    static void Release(Obj* self,Obj* instances) {Core::ManagedQuery::ClearOwnedField(self,BNM::Class(self).GetField("SubsetQuery").cast<Obj*>(),instances);}
};
inline void Register(Obj* router,Obj* type,Obj* handler,const void* method) {
    static auto stock=BNM::Class("PvZCards.Engine.Effects","CreateCardInDeckEffect");
    if(type==reinterpret_cast<Obj*>(stock.GetMonoType()))
        Engine::EffectRegistration::Alias(router,reinterpret_cast<Obj*>(BNM::Class("PvZCards.Engine.Effects","CreateCardInDeckFromSubsetEffect").GetMonoType()),handler,method);
}
inline bool Prepare(const Engine::DeckCreation::Context& context,int) {
    static auto custom=BNM::Class("PvZCards.Engine.Effects","CreateCardInDeckFromSubsetEffect");
    if(!context.effect || !BNM::IsA(context.effect,custom))return true;
    auto type=BNM::Class("PvZCards.Core.Systems","CreateCardInDeckSystem");
    auto modelField=type.GetField("<Model>k__BackingField").cast<Obj*>();auto historyField=type.GetField("<History>k__BackingField").cast<Obj*>();
    Obj* model=modelField[context.system].Get();Obj* history=historyField[context.system].Get();
    auto instancesField=BNM::Class("PvZCards.Engine","History").GetField("<Instances>k__BackingField").cast<Obj*>();Obj* instances=history?instancesField[history].Get():nullptr;
    auto stock=BNM::Class("PvZCards.Engine.Effects","CreateCardInDeckEffect");int amount=stock.GetField("AmountToCreate").cast<int>()[context.effect].Get();if(!SubsetCreationPolicy::Execute(amount))return false;
    auto finderType=BNM::Class("PvZCards.Core.Systems.Utilities","CardFromSubsetFinder");auto finderLease=Engine::Match::Service(instances,finderType);Obj* finder=finderLease?finderLease->Get():nullptr;
    if(!finder || finderType.GetField("<Model>k__BackingField").cast<Obj*>()[finder].Get()!=model || finderType.GetField("<Instances>k__BackingField").cast<Obj*>()[finder].Get()!=instances)return false;
    auto find=finderType.GetMethod("FindCard",{BNM::Class("PvZCards.Engine","EntityQuery").GetCompileTimeClass(),BNM::Class("PvZCards.Engine","Effect").GetCompileTimeClass()}).cast<int>();
    Obj* query=reinterpret_cast<Runtime*>(context.effect)->SubsetQuery;if(!query || !find.IsValid())return false;
    int guid=find[finder](query,context.effect);if(guid<=0)return false;
    stock.GetField("CardGuid").cast<int>()[context.effect].Set(guid);return true;
}
inline bool Initialize() {
    if(!CustomComponentRegistry::FindByName("PvZCards.Engine.Components","CreateCardInDeckFromSubsetEffectDescriptor") && !CustomComponentRegistry::Register({"CreateCardInDeckFromSubsetEffectDescriptor","PvZCards.Engine.Components","EngineLib.dll","CreateCardInDeckEffectDescriptor","PvZCards.Engine.Components","EngineLib.dll"}))return false;
    Engine::EffectRegistration::afterRegister.Add(Register);Engine::DeckCreation::preparation.Add(Prepare);
    const bool adapter=Engine::DeckCreation::Register({BNM::Class("PvZCards.Engine.Components","CreateCardInDeckFromSubsetEffectDescriptor"),
        [](Obj* self,Obj* instances,Obj* entity){return reinterpret_cast<Descriptor*>(self)->ToEffect(instances,entity);},Descriptor::Copy,Descriptor::Release});
    return adapter && Engine::DeckCreation::Initialize() && Engine::EffectRegistration::Initialize();
}
}





