#pragma once
// One record, one frontend-owned group. No stock CardMoveRecord substitution.
#include <BNM/UnityStructures.hpp>
#include "presentation/history/RecordedAnimation.hpp"
#include "presentation/history/NativeAnimation.hpp"
#include "engine/EntityAccess.hpp"
namespace CustomComponents::Presentation::SwapTransactions {

using Obj=BNM::IL2CPP::Il2CppObject;
struct SwapMove {int card,oldLane,oldOrder,newLane,order;};
struct SwapRefresh {std::uint64_t model;SwapMove first,second;};
inline BNM::Method<Obj*> getViewObject,createGroup,createReposition;
inline BNM::Method<void> removeFromLane,addToLane,addGroupAnimation;
inline BNM::Method<bool> laneContains;
inline BNM::Field<Obj*> laneFactory,laneQueue;
inline BNM::Method<BNM::Structures::Mono::List<Obj*>*> laneGroup;
inline BNM::Field<float> groupDelay;
inline BNM::Method<BNM::Structures::Unity::Vector3> lanePosition;
inline std::map<std::uint64_t,std::shared_ptr<Core::ManagedRuntime::Root>> visuals;
inline bool ApplySwapView(Obj* mapper,const SwapRefresh& swap,std::shared_ptr<Core::ManagedRuntime::Root>& visual) {
    if(!mapper || !getViewObject.IsValid() || !removeFromLane.IsValid() || !addToLane.IsValid() || !laneContains.IsValid())return false;
    Obj* cards[2]{};Obj* oldLanes[2]{};Obj* newLanes[2]{};
    const SwapMove moves[2]={swap.first,swap.second};
    for(int i=0;i<2;++i){
        auto error=BNM::TryInvoke([&]{
            cards[i]=getViewObject[mapper](moves[i].card);
            oldLanes[i]=getViewObject[mapper](moves[i].oldLane);
            newLanes[i]=getViewObject[mapper](moves[i].newLane);
        });
        if(error.IsValid() || !cards[i] || !oldLanes[i] || !newLanes[i]){
        LOG_WARN("CustomComponents/FrontendRefresh: phase=swap-view-resolve-failed card=%d oldLane=%d newLane=%d cardView=%p oldView=%p newView=%p",
                moves[i].card,moves[i].oldLane,moves[i].newLane,cards[i],oldLanes[i],newLanes[i]);
            return false;
        }
    }
    bool removedFirst=false,removedSecond=false,addedFirst=false,addedSecond=false;
    auto error=BNM::TryInvoke([&]{
        // Stock move animations may already have attached either view to its
        // destination. Remove both possible memberships before adding once;
        // AddToLane inserts unconditionally and otherwise duplicates the view.
        for(int i=0;i<2;++i){
            if(laneContains[oldLanes[i]](cards[i]))removeFromLane[oldLanes[i]](cards[i]);
            if(newLanes[i]!=oldLanes[i] && laneContains[newLanes[i]](cards[i]))removeFromLane[newLanes[i]](cards[i]);
        }
        removedFirst=true;removedSecond=true;
        // Restore increasing slot order when both destinations share a lane.
        const bool sameLane=moves[0].newLane==moves[1].newLane;
        if(sameLane && moves[0].order>moves[1].order){
            addToLane[newLanes[1]](cards[1],moves[1].order);addedSecond=true;
            addToLane[newLanes[0]](cards[0],moves[0].order);addedFirst=true;
        }else{
            addToLane[newLanes[0]](cards[0],moves[0].order);addedFirst=true;
            addToLane[newLanes[1]](cards[1],moves[1].order);addedSecond=true;
        }
    });
    const bool success=!error.IsValid() && removedFirst && removedSecond && addedFirst && addedSecond;
    if(success){
        std::vector<Obj*> lanes;
        for(Obj* lane:{oldLanes[0],oldLanes[1],newLanes[0],newLanes[1]})
            if(std::find(lanes.begin(),lanes.end(),lane)==lanes.end())lanes.push_back(lane);
        Obj* factory=laneFactory[newLanes[0]].Get();
        Obj* queue=laneQueue[newLanes[0]].Get();
        Obj* group=factory?createGroup[factory]():nullptr;
        Core::ManagedRuntime::Root groupRoot(group);
        if(group && queue){
            Core::Metadata::Bind(groupDelay,group).Set(0.f);
            for(Obj* lane:lanes)BNM::TryInvoke([&]{
                // Native grouping uses the anchor card's owner. The flat
                // lane pool also contains the opponent and environments.
                std::vector<Obj*> visited;
                for(int anchor:{moves[0].card,moves[1].card}){
                auto* views=Core::Metadata::Bind(laneGroup,lane)(anchor);
                if(!views || !views->items || views->size<0 || views->size>int(views->items->capacity))continue;
                for(int i=0;i<views->size;++i){
                    Obj* view=views->items->m_Items[i];if(!view || std::find(visited.begin(),visited.end(),view)!=visited.end())continue;
                    visited.push_back(view);
                    auto cardId=BNM::Class(view).GetMethod("GetCardEntityId",0).cast<int>();
                    if(!cardId.IsValid())continue;
                    const int id=cardId[view]();
                    // This list is already ordered within one owner's group.
                    // GetCardInLaneIndex searches the flat pool, including the
                    // other owner, and cannot supply this position's order.
                    const int order=i;
                    auto position=lanePosition[lane](id,order);
                    Obj* animation=createReposition[lane](view,position);
                    if(animation)addGroupAnimation[group](animation);
                }
                }
            });
            visual=std::make_shared<Core::ManagedRuntime::Root>(group);
            NativeAnimation::Call(group,"Start");
        }else LOG_WARN("CustomComponents/FrontendRefresh: swap reposition group unavailable factory=%p queue=%p",factory,queue);
    }
    if(!success){
        BNM::TryInvoke([&]{
            if(addedFirst)removeFromLane[newLanes[0]](cards[0]);
            if(addedSecond)removeFromLane[newLanes[1]](cards[1]);
            if(removedFirst)addToLane[oldLanes[0]](cards[0],moves[0].oldOrder);
            if(removedSecond)addToLane[oldLanes[1]](cards[1],moves[1].oldOrder);
        });
    }
    LOG_INFO("CustomComponents/FrontendRefresh: phase=swap-view-applied model=%llu first=%d removed=%d added=%d second=%d removed=%d added=%d success=%d",
        static_cast<unsigned long long>(swap.model),moves[0].card,int(removedFirst),int(addedFirst),
        moves[1].card,int(removedSecond),int(addedSecond),int(success));
    return success && visual && visual->Get();
}

inline HistoryAnimations::Progress Step(const History::Record& record,bool start){
    using Progress=HistoryAnimations::Progress;
    auto payload=std::get_if<History::SwapPayload>(&record.payload);if(!payload)return Progress::Failed;
    if(start){
        SwapRefresh swap{record.modelKey,
            {payload->firstBefore.card,payload->firstBefore.lane,payload->firstBefore.order,payload->firstAfter.lane,payload->firstAfter.order},
            {payload->secondBefore.card,payload->secondBefore.lane,payload->secondBefore.order,payload->secondAfter.lane,payload->secondAfter.order}};
        if(!ApplySwapView(Core::SceneViewContext::RootedMapper(),swap,visuals[record.serial]))return Progress::Failed;
    }
    auto it=visuals.find(record.serial);if(it==visuals.end() || !it->second->Get())return Progress::Failed;
    Obj* group=it->second->Get();NativeAnimation::Call(group,"Update");
    if(!NativeAnimation::Complete(group))return Progress::Running;
    NativeAnimation::Call(group,"FinishAnimation");visuals.erase(it);return Progress::Complete;
}
inline void Cancel(const History::Record& record){auto it=visuals.find(record.serial);if(it==visuals.end())return;
    NativeAnimation::Call(it->second->Get(),"Cancel");visuals.erase(it);
}
inline bool Initialize(){
    auto mapper=BNM::Class("PvZCards.Game","EntityViewMapper");
    auto lane=BNM::Class("PvZCards.Game","LaneViewMediator");
    if(!mapper.IsValid() || !lane.IsValid())return false;
    getViewObject=mapper.GetMethod("GetViewObject",{std::string_view("entityId")}).cast<Obj*>();
    removeFromLane=lane.GetMethod("RemoveFromLane",1).cast<void>();
    addToLane=lane.GetMethod("AddToLane",2).cast<void>();
    laneContains=lane.GetMethod("Contains",1).cast<bool>();
    lanePosition=lane.GetMethod("GetLanePositionForCard",2).cast<BNM::Structures::Unity::Vector3>();
    createReposition=lane.GetMethod("CreateCardRepositionAnimation",2).cast<Obj*>();
    laneFactory=lane.GetField("<AnimationFactory>k__BackingField").cast<Obj*>();
    laneQueue=lane.GetField("<AnimationQueue>k__BackingField").cast<Obj*>();
    auto group=BNM::Class("PvZCards.Game","EffectAnimationGroup");
    laneGroup=lane.GetMethod("GetCardsInSameGroupAsCard",1).cast<BNM::Structures::Mono::List<Obj*>*>();
    groupDelay=group.GetField("DelayBetweenAnimations").cast<float>();
    if(!laneGroup.IsValid() || !groupDelay.IsValid())return false;
    createGroup=BNM::Class("PvZCards.Game","EffectAnimationFactory").GetMethod("CreateEffectAnimation",0)
        .GetGeneric({group.GetCompileTimeClass()}).cast<Obj*>();
    addGroupAnimation=group.GetMethod("AddAnimation",1).cast<void>();
    if(!getViewObject.IsValid() || !removeFromLane.IsValid() || !addToLane.IsValid() || !laneContains.IsValid() ||
       !lanePosition.IsValid() || !createReposition.IsValid() ||
       !laneFactory.IsValid() || !laneQueue.IsValid() || !createGroup.IsValid() || !addGroupAnimation.IsValid())return false;
    return HistoryAnimations::Initialize() && HistoryAnimations::Register(History::Kind::SwapTransaction,Step,Cancel);
}
}
