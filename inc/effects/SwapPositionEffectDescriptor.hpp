#include "core/NativeConstruction.hpp"
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
#include "engine/SwapPolicy.hpp"
#include "engine/EffectGuards.hpp"
#include "engine/MatchAccess.hpp"
#include "engine/RouterContext.hpp"
#include "engine/SimulationChanges.hpp"
#include "engine/FrontendStateRefresh.hpp"
#include "engine/CustomRecords.hpp"
#include "engine/CustomRecordHistoryAnchor.hpp"
#include "engine/CardTypes.hpp"
#include "engine/FighterPair.hpp"
#include "variables/Resolver.hpp"
#include "log.h"
#include <cstdint>

namespace CustomComponents::Effects::SwapPosition {
using Obj=BNM::IL2CPP::Il2CppObject;
struct Descriptor:Obj {
    bool IgnoreTerrainRules=false,IgnoreOccupancyRules=false;
    BNM_CustomClass(Descriptor,
        BNM::CompileTimeClassBuilder("PvZCards.Engine.Components","SwapPositionEffectDescriptor","EngineLib.dll").Build(),
        BNM::CompileTimeClassBuilder("PvZCards.Engine.Components","MoveCardToLanesEffectDescriptor","EngineLib.dll").Build(),{});
    BNM_CustomField(IgnoreTerrainRules,BNM::Defaults::Get<bool>(),"IgnoreTerrainRules");
    BNM_CustomField(IgnoreOccupancyRules,BNM::Defaults::Get<bool>(),"IgnoreOccupancyRules");
    void Construct(){Core::NativeConstruction::Base(this);IgnoreTerrainRules=IgnoreOccupancyRules=false;}
    void DeepCopyFrom(Obj* source,Obj* instances){BNM_CallCustomMethodOrigin(DeepCopyFrom,this,source,instances);auto* from=reinterpret_cast<Descriptor*>(source);IgnoreTerrainRules=from && from->IgnoreTerrainRules;IgnoreOccupancyRules=from && from->IgnoreOccupancyRules;}
    void HandleReturnToPool(Obj* instances){IgnoreTerrainRules=IgnoreOccupancyRules=false;BNM_CallCustomMethodOrigin(HandleReturnToPool,this,instances);}
    BNM_CustomMethod(Construct,false,BNM::Defaults::Get<void>(),".ctor");
    BNM_CustomMethod(DeepCopyFrom,false,BNM::Defaults::Get<void>(),"DeepCopyFrom",BNM::Defaults::Get<Obj*>(),BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
    BNM_CustomMethod(HandleReturnToPool,false,BNM::Defaults::Get<void>(),"HandleReturnToPool",BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
    Obj* ToEffect(Obj* instances,Obj*){
        Obj* effect=Core::ManagedPool::Acquire(instances,BNM::Class("PvZCards.Engine.Effects","SwapPositionEffect"));
        if(effect){auto type=BNM::Class(effect);auto terrain=type.GetField("IgnoreTerrainRules").cast<bool>();auto occupancy=type.GetField("IgnoreOccupancyRules").cast<bool>();
            if(terrain.IsValid())terrain[effect].Set(IgnoreTerrainRules);if(occupancy.IsValid())occupancy[effect].Set(IgnoreOccupancyRules);}
        if(!effect)LOG_ERR("CustomComponents/SwapPosition: descriptor conversion failed");
        return effect;
    }
    BNM_CustomMethod(ToEffect,false,
        BNM::CompileTimeClassBuilder("PvZCards.Engine","Effect","EngineLib.dll").Build(),"ToEffect",
        BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build(),
        BNM::CompileTimeClassBuilder("PvZCards.Engine","Entity","EngineLib.dll").Build());
};
struct Runtime:Obj {
    // MoveCardToLanesEffect has an inherited Effect body plus OrderInLane at
    // offset 0x48 (total managed size 0x50). BNM uses this C++ size for the
    // registered runtime type, so preserve the complete stock payload.
    std::uint8_t inherited[0x40]{};
    bool IgnoreTerrainRules=false,IgnoreOccupancyRules=false;
    BNM_CustomClass(Runtime,
        BNM::CompileTimeClassBuilder("PvZCards.Engine.Effects","SwapPositionEffect","EngineLib.dll").Build(),
        BNM::CompileTimeClassBuilder("PvZCards.Engine.Effects","MoveCardToLanesEffect","EngineLib.dll").Build(),{});
    BNM_CustomField(IgnoreTerrainRules,BNM::Defaults::Get<bool>(),"IgnoreTerrainRules");
    BNM_CustomField(IgnoreOccupancyRules,BNM::Defaults::Get<bool>(),"IgnoreOccupancyRules");
    void DeepCopyFrom(Obj* source,Obj* instances){BNM_CallCustomMethodOrigin(DeepCopyFrom,this,source,instances);auto* from=reinterpret_cast<Runtime*>(source);IgnoreTerrainRules=from && from->IgnoreTerrainRules;IgnoreOccupancyRules=from && from->IgnoreOccupancyRules;}
    void HandleReturnToPool(Obj* instances){IgnoreTerrainRules=IgnoreOccupancyRules=false;BNM_CallCustomMethodOrigin(HandleReturnToPool,this,instances);}
    BNM_CustomMethod(DeepCopyFrom,false,BNM::Defaults::Get<void>(),"DeepCopyFrom",BNM::Defaults::Get<Obj*>(),BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
    BNM_CustomMethod(HandleReturnToPool,false,BNM::Defaults::Get<void>(),"HandleReturnToPool",BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
    // The effect factory obtains this runtime type through InstanceManager.
    // Register the public default constructor for its reflection path.
    void Construct(){Core::NativeConstruction::Base(this);IgnoreTerrainRules=IgnoreOccupancyRules=false;}
    BNM_CustomMethod(Construct,false,BNM::Defaults::Get<void>(),".ctor");
};
static_assert(sizeof(Runtime) == 0x58, "SwapPositionEffect must retain the native Move payload before swap policy fields");

inline bool Convert(Obj* descriptor,Obj* instances,Obj*,Obj*& result){
    if(!descriptor || !descriptor->klass || !descriptor->klass->name ||
        std::string_view(descriptor->klass->name)!="SwapPositionEffectDescriptor")return false;
    result=Core::ManagedPool::Acquire(instances,BNM::Class("PvZCards.Engine.Effects","SwapPositionEffect"));
    if(result){auto* to=reinterpret_cast<Runtime*>(result);auto* from=reinterpret_cast<Descriptor*>(descriptor);to->IgnoreTerrainRules=from->IgnoreTerrainRules;to->IgnoreOccupancyRules=from->IgnoreOccupancyRules;}
    return result!=nullptr;
}
struct Slot {int card=-1,lane=-1,player=-1,order=-1;};
struct LanePlacementData {int CardId;int LaneId;int PlayerId;int OrderInLane;};
static_assert(sizeof(LanePlacementData)==16);

inline std::optional<Slot> Capture(Obj* model,int id){
    Obj* card=Engine::EntityAccess::Find(model,id);
    if(!card || !Engine::CardTypes::Fighter(card))return {};
    Obj* element=Engine::EntityAccess::Component(card,"LaneElement");
    static auto laneIds=BNM::Class("PvZCards.Engine.Components","LaneElement").GetField("LaneIds")
        .cast<BNM::Structures::Mono::List<int>*>();
    auto ids=laneIds;
    auto* lanes=element && ids.IsValid()?ids[element].Get():nullptr;
    if(!Engine::EntityAccess::Valid(lanes) || lanes->size!=1)return {};
    int lane=lanes->items->m_Items[0];
    if(lane<0)return {};
    Obj* laneEntity=Engine::EntityAccess::Find(model,lane);
    Obj* laneComponent=Engine::EntityAccess::Component(laneEntity,"Lane");
    static auto getOrder=BNM::Class("PvZCards.Engine.Components","Lane")
        .GetMethod("GetOrderInLane",1).cast<int>();
    auto order=getOrder;
    int owner=Engine::BoardPlacement::Owner(card);
    if(!laneComponent || !order.IsValid() || owner<0)return {};
    int index=-1;
    const auto error=BNM::TryInvoke([&]{index=order[laneComponent](id);});
    if(error.IsValid())return {};
    return index>=0?std::optional<Slot>(Slot{id,lane,owner,index}):std::nullopt;
}

// Validate the destinations with the selected fighters removed conceptually.
// This also preserves native aquatic and team-up restrictions.
using Rules=Engine::SwapPolicy::Rules;
inline Rules Policy(Obj* effect){
    if(!effect)return {};
    if(effect->klass && std::string_view(effect->klass->name)=="SwapPositionEffect"){
        auto* runtime=reinterpret_cast<Runtime*>(effect);return {runtime->IgnoreTerrainRules,runtime->IgnoreOccupancyRules};}
    auto* descriptor=reinterpret_cast<Descriptor*>(Engine::EntityAccess::Component(effect,"SwapPositionEffectDescriptor"));
    return descriptor?Rules{descriptor->IgnoreTerrainRules,descriptor->IgnoreOccupancyRules}:Rules{};
}
inline bool CanOccupy(Obj* model,int cardId,int laneId,const Slot& a,const Slot& b,Rules rules){
    Obj* card=Engine::EntityAccess::Find(model,cardId);
    Obj* laneEntity=Engine::EntityAccess::Find(model,laneId);
    Obj* lane=Engine::EntityAccess::Component(laneEntity,"Lane");
    if(!card || !lane)return false;
    const int player=Engine::BoardPlacement::Owner(card);
    auto fighters=BNM::Class("PvZCards.Engine.Components","Lane").GetMethod("GetFightersForPlayer",1)
        .cast<BNM::Structures::Mono::List<int>*>();
    auto* list=fighters.IsValid()?fighters[lane](player):nullptr;
    if(!Engine::EntityAccess::Valid(list))return false;
    int residents=0;bool residentTeamup=false;
    for(int i=0;i<list->size;++i){
        int id=list->items->m_Items[i];if(id==a.card || id==b.card)continue;
        ++residents;
        residentTeamup=residentTeamup || Engine::BoardPlacement::Active(Engine::EntityAccess::Find(model,id),"Teamup");
    }
    Engine::BoardPlacementPolicy::Card traits{
        true,false,Engine::EntityAccess::Component(card,"Plants")!=nullptr,
        Engine::BoardPlacement::Active(card,"Teamup"),Engine::BoardPlacement::Active(card,"Aquatic")};
    return Engine::SwapPolicy::Fits(traits,residents,residentTeamup,Engine::EntityAccess::Component(laneEntity,"WaterTerrain")!=nullptr,rules);
}

inline bool CanExchangeSlots(Obj* model,const Slot& a,const Slot& b,Rules rules={}){
    return a.card!=b.card && a.player>=0 && b.player>=0 &&
        Engine::FighterPair::Live(model,a.card) && Engine::FighterPair::Live(model,b.card) &&
        (a.lane!=b.lane || a.order!=b.order) &&
        CanOccupy(model,a.card,b.lane,a,b,rules) && CanOccupy(model,b.card,a.lane,a,b,rules);
}

inline bool CanExchange(Obj* model,int first,int second,Obj* effect=nullptr){
    const auto a=Capture(model,first),b=Capture(model,second);
    return a && b && CanExchangeSlots(model,*a,*b,Policy(effect));
}

inline int DestinationOrder(Obj* model,const Slot& incoming,const Slot& outgoing,const Slot& a,const Slot& b){
    if(incoming.player==outgoing.player)return outgoing.order;
    Obj* lane=Engine::EntityAccess::Component(Engine::EntityAccess::Find(model,outgoing.lane),"Lane");
    static auto get=BNM::Class("PvZCards.Engine.Components","Lane").GetMethod("GetFightersForPlayer",1).cast<BNM::Structures::Mono::List<int>*>();
    auto* list=lane && get.IsValid()?get[lane](incoming.player):nullptr;
    int residents=0;if(Engine::EntityAccess::Valid(list))for(int i=0;i<list->size;++i)if(list->items->m_Items[i]!=a.card && list->items->m_Items[i]!=b.card)++residents;
    const bool teamup=Engine::BoardPlacement::Active(Engine::EntityAccess::Find(model,incoming.card),"Teamup");
    return residents && teamup?residents:0;
}
inline bool Exchange(Obj* model,Obj* effect){
    auto effectType=BNM::Class("PvZCards.Engine","Effect");
    auto target=effectType.GetField("TargetId").cast<int>();
    auto secondary=effectType.GetField("SecondaryTargetId").cast<int>();
    if(!model || !target.IsValid() || !secondary.IsValid() || !effect)return false;
    const int aId=target[effect].Get(),bId=secondary[effect].Get();
    if(aId<0 || bId<0 || aId==bId)return false;
    auto a=Capture(model,aId),b=Capture(model,bId);
    if(!a || !b || !CanExchangeSlots(model,*a,*b,Policy(effect)))return false;
    Obj* source=Engine::EntityAccess::Find(model,Variables::Field(effect,"SourceId"));
    Obj* instances=Engine::Match::Instances(source?source:Engine::EntityAccess::Find(model,aId));
    if(!instances)return false;
    auto laneType=BNM::Class("PvZCards.Core.Systems.Utilities","LaneModifier");
    Obj* modifier=Engine::Match::Provide(instances,laneType.GetMonoType());
    auto remove=laneType.GetMethod("RemoveCardFromLane",1).cast<void>();
    auto place=laneType.GetMethod("PlayCardInLane",1).cast<int>();
    Obj* history=Engine::Match::Provide(instances,BNM::Class("PvZCards.Engine","History").GetMonoType());
    if(!modifier || !history || !remove.IsValid() || !place.IsValid())return false;
    const int destinationA=DestinationOrder(model,*a,*b,*a,*b),destinationB=DestinationOrder(model,*b,*a,*a,*b);
    int newOrderA=-1,newOrderB=-1;
    bool removedA=false,removedB=false,placedA=false,placedB=false;
    auto error=BNM::TryInvoke([&]{
        // Both source slots become free before either destination is filled.
        remove[modifier](aId);removedA=true;
        remove[modifier](bId);removedB=true;
        // Insert any front-position destination first (important when both
        // fighters were team-up partners in the same lane).
        const LanePlacementData destA{aId,b->lane,a->player,destinationA};
        const LanePlacementData destB{bId,a->lane,b->player,destinationB};
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
        appliedA->lane==b->lane && appliedA->order==destinationA &&
        appliedB->lane==a->lane && appliedB->order==destinationB;
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
    History::Record transaction;
    transaction.kind=History::Kind::SwapTransaction;transaction.stage=History::Stage::Committed;
    transaction.visual=History::VisualPolicy::Required;
    transaction.model=reinterpret_cast<std::uintptr_t>(model);
    transaction.modelToken=Core::ModelRegistry::Identify(model,true);
    transaction.history=reinterpret_cast<std::uintptr_t>(history);
    transaction.stockHistoryAnchor=Engine::CustomRecordHistoryAnchor::Count(history);
    transaction.entityId=aId;transaction.ownerId=a->player;transaction.sourceId=Variables::Field(effect,"SourceId");
    transaction.effectId=Variables::Field(effect,"EffectEntityId");
    transaction.from=transaction.to=History::Origin::Lane;
    transaction.fromZoneId=a->lane;transaction.toZoneId=b->lane;
    transaction.payload=History::SwapPayload{{aId,a->lane,a->order},{aId,b->lane,newOrderA},{bId,b->lane,b->order},{bId,a->lane,newOrderB},a->player,b->player,Policy(effect).ignoreTerrain,Policy(effect).ignoreOccupancy};
    // AI/prediction mutations remain simulation-only; never enqueue their views.
    if(transaction.model!=Core::SceneViewContext::ModelIdentity())transaction.visual=History::VisualPolicy::NoVisual;
    if(!Core::Performance::aiDepth && !History::Commit(transaction))LOG_ERR("CustomHistory/Swap: commit failed after verified native placement");
    Engine::SimulationChanges::Changed(model);
    LOG_INFO("CustomComponents/SwapPosition: %d lane=%d order=%d <-> %d lane=%d order=%d",
        aId,a->lane,a->order,bId,b->lane,b->order);
    return true;
}
inline bool Prepare(const Engine::EffectGuards::Context& c){
    if(!c.effect || !c.effect->klass || !c.effect->klass->name ||
        std::string_view(c.effect->klass->name)!="SwapPositionEffect")return true;
    Obj* model=Core::ModelRegistry::Get(Engine::RouterContext::RouterModel(c.router));
    if(!Exchange(model,c.effect))LOG_WARN("CustomComponents/SwapPosition: rejected effect=%p model=%p target=%d secondary=%d (invalid selection, destination or native failure)",
        static_cast<void*>(c.effect),static_cast<void*>(model),Variables::Field(c.effect,"TargetId"),Variables::Field(c.effect,"SecondaryTargetId"));
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
            "PvZCards.Engine.Components","MoveCardToLanesEffectDescriptor",{{"IgnoreTerrainRules",Systems::CustomFieldBridgeSystem::FieldKind::Bool},{"IgnoreOccupancyRules",Systems::CustomFieldBridgeSystem::FieldKind::Bool}},false) &&
        Core::ComponentLifecycle::PatchVirtual(descriptor,baseDescriptor,
            BNM::Class("PvZCards.Engine.Components","EffectDescriptor"),"ToEffect",2) &&
        Engine::AbilityConversion::Add(Convert) &&
        Engine::FrontendStateRefresh::Initialize() &&
        Engine::EffectGuards::RegisterPreparation(Prepare,-27);
}
}
