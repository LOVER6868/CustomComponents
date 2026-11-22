#pragma once
#include <BNM/ClassesManagement.hpp>
#include <BNM/Defaults.hpp>
#include "core/ComponentLifecycle.hpp"
#include "core/CustomComponentRegistry.hpp"
#include "core/CustomFieldBridgeSystem.hpp"
#include "core/ManagedPool.hpp"
#include "diagnostics/DiagnosticLogging.hpp"
#include "core/NativeConstruction.hpp"
#include "engine/AbilityConversion.hpp"
#include "engine/BoardPlacement.hpp"
#include "engine/EffectGuards.hpp"
#include "engine/FighterPair.hpp"
#include "engine/SimulationChanges.hpp"
#include "engine/CustomRecordHistoryAnchor.hpp"
#include "history/RecordWriter.hpp"
#include "triggers/CustomTriggerSystem.hpp"

namespace CustomComponents::Effects::TransformExisting {
namespace Bridge=Systems::CustomFieldBridgeSystem;
using Obj=Engine::Match::Obj;
inline constexpr const char* DescriptorName="TransformCardIntoExistingCardEffectDescriptor";
inline constexpr const char* RuntimeName="TransformCardIntoExistingCardEffect";
struct Descriptor:Obj {
    Obj* SubsetQuery=nullptr; // inherited stock query slot at 0x10
    bool PluckFailSafe=false,MakeACopy=false,FirePlayTrigger=false;
    BNM_CustomClass(Descriptor,
        BNM::CompileTimeClassBuilder("PvZCards.Engine.Components","TransformCardIntoExistingCardEffectDescriptor","EngineLib.dll").Build(),
        BNM::CompileTimeClassBuilder("PvZCards.Engine.Components","TransformIntoCardFromSubsetEffectDescriptor","EngineLib.dll").Build(),{});

