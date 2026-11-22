#pragma once
#include "history/RecordWriter.hpp"
#include "variables/Resolver.hpp"
#include "effects/CreateCardInDeckFromSubset.hpp"
#include "engine/StartingSuperpowers.hpp"
#include "engine/SimulationChanges.hpp"
namespace CustomComponents::Zones::CreateSuperpowerSubset {
using Obj=Engine::Match::Obj;
namespace Deck=Effects::CreateDeckSubset;
struct Runtime : Obj {
    std::uint8_t inherited[0x48]{};Obj* SubsetQuery=nullptr;
    BNM_CustomClass(Runtime,BNM::CompileTimeClassBuilder("PvZCards.Engine.Effects","CreateCardInSuperpowersFromSubsetEffect","EngineLib.dll").Build(),BNM::CompileTimeClassBuilder("PvZCards.Engine.Effects","CreateCardInDeckEffect","EngineLib.dll").Build(),{});
    BNM_CustomField(SubsetQuery,Deck::QueryType(),"SubsetQuery");
    void Construct(){Core::NativeConstruction::Base(this);}
    BNM_CustomMethod(Construct,false,BNM::Defaults::Get<void>(),".ctor");
    void DeepCopyFrom(Obj* source,Obj* instances){
        BNM_CallCustomMethodOrigin(DeepCopyFrom,this,source,instances);
        if(!Deck::CopySubsetQuery(this,source,instances)){
            Core::ManagedQuery::ClearOwnedField(this,BNM::Class(this).GetField("SubsetQuery").cast<Obj*>(),instances);
            auto amount=BNM::Class("PvZCards.Engine.Effects","CreateCardInDeckEffect").GetField("AmountToCreate").cast<int>();
            if(amount.IsValid())Core::Metadata::Bind(amount,static_cast<Obj*>(this)).Set(0);
            LOG_ERR("CustomComponents/Superpowers: runtime subset copy failed");
        }
    }
    void HandleReturnToPool(Obj* instances){
        Core::ManagedQuery::ClearOwnedField(this,BNM::Class(this).GetField("SubsetQuery").cast<Obj*>(),instances);
        BNM_CallCustomMethodOrigin(HandleReturnToPool,this,instances);
    }
    BNM_CustomMethod(DeepCopyFrom,false,BNM::Defaults::Get<void>(),"DeepCopyFrom",BNM::Defaults::Get<Obj*>(),BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
    BNM_CustomMethod(HandleReturnToPool,false,BNM::Defaults::Get<void>(),"HandleReturnToPool",BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
};
struct Descriptor : Obj {
    std::uint8_t inherited[0x10]{};Obj* SubsetQuery=nullptr;
    BNM_CustomClass(Descriptor,BNM::CompileTimeClassBuilder("PvZCards.Engine.Components","CreateCardInSuperpowersFromSubset","EngineLib.dll").Build(),BNM::CompileTimeClassBuilder("PvZCards.Engine.Components","CreateCardInDeckEffectDescriptor","EngineLib.dll").Build(),{});
    BNM_CustomField(SubsetQuery,Deck::QueryType(),"SubsetQuery");
    void Construct(){Core::NativeConstruction::Base(this);static auto amount=BNM::Class("PvZCards.Engine.Components","CreateCardInDeckEffectDescriptor").GetField("AmountToCreate").cast<int>();if(amount.IsValid())Core::Metadata::Bind(amount,static_cast<Obj*>(this)).Set(1);}
    BNM_CustomMethod(Construct,false,BNM::Defaults::Get<void>(),".ctor");
    Obj* ToEffect(Obj* instances,Obj* entity){
        Obj* selected=reinterpret_cast<Deck::Descriptor*>(this)->ToEffect(instances,entity);Core::ManagedRuntime::Root selectedRoot(selected);if(!selected)return nullptr;
        Obj* effect=Core::ManagedPool::Acquire(instances,BNM::Class("PvZCards.Engine.Effects","CreateCardInSuperpowersFromSubsetEffect"));Core::ManagedRuntime::Root root(effect);
        if(effect)reinterpret_cast<Runtime*>(effect)->DeepCopyFrom(selected,instances);
        Core::ManagedPool::Return(instances,selected);return effect;
    }
    BNM_CustomMethod(ToEffect,false,BNM::CompileTimeClassBuilder("PvZCards.Engine","Effect","EngineLib.dll").Build(),"ToEffect",BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build(),BNM::CompileTimeClassBuilder("PvZCards.Engine","Entity","EngineLib.dll").Build());
};
inline Obj* Pool(Obj* model,int owner){
    return Engine::CardPoolAccess::Get(model,owner,true);
}
inline bool Prepare(const Engine::EffectGuards::Context& context){
    Obj* effect=context.effect;
    if(!effect || !effect->klass || std::string_view(effect->klass->name)!="CreateCardInSuperpowersFromSubsetEffect")return true;
    Obj* model=Core::ModelRegistry::Get(Engine::RouterContext::RouterModel(context.router));
    static auto target=BNM::Class("PvZCards.Engine","Effect").GetField("TargetId").cast<int>();
    static auto source=BNM::Class("PvZCards.Engine","Effect").GetField("SourceId").cast<int>();
    if(!model || !target.IsValid() || !source.IsValid())return false;
    Obj* recipient=Engine::EntityAccess::Find(model,Core::Metadata::Bind(target,effect).Get());
    Obj* origin=Engine::EntityAccess::Find(model,Core::Metadata::Bind(source,effect).Get());
    Obj* instances=Engine::Match::Instances(recipient?recipient:origin);if(!instances)return false;
    const int owner=Engine::EntityAccess::Component(recipient,"Player")?Engine::Match::Id(recipient):Engine::BoardPlacement::Owner(recipient);
    Obj* pool=Pool(model,owner);Obj* ordered=Engine::EntityAccess::Component(pool,"OrderedCardPool");
    auto system=Engine::Match::Service(instances,BNM::Class("PvZCards.Core.Systems","CreateCardInDeckSystem"));
    auto modifier=Engine::Match::Service(instances,BNM::Class("PvZCards.Core.Utilities","OrderedCardPoolUtils"));
    if(!pool || !ordered || !system || !modifier || !Deck::Choose(system->Get(),effect))return false;
    static auto add=BNM::Class("PvZCards.Core.Utilities","OrderedCardPoolUtils").GetMethod("AddCardGuid",{"cardGuid","cardPool","position"}).cast<void>();
    static auto record=BNM::Class("PvZCards.Core.Systems","CreateCardInDeckSystem").GetMethod("GenerateRecord",1).cast<void>();
    const auto stock=BNM::Class("PvZCards.Engine.Effects","CreateCardInDeckEffect");
    auto guid=stock.GetField("CardGuid").cast<int>(),amount=stock.GetField("AmountToCreate").cast<int>(),position=stock.GetField("DeckPosition").cast<int>();
    if(!add.IsValid() || !record.IsValid() || !guid.IsValid() || !amount.IsValid() || !position.IsValid())return false;
    const int count=Core::Metadata::Bind(amount,effect).Get();
    if(count<=0)return false;
    static auto entries=BNM::Class("PvZCards.Engine.Components","OrderedCardPool").GetField("Entries").cast<Engine::EntityAccess::List*>();
    auto* beforeList=entries.IsValid()?entries[ordered].Get():nullptr;
    if(!Engine::EntityAccess::Valid(beforeList))return false;
    const int before=beforeList->size;
    for(int i=0;i<count;++i)Core::Metadata::Bind(add,modifier->Get())(Core::Metadata::Bind(guid,effect).Get(),ordered,Core::Metadata::Bind(position,effect).Get());
    Core::Metadata::Bind(record,system->Get())(effect);
    Engine::SimulationChanges::Changed(model);
    auto* afterList=entries[ordered].Get();
    if(Engine::EntityAccess::Valid(afterList)){
        History::Record created;created.kind=History::Kind::CardCreated;created.visual=History::VisualPolicy::NoVisual;
        created.sourceId=Core::Metadata::Bind(source,effect).Get();created.entityId=created.sourceId;created.ownerId=owner;
        created.effectId=Variables::Field(effect,"EffectEntityId");created.from=History::Origin::Unknown;created.to=History::Origin::Pool;created.toZoneId=Engine::Match::Id(pool);
        created.payload=History::PoolCreatedPayload{Engine::Match::Id(pool),owner,Core::Metadata::Bind(guid,effect).Get(),before,afterList->size,count,Core::Metadata::Bind(position,effect).Get(),true};
        if(afterList->size-before!=count)created.stage=History::Stage::PartialFailure;
        History::Writer::Commit(std::move(created),model,instances);
    }
    LOG_INFO("CustomComponents/Superpowers/CREATE: player=%d pool=%d guid=%d amount=%d",owner,Engine::Match::Id(pool),Core::Metadata::Bind(guid,effect).Get(),count);
    return false; // This action has its own destination; never run regular-deck creation.
}
inline bool Initialize(){
    const auto descriptor=BNM::Class("PvZCards.Engine.Components","CreateCardInSuperpowersFromSubset");
    const auto base=BNM::Class("PvZCards.Engine.Components","CreateCardInDeckEffectDescriptor");
    if(!CustomComponentRegistry::Register({"CreateCardInSuperpowersFromSubset","PvZCards.Engine.Components","EngineLib.dll","CreateCardInDeckEffectDescriptor","PvZCards.Engine.Components","EngineLib.dll"}))return false;
    if(!Core::ComponentLifecycle::PatchVirtual(descriptor,base,BNM::Class("PvZCards.Engine.Components","EffectDescriptor"),"ToEffect",2))return false;
    if(!Core::ComponentLifecycle::Patch(BNM::Class("PvZCards.Engine.Effects","CreateCardInSuperpowersFromSubsetEffect"),BNM::Class("PvZCards.Engine.Effects","CreateCardInDeckEffect")))return false;
    return Engine::DeckCreation::Register({descriptor,[](Obj* self,Obj* instances,Obj* entity){return reinterpret_cast<Descriptor*>(self)->ToEffect(instances,entity);},Deck::Descriptor::Copy,Deck::Descriptor::Release}) &&
        Engine::EffectGuards::RegisterPreparation(Prepare,-25);
}
}
