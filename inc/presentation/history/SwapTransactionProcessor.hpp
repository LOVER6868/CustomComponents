#pragma once
// One record, one frontend-owned group. No stock CardMoveRecord substitution.
#include <BNM/UnityStructures.hpp>
#include <functional>
#include "presentation/history/RecordedAnimation.hpp"
#include "presentation/history/NativeAnimation.hpp"
#include "engine/EntityAccess.hpp"
namespace CustomComponents::Presentation::SwapTransactions {

using Obj=BNM::IL2CPP::Il2CppObject;
struct SwapMove {int card,oldLane,oldOrder,newLane,order;};
struct SwapRefresh {std::uint64_t model;std::vector<SwapMove> moves;};
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
    const auto& moves=swap.moves;
    if(moves.size()<2)return false;
    std::vector<Obj*> cards(moves.size()),oldLanes(moves.size()),newLanes(moves.size());
    for(std::size_t i=0;i<moves.size();++i){
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
    std::vector<bool> removed(moves.size()),added(moves.size());
    std::vector<std::size_t> order(moves.size());
    for(std::size_t i=0;i<order.size();++i)order[i]=i;
    auto error=BNM::TryInvoke([&]{
        // Stock move animations may already have attached either view to its
        // destination. Remove both possible memberships before adding once;
        // AddToLane inserts unconditionally and otherwise duplicates the view.
        for(std::size_t i=0;i<moves.size();++i){
            if(laneContains[oldLanes[i]](cards[i]))removeFromLane[oldLanes[i]](cards[i]);
            if(newLanes[i]!=oldLanes[i] && laneContains[newLanes[i]](cards[i]))removeFromLane[newLanes[i]](cards[i]);
            removed[i]=true;
        }
        std::stable_sort(order.begin(),order.end(),[&](auto a,auto b){return moves[a].order<moves[b].order;});
        for(auto i:order){addToLane[newLanes[i]](cards[i],moves[i].order);added[i]=true;}
    });
    const bool success=!error.IsValid();
    if(success){
        std::vector<Obj*> lanes;
        for(const auto& group:{std::cref(oldLanes),std::cref(newLanes)})for(Obj* lane:group.get())
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
                for(const auto& move:moves){
                auto* views=Core::Metadata::Bind(laneGroup,lane)(move.card);
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
            for(auto i:order)if(added[i])removeFromLane[newLanes[i]](cards[i]);
            std::stable_sort(order.begin(),order.end(),[&](auto a,auto b){return moves[a].oldOrder<moves[b].oldOrder;});
            for(auto i:order)if(removed[i])addToLane[oldLanes[i]](cards[i],moves[i].oldOrder);
        });
    }
    LOG_INFO("CustomComponents/FrontendRefresh: phase=swap-view-applied model=%llu cards=%zu success=%d",
        static_cast<unsigned long long>(swap.model),moves.size(),int(success));
    return success && visual && visual->Get();
}

inline HistoryAnimations::Progress Step(const History::Record& record,bool start){
    using Progress=HistoryAnimations::Progress;
    auto payload=std::get_if<History::SwapPayload>(&record.payload);if(!payload)return Progress::Failed;
    if(start){
        SwapRefresh swap{record.modelKey,{}};
        const auto add=[&](const auto& before,const auto& after){
            swap.moves.push_back({before.card,before.lane,before.order,after.lane,after.order});
        };
        if(payload->moves.empty()){
            add(payload->firstBefore,payload->firstAfter);
            add(payload->secondBefore,payload->secondAfter);
        }else for(const auto& move:payload->moves)add(move.before,move.after);
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
