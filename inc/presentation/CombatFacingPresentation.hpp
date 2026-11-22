#pragma once
#include "history/DomainRecords.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <mutex>
#include <vector>

#include <BNM/Class.hpp>
#include <BNM/Method.hpp>
#include <BNM/UnityStructures.hpp>

#include "core/AnimationQueuePostUpdate.hpp"
#include "core/ModelRegistry.hpp"
#include "core/SceneViewContext.hpp"
#include "core/CardTimelineStartCoordinator.hpp"
#include "engine/FrontendServices.hpp"
#include "engine/BoardPlacement.hpp"
#include "abilities/LaneAttacks.hpp"
#include "presentation/CombatFacingPolicy.hpp"
#include "log.h"

namespace CustomComponents::Presentation::CombatFacing {
using Obj = BNM::IL2CPP::Il2CppObject;
using Token = Core::ModelRegistry::Token;
using Vec3 = BNM::Structures::Unity::Vector3;

struct Request { Token model=0; int first=-1; int second=-1; bool begin=false; unsigned long long firstGeneration=0,secondGeneration=0; std::uint64_t historyId=0; };
struct Encounter {
    Token model=0;
    int first=-1,second=-1;
    float started=0.f;
    float firstOriginal=0.f,secondOriginal=0.f;
    bool finishRequested=false;
    bool restoring=false;
    unsigned long long firstGeneration=0,secondGeneration=0;
    std::uint64_t historyId=0;
};

inline std::mutex requestLock;
inline std::vector<Request> requests;
inline std::vector<Encounter> active;
inline BNM::Method<Obj*> getViewObject,getTransform,getGameObject;
inline BNM::Method<Vec3> getPosition,getEulerAngles;
inline BNM::Method<Obj*> rotateZ;
inline BNM::Method<float> getTime;
inline BNM::Method<bool> queueIsEmpty;
inline std::shared_ptr<Core::ManagedRuntime::Root> liveQueue;
inline std::uintptr_t queueScene=0;
inline constexpr float kTurnSeconds=0.24f;
inline bool enabled=true;

inline void Queue(Obj* model,int first,int second,bool begin,std::uint64_t historyId=0) {
    if(!model || first<0 || second<0 || first==second)return;
    if(!enabled)return;
    if(begin){
        Obj* fighter=Engine::EntityAccess::Find(model,first);
        if(Engine::EntityAccess::Component(fighter,"AttacksInAllLanes") || Abilities::LaneAttacks::OnlyActive(fighter))return;
    }
    if(reinterpret_cast<std::uintptr_t>(model)!=Core::SceneViewContext::ModelIdentity())return;
    if(begin){
        static auto laneIds=BNM::Class("PvZCards.Engine.Components","LaneElement").GetField("LaneIds").cast<BNM::Structures::Mono::List<int>*>();
        auto lanes=[&](int id){
            Obj* element=Engine::EntityAccess::Component(Engine::EntityAccess::Find(model,id),"LaneElement");
            return element && laneIds.IsValid()?Core::Metadata::Bind(laneIds,element).Get():nullptr;
        };
        auto* a=lanes(first);auto* b=lanes(second);
        if(Engine::EntityAccess::Valid(a) && Engine::EntityAccess::Valid(b))
            for(int i=0;i<a->size;++i)for(int j=0;j<b->size;++j)
                if(a->items->m_Items[i]==b->items->m_Items[j])return;
    }
    const Token token=Core::ModelRegistry::Identify(model,true);
    if(!token)return;
    std::lock_guard<std::mutex> guard(requestLock);
    if(requests.size()<64)requests.push_back({token,first,second,begin,
        Core::CardTimelineStartCoordinator::Read(first).generation,
        Core::CardTimelineStartCoordinator::Read(second).generation,historyId});
    LOG_INFO("CustomComponents/CombatFacing: request=%s model=%llu first=%d second=%d queued=%d",
        begin?"begin":"restore",static_cast<unsigned long long>(token),first,second,int(requests.size()<64));
}

inline Obj* View(Obj* mapper,int id) {
    if(!mapper || !getViewObject.IsValid())return nullptr;
    Obj* view=nullptr;
    auto error=BNM::TryInvoke([&]{view=getViewObject[mapper](id);});
    return error.IsValid()?nullptr:view;
}

inline bool ResolveCard(Obj* mapper,int id,Obj*& gameObject,Vec3& position,float& angle) {
    Obj* view=View(mapper,id);
    if(!view || !getTransform.IsValid() || !getGameObject.IsValid())return false;
    Obj* transform=nullptr;
    auto error=BNM::TryInvoke([&]{transform=getTransform[view]();gameObject=getGameObject[view]();});
    if(error.IsValid() || !transform || !gameObject || !getPosition.IsValid() || !getEulerAngles.IsValid())return false;
    Vec3 euler{};
    error=BNM::TryInvoke([&]{position=getPosition[transform]();euler=getEulerAngles[transform]();});
    if(error.IsValid())return false;
    angle=euler.z;
    return true;
}

inline bool Begin(Obj* mapper,Encounter& encounter,float now) {
    Obj* firstObject=nullptr;Obj* secondObject=nullptr;Vec3 firstPosition{},secondPosition{};
    float firstAngle=0.f,secondAngle=0.f;
    const bool firstReady=ResolveCard(mapper,encounter.first,firstObject,firstPosition,firstAngle);
    const bool secondReady=ResolveCard(mapper,encounter.second,secondObject,secondPosition,secondAngle);
    if(!firstReady || !secondReady || !rotateZ.IsValid()){
        LOG_WARN("CustomComponents/CombatFacing: begin failed first=%d view=%d second=%d view=%d rotate=%d",
            encounter.first,int(firstReady),encounter.second,int(secondReady),int(rotateZ.IsValid()));
        return false;
    }
    // A card's local up axis is its facing direction. Point each card toward
    // the other's live screen position, then restore its original world angle.
    constexpr float radiansToDegrees=57.2957795131f;
    float firstTarget=std::atan2(secondPosition.y-firstPosition.y,secondPosition.x-firstPosition.x)*radiansToDegrees-90.f;
    float secondTarget=std::atan2(firstPosition.y-secondPosition.y,firstPosition.x-secondPosition.x)*radiansToDegrees-90.f;
    Obj* model=Core::ModelRegistry::Get(encounter.model);
    Obj* game=Engine::FrontendServices::Game();
    static auto localPlayer=BNM::Class("PvZCards.Core.Utilities","GameModel").GetField("LocalPlayerId").cast<int>();
    if(!model || !game || !localPlayer.IsValid())return false;
    const int local=Core::Metadata::Bind(localPlayer,game).Get();
    auto directed=[&](int id,float original,float desired,bool left){
        Obj* card=Engine::EntityAccess::Find(model,id);
        const bool top=Engine::BoardPlacement::Owner(card)!=local;
        const bool plant=Engine::EntityAccess::Component(card,"Plants")!=nullptr;
        return CombatFacingPolicy::Target(original,desired,top,left,plant);
    };
    firstTarget=directed(encounter.first,firstAngle,firstTarget,firstPosition.x<=secondPosition.x);
    secondTarget=directed(encounter.second,secondAngle,secondTarget,secondPosition.x<firstPosition.x);
    bool firstStarted=false,secondStarted=false;
    auto error=BNM::TryInvoke([&]{rotateZ[static_cast<Obj*>(nullptr)](firstObject,firstTarget,kTurnSeconds);firstStarted=true;});
    if(error.IsValid() || !firstStarted){
        LOG_WARN("CustomComponents/CombatFacing: first rotation failed card=%d",encounter.first);
        return false;
    }
    error=BNM::TryInvoke([&]{rotateZ[static_cast<Obj*>(nullptr)](secondObject,secondTarget,kTurnSeconds);secondStarted=true;});
    if(error.IsValid() || !secondStarted){
        BNM::TryInvoke([&]{rotateZ[static_cast<Obj*>(nullptr)](firstObject,firstAngle,kTurnSeconds);});
        LOG_WARN("CustomComponents/CombatFacing: second rotation failed card=%d",encounter.second);
        return false;
    }
    encounter.firstOriginal=firstAngle;encounter.secondOriginal=secondAngle;
    encounter.started=now;encounter.restoring=false;
    History::Domain::Presentation(model,"combat-facing","started",{encounter.first,encounter.second},encounter.historyId,firstTarget,secondTarget,kTurnSeconds);
    LOG_INFO("CustomComponents/CombatFacing: phase=turned model=%llu first=%d angle=%.1f second=%d angle=%.1f duration=%.2f",
        static_cast<unsigned long long>(encounter.model),encounter.first,firstTarget,
        encounter.second,secondTarget,kTurnSeconds);
    return true;
}

inline void Restore(Obj* mapper,Encounter& encounter,float now) {
    Obj* firstObject=nullptr;Obj* secondObject=nullptr;Vec3 position{};float current=0.f;
    const bool firstReady=ResolveCard(mapper,encounter.first,firstObject,position,current);
    const bool secondReady=ResolveCard(mapper,encounter.second,secondObject,position,current);
    bool firstStarted=false,secondStarted=false;
    if(rotateZ.IsValid() && firstReady)BNM::TryInvoke([&]{rotateZ[static_cast<Obj*>(nullptr)](firstObject,encounter.firstOriginal,kTurnSeconds);firstStarted=true;});
    if(rotateZ.IsValid() && secondReady)BNM::TryInvoke([&]{rotateZ[static_cast<Obj*>(nullptr)](secondObject,encounter.secondOriginal,kTurnSeconds);secondStarted=true;});
    History::Domain::Presentation(Core::ModelRegistry::Get(encounter.model),"combat-facing","restore-started",
        {encounter.first,encounter.second},encounter.historyId,encounter.firstOriginal,encounter.secondOriginal,kTurnSeconds);
    encounter.restoring=true;encounter.started=now;
    LOG_INFO("CustomComponents/CombatFacing: phase=restore model=%llu first=%d started=%d second=%d started=%d",
        static_cast<unsigned long long>(encounter.model),encounter.first,int(firstStarted),encounter.second,int(secondStarted));
}

inline void Tick(void* queue,const void*) {
    const auto scene=Core::SceneViewContext::ModelIdentity();
    Obj* mapper=Core::SceneViewContext::MapperForModel(scene);
    if(!scene || !mapper || !getTime.IsValid())return;
    if(queueScene!=scene){
        for(const auto& encounter:active)History::Domain::Presentation(Core::ModelRegistry::Get(encounter.model),"combat-facing","cancelled-scene",{encounter.first,encounter.second},encounter.historyId);
        liveQueue.reset();active.clear();queueScene=scene;
    }
    bool hasRequests=false;
    {std::lock_guard<std::mutex> guard(requestLock);hasRequests=!requests.empty();}
    if(active.empty() && !hasRequests)return;
    if(!liveQueue || !liveQueue->Get()){
        Obj* owner=Engine::FrontendServices::Provide(BNM::Class("PvZCards.Game","EffectAnimationQueue"),Engine::FrontendServices::Game());
        if(owner)liveQueue=std::make_shared<Core::ManagedRuntime::Root>(owner,true);
    }
    // Several queues are updated by the frontend. An unrelated empty queue
    // cannot declare these attacks complete.
    if(!liveQueue || liveQueue->Get()!=queue)return;
    float now=0.f;
    auto timeError=BNM::TryInvoke([&]{now=getTime[static_cast<Obj*>(nullptr)]();});
    if(timeError.IsValid())return;
    std::vector<Request> pending;
    {std::lock_guard<std::mutex> guard(requestLock);pending.swap(requests);}
    for(const auto& request:pending){
        if(reinterpret_cast<std::uintptr_t>(Core::ModelRegistry::Get(request.model))!=scene)continue;
        auto found=std::find_if(active.begin(),active.end(),[&](const Encounter& e){return e.model==request.model&&e.first==request.first&&e.second==request.second;});
        if(request.begin){
            if(found!=active.end())continue;
            Encounter encounter{request.model,request.first,request.second};
            encounter.historyId=request.historyId;
            encounter.firstGeneration=request.firstGeneration;encounter.secondGeneration=request.secondGeneration;
            if(Begin(mapper,encounter,now))active.push_back(encounter);
            else History::Domain::Presentation(Core::ModelRegistry::Get(request.model),"combat-facing","failed-missing-view",{request.first,request.second},request.historyId);
        }else if(found!=active.end())found->finishRequested=true;
        else LOG_WARN("CustomComponents/CombatFacing: restore request without begin model=%llu first=%d second=%d",
            static_cast<unsigned long long>(request.model),request.first,request.second);
    }
    for(auto it=active.begin();it!=active.end();){
        if(reinterpret_cast<std::uintptr_t>(Core::ModelRegistry::Get(it->model))!=scene){
            it=active.erase(it);continue;
        }
        bool presentationFinished=false;
        if(queue && queueIsEmpty.IsValid())
            BNM::TryInvoke([&]{presentationFinished=queueIsEmpty[static_cast<Obj*>(queue)]();});
        const auto first=Core::CardTimelineStartCoordinator::Read(it->first);
        const auto second=Core::CardTimelineStartCoordinator::Read(it->second);
        const bool played=first.generation>it->firstGeneration || second.generation>it->secondGeneration;
        // Future queue entries (turn buttons, another ability) are not part
        // of this encounter. Use the fighters' actual timeline completion.
        presentationFinished=presentationFinished || (played && !first.running && !second.running);
        if(!it->restoring && it->finishRequested && presentationFinished && now-it->started>=kTurnSeconds)Restore(mapper,*it,now);
        else if(it->restoring && now-it->started>=kTurnSeconds){
            LOG_INFO("CustomComponents/CombatFacing: phase=complete model=%llu first=%d second=%d",
                static_cast<unsigned long long>(it->model),it->first,it->second);
            History::Domain::Presentation(Core::ModelRegistry::Get(it->model),"combat-facing","complete",{it->first,it->second},it->historyId);
            it=active.erase(it);continue;
        }
        ++it;
    }
}

inline bool Initialize(){
    auto mapper=BNM::Class("PvZCards.Game","EntityViewMapper");
    auto component=BNM::Class("UnityEngine","Component");
    auto transform=BNM::Class("UnityEngine","Transform");
    auto leanTween=BNM::Class("","LeanTween");
    auto time=BNM::Class("UnityEngine","Time");
    if(!mapper.IsValid() || !component.IsValid() || !transform.IsValid() || !leanTween.IsValid() || !time.IsValid())return false;
    getViewObject=mapper.GetMethod("GetViewObject",{std::string_view("entityId")}).cast<Obj*>();
    getTransform=component.GetMethod("get_transform",0).cast<Obj*>();
    getGameObject=component.GetMethod("get_gameObject",0).cast<Obj*>();
    getPosition=transform.GetMethod("get_position",0).cast<Vec3>();
    getEulerAngles=transform.GetMethod("get_eulerAngles",0).cast<Vec3>();
    rotateZ=leanTween.GetMethod("rotateZ",3).cast<Obj*>();
    getTime=time.GetMethod("get_time",0).cast<float>();
    queueIsEmpty=BNM::Class("PvZCards.Game","EffectAnimationQueue").GetMethod("IsEmpty",0).cast<bool>();
    if(!getViewObject.IsValid() || !getTransform.IsValid() || !getGameObject.IsValid() ||
       !getPosition.IsValid() || !getEulerAngles.IsValid() || !rotateZ.IsValid() || !getTime.IsValid() || !queueIsEmpty.IsValid())return false;
    return Core::AnimationQueuePostUpdate::Register(Tick);
}
} // namespace CustomComponents::Presentation::CombatFacing
