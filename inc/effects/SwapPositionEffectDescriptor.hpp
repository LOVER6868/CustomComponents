#pragma once
#include <BNM/ClassesManagement.hpp>
#include <BNM/Class.hpp>
#include <BNM/Defaults.hpp>
#include "core/CustomComponentRegistry.hpp"
#include "core/ComponentLifecycle.hpp"
#include "core/CustomFieldBridgeSystem.hpp"
#include "core/ManagedPool.hpp"
#include "engine/AbilityConversion.hpp"
#include "engine/BoardPlacement.hpp"
#include "engine/BoardPlacementPolicy.hpp"
#include "engine/EffectGuards.hpp"
#include "engine/MatchAccess.hpp"
#include "engine/RouterContext.hpp"
#include "engine/SimulationChanges.hpp"
#include "engine/CardTypes.hpp"
#include "variables/Resolver.hpp"
#include "log.h"

namespace CustomComponents::Effects::SwapPosition {
using Obj=BNM::IL2CPP::Il2CppObject;
struct Descriptor:Obj {
    BNM_CustomClass(Descriptor,
        BNM::CompileTimeClassBuilder("PvZCards.Engine.Components","SwapPositionEffectDescriptor","EngineLib.dll").Build(),
        BNM::CompileTimeClassBuilder("PvZCards.Engine.Components","MoveCardToLanesEffectDescriptor","EngineLib.dll").Build(),{});
    Obj* ToEffect(Obj* instances,Obj*){
        return Core::ManagedPool::Acquire(instances,BNM::Class("PvZCards.Engine.Effects","SwapPositionEffect"));
    }
    BNM_CustomMethod(ToEffect,false,
        BNM::CompileTimeClassBuilder("PvZCards.Engine","Effect","EngineLib.dll").Build(),"ToEffect",
        BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build(),
        BNM::CompileTimeClassBuilder("PvZCards.Engine","Entity","EngineLib.dll").Build());
};
struct Runtime:Obj {
    BNM_CustomClass(Runtime,
        BNM::CompileTimeClassBuilder("PvZCards.Engine.Effects","SwapPositionEffect","EngineLib.dll").Build(),
        BNM::CompileTimeClassBuilder("PvZCards.Engine.Effects","MoveCardToLanesEffect","EngineLib.dll").Build(),{});
};

inline bool Convert(Obj* descriptor,Obj* instances,Obj*,Obj*& result){
    if(!descriptor || !descriptor->klass || !descriptor->klass->name ||
        std::string_view(descriptor->klass->name)!="SwapPositionEffectDescriptor")return false;
    result=Core::ManagedPool::Acquire(instances,BNM::Class("PvZCards.Engine.Effects","SwapPositionEffect"));
    return true;
}
struct Slot {int card=-1,lane=-1,player=-1,order=-1;};
struct LanePlacementData {int CardId;int LaneId;int PlayerId;int OrderInLane;};
static_assert(sizeof(LanePlacementData)==16);

inline std::optional<Slot> Capture(Obj* model,int id){
    Obj* card=Engine::EntityAccess::Find(model,id);
    if(!card || !Engine::CardTypes::Fighter(card))return {};
    Obj* element=Engine::EntityAccess::Component(card,"LaneElement");
    auto ids=BNM::Class("PvZCards.Engine.Components","LaneElement").GetField("LaneIds")
        .cast<BNM::Structures::Mono::List<int>*>();
    auto* lanes=element && ids.IsValid()?ids[element].Get():nullptr;
    if(!lanes || !lanes->items || lanes->size!=1)return {};
    int lane=lanes->items->m_Items[0];
    Obj* laneEntity=Engine::EntityAccess::Find(model,lane);
    Obj* laneComponent=Engine::EntityAccess::Component(laneEntity,"Lane");
    auto order=BNM::Class("PvZCards.Engine.Components","Lane").GetMethod("GetOrderInLane",1).cast<int>();
    int owner=Engine::BoardPlacement::Owner(card);
    if(!laneComponent || !order.IsValid() || owner<0)return {};
    int index=order[laneComponent](id);
    return index>=0 && index<=1?std::optional<Slot>(Slot{id,lane,owner,index}):std::nullopt;
}

// Validate the destinations with the selected fighters removed conceptually.
// This also preserves native aquatic and team-up restrictions.
inline bool CanOccupy(Obj* model,int cardId,int laneId,const Slot& a,const Slot& b){
    Obj* card=Engine::EntityAccess::Find(model,cardId);
    Obj* laneEntity=Engine::EntityAccess::Find(model,laneId);
    Obj* lane=Engine::EntityAccess::Component(laneEntity,"Lane");
    if(!card || !lane)return false;
    const int player=Engine::BoardPlacement::Owner(card);
    auto fighters=BNM::Class("PvZCards.Engine.Components","Lane").GetMethod("GetFightersForPlayer",1)
        .cast<BNM::Structures::Mono::List<int>*>();
    auto* list=fighters.IsValid()?fighters[lane](player):nullptr;
    if(!list || !list->items || list->size<0 || list->size>2)return false;
    int residents=0;bool residentTeamup=false;
    for(int i=0;i<list->size;++i){
        int id=list->items->m_Items[i];if(id==a.card || id==b.card)continue;
        ++residents;
        residentTeamup=residentTeamup || Engine::BoardPlacement::Active(Engine::EntityAccess::Find(model,id),"Teamup");
    }
    Engine::BoardPlacementPolicy::Card traits{
        true,false,Engine::EntityAccess::Component(card,"Plants")!=nullptr,
        Engine::BoardPlacement::Active(card,"Teamup"),Engine::BoardPlacement::Active(card,"Aquatic")};
    return Engine::BoardPlacementPolicy::Fits(traits,residents,residentTeamup,
        Engine::EntityAccess::Component(laneEntity,"WaterTerrain")!=nullptr);
}

inline bool Exchange(Obj* model,Obj* effect){
    auto effectType=BNM::Class("PvZCards.Engine","Effect");
    auto target=effectType.GetField("TargetId").cast<int>();
    auto secondary=effectType.GetField("SecondaryTargetId").cast<int>();
    if(!model || !target.IsValid() || !secondary.IsValid() || !effect)return false;
    const int aId=target[effect].Get(),bId=secondary[effect].Get();
    if(aId<0 || bId<0 || aId==bId)return false;
    auto a=Capture(model,aId),b=Capture(model,bId);
    if(!a || !b)return false;
    if(a->lane==b->lane && a->order==b->order)return false;
    if(!CanOccupy(model,aId,b->lane,*a,*b) || !CanOccupy(model,bId,a->lane,*a,*b))return false;
    Obj* source=Engine::EntityAccess::Find(model,Variables::Field(effect,"SourceId"));
    Obj* instances=Engine::Match::Instances(source?source:Engine::EntityAccess::Find(model,aId));
    if(!instances)return false;
    auto laneType=BNM::Class("PvZCards.Core.Systems.Utilities","LaneModifier");
    Obj* modifier=Engine::Match::Provide(instances,laneType.GetMonoType());
    auto remove=laneType.GetMethod("RemoveCardFromLane",1).cast<void>();
    auto place=laneType.GetMethod("PlayCardInLane",1).cast<int>();
    // Use the stock movement system's record generator: it creates a native
    // CardMoveRecord with the same fields as an ordinary move. Do not simulate
    // a move by editing LaneElement directly.
    auto moveType=BNM::Class("PvZCards.Core.Systems","MoveCardToLanesSystem");
    Obj* moveSystem=Engine::Match::Provide(instances,moveType.GetMonoType());
    auto record=moveType.GetMethod("RecordCardMove",3).cast<void>();
    if(!modifier || !moveSystem || !remove.IsValid() || !place.IsValid() || !record.IsValid())return false;
    int newOrderA=-1,newOrderB=-1;
    bool removedA=false,removedB=false,placedA=false,placedB=false;
    auto error=BNM::TryInvoke([&]{
        // Both source slots become free before either destination is filled.
        remove[modifier](aId);removedA=true;
        remove[modifier](bId);removedB=true;
        // Insert any front-position destination first (important when both
        // fighters were team-up partners in the same lane).
        const LanePlacementData destA{aId,b->lane,a->player,b->order};
        const LanePlacementData destB{bId,a->lane,b->player,a->order};
        if(a->lane==b->lane && b->order>a->order){
            newOrderB=place[modifier](destB);placedB=true;
            newOrderA=place[modifier](destA);placedA=true;
        }else{
            newOrderA=place[modifier](destA);placedA=true;
            newOrderB=place[modifier](destB);placedB=true;
        }
    });
    auto appliedA=(!error.IsValid() && placedA)?Capture(model,aId):std::nullopt;
    auto appliedB=(!error.IsValid() && placedB)?Capture(model,bId):std::nullopt;
    const bool exact=appliedA && appliedB &&
        appliedA->lane==b->lane && appliedA->order==b->order &&
        appliedB->lane==a->lane && appliedB->order==a->order;
    if(error.IsValid() || !exact){
        if(removedA || removedB){
            // Best-effort native rollback in case of an unexpected placement
            // failure; never emit either movement record on failure.
            BNM::TryInvoke([&]{
                if(placedA)remove[modifier](aId);
                if(placedB)remove[modifier](bId);
                const auto restoreA=LanePlacementData{aId,a->lane,a->player,a->order};
                const auto restoreB=LanePlacementData{bId,b->lane,b->player,b->order};
                if(a->lane==b->lane && a->order>b->order){
                    if(removedB)place[modifier](restoreB);
                    if(removedA)place[modifier](restoreA);
                }else{
                    if(removedA)place[modifier](restoreA);
                    if(removedB)place[modifier](restoreB);
                }
            });
        }
        return false;
    }
    // Record both moves using their own target, destination and old lane.
    // The native writer stores SecondaryTargetId as the DESTINATION LANE ID,
    // rather than as another fighter; temporarily repurpose it only here.
    const int priorPrimary=target[effect].Get(),priorSecondary=secondary[effect].Get();
    error=BNM::TryInvoke([&]{
        target[effect].Set(aId);secondary[effect].Set(b->lane);
        record[moveSystem](effect,a->lane,newOrderA);
        target[effect].Set(bId);secondary[effect].Set(a->lane);
        record[moveSystem](effect,b->lane,newOrderB);
    });
    target[effect].Set(priorPrimary);secondary[effect].Set(priorSecondary);
    if(error.IsValid()){
        LOG_ERR("CustomComponents/SwapPosition: placement succeeded but native movement record failed");
        return false;
    }
    Engine::SimulationChanges::Changed(model);
    LOG_INFO("CustomComponents/SwapPosition: %d lane=%d order=%d <-> %d lane=%d order=%d",
        aId,a->lane,a->order,bId,b->lane,b->order);
    return true;
}
inline bool Prepare(const Engine::EffectGuards::Context& c){
    if(!c.effect || !c.effect->klass || !c.effect->klass->name ||
        std::string_view(c.effect->klass->name)!="SwapPositionEffect")return true;
    Obj* model=Core::ModelRegistry::Get(Engine::RouterContext::RouterModel(c.router));
    if(!Exchange(model,c.effect))LOG_WARN("CustomComponents/SwapPosition: swap rejected (invalid targets or native failure)");
    return false; // Custom effect is not a native single-card MoveCardToLanesEffect.
}
inline bool Initialize(){
    auto descriptor=BNM::Class("PvZCards.Engine.Components","SwapPositionEffectDescriptor");
    auto runtime=BNM::Class("PvZCards.Engine.Effects","SwapPositionEffect");
    auto baseDescriptor=BNM::Class("PvZCards.Engine.Components","MoveCardToLanesEffectDescriptor");
    auto baseRuntime=BNM::Class("PvZCards.Engine.Effects","MoveCardToLanesEffect");
    return CustomComponentRegistry::Register({"SwapPositionEffectDescriptor","PvZCards.Engine.Components","EngineLib.dll",
        "MoveCardToLanesEffectDescriptor","PvZCards.Engine.Components","EngineLib.dll"}) &&
        CustomComponentRegistry::Register({"SwapPositionEffect","PvZCards.Engine.Effects","EngineLib.dll",
        "MoveCardToLanesEffect","PvZCards.Engine.Effects","EngineLib.dll"}) &&
        Core::ComponentLifecycle::Patch(descriptor,baseDescriptor) &&
        Core::ComponentLifecycle::Patch(runtime,baseRuntime) &&
        Systems::CustomFieldBridgeSystem::Register("PvZCards.Engine.Components","SwapPositionEffectDescriptor",
            "PvZCards.Engine.Components","MoveCardToLanesEffectDescriptor",{},false) &&
        Engine::EffectGuards::RegisterPreparation(Prepare,-27);
}
}
