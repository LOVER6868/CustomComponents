#include "core/NativeConstruction.hpp"
#include "effects/SubsetCreationPolicy.hpp"
#pragma once
#include "integration/ReplayEventBridge.hpp"
#include "core/SceneViewContext.hpp"
#include <BNM/ClassesManagement.hpp>
#include <BNM/Defaults.hpp>
#include "core/ManagedQuery.hpp"
#include "engine/EffectRegistration.hpp"
#include "engine/DeckCreation.hpp"
#include "engine/MatchAccess.hpp"
#include "engine/BoardPlacement.hpp"
#include "engine/EffectEventContext.hpp"
#include "core/CustomComponentRegistry.hpp"
namespace CustomComponents::Effects::CreateDeckSubset {
using Obj=BNM::IL2CPP::Il2CppObject;
inline auto QueryType(){return BNM::CompileTimeClassBuilder("PvZCards.Engine","EntityQuery","EngineLib.dll").Build();}
inline bool CopySubsetQuery(Obj* destination,Obj* source,Obj* instances){
    auto to=BNM::Class(destination).GetField("SubsetQuery").cast<Obj*>();
    auto from=source?BNM::Class(source).GetField("SubsetQuery").cast<Obj*>():BNM::Field<Obj*>{};
    return to.IsValid() && (!source || from.IsValid()) && Core::ManagedQuery::CopyOwnedField(destination,to,source?Core::Metadata::Bind(from,source).Get():nullptr,instances);
}
struct Runtime:Obj {
    unsigned char inherited[0x48]{};Obj* SubsetQuery=nullptr;
    BNM_CustomClass(Runtime,BNM::CompileTimeClassBuilder("PvZCards.Engine.Effects","CreateCardInDeckFromSubsetEffect","EngineLib.dll").Build(),BNM::CompileTimeClassBuilder("PvZCards.Engine.Effects","CreateCardInDeckEffect","EngineLib.dll").Build(),{});
    BNM_CustomField(SubsetQuery,QueryType(),"SubsetQuery");
    void Construct(){Core::NativeConstruction::Base(this);}
    BNM_CustomMethod(Construct,false,BNM::Defaults::Get<void>(),".ctor");
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
        Core::NativeConstruction::Base(this);
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
inline bool Choose(Obj* system,Obj* effect) {
    auto type=BNM::Class("PvZCards.Core.Systems","CreateCardInDeckSystem");
    auto modelField=type.GetField("<Model>k__BackingField").cast<Obj*>();auto historyField=type.GetField("<History>k__BackingField").cast<Obj*>();
    Obj* model=modelField[system].Get();Obj* history=historyField[system].Get();
    auto instancesField=BNM::Class("PvZCards.Engine","History").GetField("<Instances>k__BackingField").cast<Obj*>();Obj* instances=history?instancesField[history].Get():nullptr;
    auto stock=BNM::Class("PvZCards.Engine.Effects","CreateCardInDeckEffect");int amount=stock.GetField("AmountToCreate").cast<int>()[effect].Get();if(!SubsetCreationPolicy::Execute(amount))return false;
    auto effectId=BNM::Class("PvZCards.Engine","Effect").GetField("EffectEntityId").cast<int>();
    if(!effectId.IsValid())return false;
    Obj* entity=Engine::EntityAccess::Find(model,Core::Metadata::Bind(effectId,effect).Get());
    Obj* event=Engine::EffectEventContext::Acquire(effect,instances,entity);Core::ManagedRuntime::Root root(event);
    if(!root.Get())return false;
    struct Release {Obj* instances;Obj* event;~Release(){Core::ManagedPool::Return(instances,event);}} release{instances,event};
    auto queryField=BNM::Class(effect).GetField("SubsetQuery").cast<Obj*>();
    Obj* query=queryField.IsValid()?Core::Metadata::Bind(queryField,effect).Get():nullptr;
    auto find=BNM::Class("PvZCards.Core.Utilities","CardDatabase").GetMethod("FindEntities",2).cast<Obj*>();
    Obj* database=Engine::BoardPlacement::Database(entity);
    if(!query || !database || !find.IsValid())return false;
    Obj* found=Core::Metadata::Bind(find,database)(query,event);Core::ManagedRuntime::Root foundRoot(found);
    auto* cards=reinterpret_cast<Engine::EntityAccess::List*>(found);
    if(!Engine::EntityAccess::Valid(cards))return false;
    const int selected=Engine::Match::Roll(entity,cards->size);
    const int guid=selected<0?-1:Engine::BoardPlacement::Guid(cards->items->m_Items[selected]);
    LOG_INFO("CustomComponents/CreateDeckSubset: effect=%d candidates=%d selectedGuid=%d",Engine::Match::Id(entity),cards->size,guid);
    if(guid<=0)return false;
    // This is a validated choice, NOT proof CreateCardInDeck committed.
    if (reinterpret_cast<std::uintptr_t>(model)==Core::SceneViewContext::ModelIdentity())
        Integration::ReplayEventBridge::Publish("deck_subset_choice", {
            {"effectEntityId",Engine::Match::Id(entity)},
            {"selectedGuid",guid},{"candidateCount",cards->size},
            {"modelCommitProven",0}});
    stock.GetField("CardGuid").cast<int>()[effect].Set(guid);return true;
}
inline bool Prepare(const Engine::DeckCreation::Context& context,int) {
    static auto custom=BNM::Class("PvZCards.Engine.Effects","CreateCardInDeckFromSubsetEffect");
    return !context.effect || !BNM::IsA(context.effect,custom) || Choose(context.system,context.effect);
}
inline bool Initialize() {
    if(!CustomComponentRegistry::FindByName("PvZCards.Engine.Components","CreateCardInDeckFromSubsetEffectDescriptor") && !CustomComponentRegistry::Register({"CreateCardInDeckFromSubsetEffectDescriptor","PvZCards.Engine.Components","EngineLib.dll","CreateCardInDeckEffectDescriptor","PvZCards.Engine.Components","EngineLib.dll"}))return false;
    Engine::EffectRegistration::afterRegister.Add(Register);Engine::DeckCreation::preparation.Add(Prepare);
    const bool adapter=Engine::DeckCreation::Register({BNM::Class("PvZCards.Engine.Components","CreateCardInDeckFromSubsetEffectDescriptor"),
        [](Obj* self,Obj* instances,Obj* entity){return reinterpret_cast<Descriptor*>(self)->ToEffect(instances,entity);},Descriptor::Copy,Descriptor::Release});
    return adapter && Engine::DeckCreation::Initialize() && Engine::EffectRegistration::Initialize();
}
}