    BNM_CustomField(PluckFailSafe,BNM::Defaults::Get<bool>(),"PluckFailSafe");
    BNM_CustomField(MakeACopy,BNM::Defaults::Get<bool>(),"MakeACopy");
    BNM_CustomField(FirePlayTrigger,BNM::Defaults::Get<bool>(),"FirePlayTrigger");
    void Construct(){Core::NativeConstruction::Base(this);PluckFailSafe=MakeACopy=FirePlayTrigger=false;}
    void DeepCopyFrom(Obj* source,Obj* instances){
        BNM_CallCustomMethodOrigin(DeepCopyFrom,this,source,instances);
        auto* from=reinterpret_cast<Descriptor*>(source);
        MakeACopy=from && from->MakeACopy;
        FirePlayTrigger=from && from->FirePlayTrigger;
        PluckFailSafe=from && from->PluckFailSafe;

    }
    void HandleReturnToPool(Obj* instances){

        PluckFailSafe=MakeACopy=FirePlayTrigger=false;
        BNM_CallCustomMethodOrigin(HandleReturnToPool,this,instances);
    }
    Obj* ToEffect(Obj* instances,Obj*){
        static const auto type=BNM::Class("PvZCards.Engine.Effects",RuntimeName);
        return Core::ManagedPool::Acquire(instances,type);
    }
    BNM_CustomMethod(Construct,false,BNM::Defaults::Get<void>(),".ctor");
    BNM_CustomMethod(DeepCopyFrom,false,BNM::Defaults::Get<void>(),"DeepCopyFrom",BNM::Defaults::Get<Obj*>(),BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
    BNM_CustomMethod(HandleReturnToPool,false,BNM::Defaults::Get<void>(),"HandleReturnToPool",BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
    BNM_CustomMethod(ToEffect,false,BNM::CompileTimeClassBuilder("PvZCards.Engine","Effect","EngineLib.dll").Build(),"ToEffect",
        BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build(),
        BNM::CompileTimeClassBuilder("PvZCards.Engine","Entity","EngineLib.dll").Build());
};
struct Runtime:Obj {
    std::uint8_t inherited[0x40]{};
    BNM_CustomClass(Runtime,
        BNM::CompileTimeClassBuilder("PvZCards.Engine.Effects","TransformCardIntoExistingCardEffect","EngineLib.dll").Build(),
        BNM::CompileTimeClassBuilder("PvZCards.Engine.Effects","MoveCardToLanesEffect","EngineLib.dll").Build(),{});
    void Construct(){Core::NativeConstruction::Base(this);}
    BNM_CustomMethod(Construct,false,BNM::Defaults::Get<void>(),".ctor");
};
inline bool Convert(Obj* descriptor,Obj* instances,Obj* entity,Obj*& result){
    if(!descriptor || !descriptor->klass || !descriptor->klass->name ||
       std::string_view(descriptor->klass->name)!=DescriptorName)return false;
    result=reinterpret_cast<Descriptor*>(descriptor)->ToEffect(instances,entity);
    return result!=nullptr;
}
inline const Descriptor* Options(Obj* entity){
    return reinterpret_cast<const Descriptor*>(Engine::EntityAccess::Component(entity,DescriptorName));
}
inline bool HasFallback(Obj* entity){
    const auto* options=Options(entity);
    return options && options->PluckFailSafe && options->SubsetQuery;
}
inline int Lane(Obj* card){
    static auto laneIds=BNM::Class("PvZCards.Engine.Components","LaneElement").GetField("LaneIds")
        .cast<BNM::Structures::Mono::List<int>*>();
    auto* element=Engine::EntityAccess::Component(card,"LaneElement");
    auto* lanes=element && laneIds.IsValid()?laneIds[element].Get():nullptr;
    return Engine::EntityAccess::Valid(lanes) && lanes->size==1?lanes->items->m_Items[0]:-1;
}
inline bool Execute(Obj* model,Obj* effect){
    static const auto base=BNM::Class("PvZCards.Engine","Effect");
    static auto target=base.GetField("TargetId").cast<int>();
    static auto secondary=base.GetField("SecondaryTargetId").cast<int>();
    static auto definition=base.GetField("EffectEntityId").cast<int>();
    static const auto systemClass=BNM::Class("PvZCards.Core.Systems","TransformIntoCardFromSubsetSystem");
    static auto inLane=systemClass.GetMethod("TransformInLane",3).cast<void>();
    static const auto finderClass=BNM::Class("PvZCards.Core.Systems.Utilities","CardFromSubsetFinder");
    static auto find= finderClass.GetMethod("FindCard",{"subsetQuery","laneId","effect"}).cast<int>();
    static const auto historyClass=BNM::Class("PvZCards.Engine","History");
    static auto add=historyClass.GetMethod("AddRecord",{"recordType"}).cast<Obj*>();
    static const auto retireClass=BNM::Class("PvZCards.Engine.Records","RemoveDisplacedFighterRecord");
    static auto retireTarget=retireClass.GetMethod("set_TargetId",1).cast<void>();
    static const auto laneClass=BNM::Class("PvZCards.Core.Systems.Utilities","LaneModifier");
    static auto detach=laneClass.GetMethod("RemoveCardFromLane",1).cast<void>();
    static const auto destroyClass=BNM::Class("PvZCards.Core.Systems.Utilities","EntityDestroyer");
    static auto destroy=destroyClass.GetMethod("DestroyEntity",1).cast<void>();

    if(!model || !target.IsValid() || !secondary.IsValid() || !definition.IsValid() || !inLane.IsValid())return false;
    const int targetId=target[effect].Get(),donorId=secondary[effect].Get();
    auto* card=Engine::EntityAccess::Find(model,targetId);
    const int lane=Lane(card);
    auto* ability=Engine::EntityAccess::Find(model,definition[effect].Get());
    const auto* options=Options(ability);
    if(!options || lane<0 || !Engine::FighterPair::Live(model,targetId))return false;
    const bool copy=options->MakeACopy,play=options->FirePlayTrigger;
    auto* instances=Engine::Match::Instances(card);
    auto* system=Engine::Match::Provide(instances,systemClass.GetMonoType());
    auto* history=Engine::Match::Provide(instances,historyClass.GetMonoType());
    if(!system || !history)return false;

    auto* donor=Engine::EntityAccess::Find(model,donorId);
    const bool legalDonor=Engine::FighterPair::Legal(model,targetId,donorId) && Lane(donor)>=0;
    int guid=legalDonor?Engine::BoardPlacement::Guid(donor):-1;
    if(guid<0 && HasFallback(ability) && find.IsValid()){
        auto* finder=Engine::Match::Provide(instances,finderClass.GetMonoType());
        if(finder)guid=find[finder](options->SubsetQuery,lane,effect);
    }
    if(guid<0)return false;

    const bool consume=legalDonor && !copy;
    Obj* lanes=consume?Engine::Match::Provide(instances,laneClass.GetMonoType()):nullptr;
    Obj* destroyer=consume?Engine::Match::Provide(instances,destroyClass.GetMonoType()):nullptr;
    if(consume && (!lanes || !destroyer || !retireClass.IsValid() || !add.IsValid() ||
                   !retireTarget.IsValid() || !detach.IsValid() || !destroy.IsValid()))return false;
    const int donorLane=consume?Lane(donor):-1;
    const int donorOwner=consume?Engine::BoardPlacement::Owner(donor):-1;
    const auto before=Engine::CustomRecordHistoryAnchor::Count(history);
    const auto transformed=BNM::TryInvoke([&]{inLane[system](effect,guid,lane);});
    auto* result=Engine::EntityAccess::Find(model,targetId);
    if(transformed.IsValid() || Lane(result)!=lane || Engine::BoardPlacement::Guid(result)!=guid ||
       Engine::CustomRecordHistoryAnchor::Count(history)<=before)return false;

    if(consume){
        const auto removed=BNM::TryInvoke([&]{
            detach[lanes](donorId);
            if(Lane(donor)>=0)return;
            destroy[destroyer](donor);
        });
        if(removed.IsValid() || Engine::EntityAccess::Find(model,donorId)){
            LOG_ERR("CustomComponents/TransformExisting: transform committed but donor removal failed target=%d donor=%d",targetId,donorId);
            return false;
        }
        if(auto* vacated=add[history](retireClass.GetMonoType()))retireTarget[vacated](donorId);
        History::Record record;
        record.kind=History::Kind::TransformDonorConsumed;
        record.visual=History::VisualPolicy::NoVisual;
        record.entityId=donorId;
        record.ownerId=donorOwner;
        record.from=History::Origin::Lane;
        record.fromZoneId=donorLane;
        record.payload=History::TransformDonorPayload{donorId,targetId,donorLane};
        History::Writer::Commit(std::move(record),model,instances);
    }
    Engine::SimulationChanges::Changed(model);
    if(play)Systems::CustomTriggerSystem::FireInModel("PlayTrigger",effect,model);
    CC_DIAG("CustomComponents/TransformExisting: target=%d donor=%d guid=%d consumed=%d play=%d",targetId,donorId,guid,int(consume),int(play));
    return true;
}
inline bool Prepare(const Engine::EffectGuards::Context& context){
    auto* effect=context.effect;
    if(!effect || !effect->klass || !effect->klass->name ||
       std::string_view(effect->klass->name)!=RuntimeName)return true;
    auto* model=Core::ModelRegistry::Get(Engine::RouterContext::RouterModel(context.router));
    if(!Execute(model,effect))CC_DIAG("CustomComponents/TransformExisting: no committed replacement");
    return false;
}
inline bool Initialize(){

    using K=Bridge::FieldKind;
    const auto descriptor=BNM::Class("PvZCards.Engine.Components",DescriptorName);
    const auto baseDescriptor=BNM::Class("PvZCards.Engine.Components","TransformIntoCardFromSubsetEffectDescriptor");
    const auto runtime=BNM::Class("PvZCards.Engine.Effects",RuntimeName);
    return CustomComponentRegistry::Register({DescriptorName,"PvZCards.Engine.Components","EngineLib.dll","TransformIntoCardFromSubsetEffectDescriptor","PvZCards.Engine.Components","EngineLib.dll"}) &&
        Core::ComponentLifecycle::Patch(descriptor,baseDescriptor) &&
        Core::ComponentLifecycle::Patch(runtime,BNM::Class("PvZCards.Engine.Effects","MoveCardToLanesEffect")) &&
        Bridge::Register("PvZCards.Engine.Components",DescriptorName,"PvZCards.Engine.Components","TransformIntoCardFromSubsetEffectDescriptor",
            {{"PluckFailSafe",K::Bool},{"MakeACopy",K::Bool},{"FirePlayTrigger",K::Bool}},false) &&
        Core::ComponentLifecycle::PatchVirtual(descriptor,baseDescriptor,BNM::Class("PvZCards.Engine.Components","EffectDescriptor"),"ToEffect",2) &&
        Engine::AbilityConversion::Add(Convert) && Engine::EffectGuards::RegisterPreparation(Prepare,-27);
}
}
