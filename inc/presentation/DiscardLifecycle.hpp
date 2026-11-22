#pragma once
#include <atomic>
#include <map>
#include <mutex>
#include <vector>
#include <BNM/Exceptions.hpp>
#include "core/HookRegistry.hpp"
#include "core/SceneViewContext.hpp"
#include "core/ModelRegistry.hpp"
#include "core/PresentationInvalidation.hpp"
#include "engine/EffectGuards.hpp"
#include "engine/FrontendStateRefresh.hpp"
#include "core/CardViewLifecycleCoordinator.hpp"
namespace CustomComponents::Presentation::DiscardLifecycle {
using Obj=BNM::IL2CPP::Il2CppObject;
// Model removal happens during simulation, whereas the visible CardView is
// owned by the frontend history player. Retain only a Discard card's view
// mapping until its actual move/discard history record is processed. Do not
// postpone any authoritative model or zone mutation (prediction/PvP safety).
struct PendingVisual {
    std::shared_ptr<Core::ManagedRuntime::Root> mapper;
    const void* removeMethod=nullptr;
    bool deferred=false;
    bool historyCompleted=false;
    const void* historyRecord=nullptr;
    unsigned idleUpdates=0;
};
inline std::mutex visualMutex;
using VisualKey=std::pair<Core::ModelRegistry::Token,int>;
inline std::map<VisualKey,PendingVisual> visualPending;
inline VisualKey KeyFor(Obj* model,int id){return {Core::ModelRegistry::Identify(model,false),id};}
inline Obj* MapperModel(Obj* mapper){
    static auto field=BNM::Class("PvZCards.Game","EntityViewMapper")
        .GetField("<EntityModel>k__BackingField").cast<Obj*>();
    return mapper && field.IsValid()?field[mapper].Get():nullptr;
}
inline void StageVisual(Obj* model,int id){
    if(id<0 || !model)return;
    // All future creations of this physical card must wait for its old view,
    // including native creation effects, not only explicit discard returns.
    if(reinterpret_cast<std::uintptr_t>(model)==Core::SceneViewContext::ModelIdentity())
        Core::CardViewLifecycleCoordinator::NoteReturn(id,-1,-1);
    const auto key=VisualKey{Core::ModelRegistry::Identify(model,true),id};
    if(!key.first)return;
    std::lock_guard<std::mutex> guard(visualMutex);
    if(visualPending.size()>256)visualPending.clear();
    visualPending.try_emplace(key);
}
// The hand presentation adapter may only release a ticket after the stock
// view mapper has actually attempted to unmap THIS card, not merely because
// unrelated EffectAnimationQueue.Update() happened to run.
inline bool HasDeferredVisual(Obj* mapper,int id){
    Obj* model=MapperModel(mapper);
    const auto key=KeyFor(model,id);
    std::lock_guard<std::mutex> guard(visualMutex);
    auto it=visualPending.find(key);
    return key.first && it!=visualPending.end() && it->second.deferred;
}
inline bool AwaitingVisualHistory(Obj* mapper,int id){
    const auto key=KeyFor(MapperModel(mapper),id);
    std::lock_guard<std::mutex> guard(visualMutex);
    auto it=visualPending.find(key);
    return key.first && it!=visualPending.end() && !it->second.historyCompleted;
}
inline bool HasVisual(Obj* mapper,int id){
    Obj* model=MapperModel(mapper);
    const auto key=KeyFor(model,id);
    std::lock_guard<std::mutex> guard(visualMutex);
    return key.first && visualPending.find(key)!=visualPending.end();
}
// A stock ability expiry can enqueue removal animations BEFORE a Discard
// discard moves the card away. By the time the frontend consumes them the
// original CardView / AbilityGrantableTarget has been unmapped. Only suppress
// this obsolete *presentation* for an already-discarded Discard card; model
// state and all non-Discard animation/records retain stock processing.
inline bool MissingDiscardView(Obj* mapper,int id){
    if(!mapper || id<0)return false;
    auto field=BNM::Class("PvZCards.Game","EntityViewMapper").GetField("<EntityModel>k__BackingField").cast<Obj*>();
    Obj* model=field.IsValid()?field[mapper].Get():nullptr;
    Obj* card=Engine::EntityAccess::Find(model,id);
    return card && 
        (Engine::EntityAccess::Component(card,"DiscardPileElement") ||
         Engine::EntityAccess::Component(card,"DeckElement"));
}
inline void(*originalRemoveRecord)(Obj*,Obj*,const void*)=nullptr;
static void RemoveRecord(Obj* self,Obj* record,const void* method){
    static auto mapperField=BNM::Class("PvZCards.Game","AbilityRemovedRecordProcessor").GetField("<EntityViewMapper>k__BackingField").cast<Obj*>();
    static auto idMethod=BNM::Class("PvZCards.Engine","Record").GetMethod("GetEntityId",1).cast<int>();
    Obj* mapper=mapperField.IsValid()?mapperField[self].Get():nullptr;
    int id=record && idMethod.IsValid()?idMethod[record](1):-1; // RecordKey.TargetId
    if(MissingDiscardView(mapper,id)){
        static std::atomic<unsigned> n{0};if(n.fetch_add(1)<24)
            LOG_INFO("CustomComponents/Discard/SKIP_STALE_RECORD: card=%d",id);
        return;
    }
    originalRemoveRecord(self,record,method);
    // Stock removal animations do not guarantee a full frame-sprite rebuild.
    // Reconcile the actual model's active traits when the presentation queue
    // becomes idle. This cannot remove innate traits or gameplay Armor.
    Core::PresentationInvalidation::Card(id);
}
inline void(*originalRemoveAnimation)(Obj*,const void*)=nullptr;
static void RemoveAnimation(Obj* self,const void* method){
    auto type=BNM::Class("PvZCards.Game","RemoveGrantableAbilityAnimation");
    static auto mapField=type.GetField("<ViewMap>k__BackingField").cast<Obj*>();
    static auto cardField=type.GetField("CardId").cast<int>();
    Obj* mapper=mapField.IsValid()?mapField[self].Get():nullptr;
    int id=cardField.IsValid()?cardField[self].Get():-1;
    if(MissingDiscardView(mapper,id)){
        // FinishAnimation emits the completion signal; returning without this
        // leaves the animation queue waiting even though nothing is drawable.
        static auto finish=BNM::Class("PvZCards.Game","EffectAnimation").GetMethod("FinishAnimation",0).cast<void>();
        if(finish.IsValid()){
            static std::atomic<unsigned> n{0};if(n.fetch_add(1)<24)
                LOG_INFO("CustomComponents/Discard/SKIP_STALE_ANIMATION: card=%d",id);
            finish[self]();return;
        }
    }
    originalRemoveAnimation(self,method);
}
// The frontend may unmap a view when it observes a future model revision,
// before its own history consumer has reached the discard/destroy record.
// Only defer if this is a specific queued Discard card already outside play.
inline void(*originalUnmapView)(Obj*,int,const void*)=nullptr;
static void UnmapView(Obj* mapper,int id,const void* method){
    // RemoveViewObject is registry cleanup, not an animation barrier. The
    // Unity object/collider can already be destroyed here. Retaining its
    // mapping makes board hit-testing call Collider2D.OverlapPoint on a dead
    // object (observed in the capture). Let stock retire the mapping now.
    if(Core::CardViewLifecycleCoordinator::IsReturned(id) && Core::CardViewLifecycleCoordinator::getView.IsValid()){
        Obj* oldView=Core::Metadata::Bind(Core::CardViewLifecycleCoordinator::getView,mapper)(id);
        Core::CardViewLifecycleCoordinator::TrackRetiringView(id,oldView);
    }
    originalUnmapView(mapper,id,method);
}
inline void FinishVisualRecord(int id,Obj* frontendMapper=nullptr){
    if(id<0)return;
    if(!frontendMapper)frontendMapper=Core::SceneViewContext::RootedMapper();
    const auto key=KeyFor(MapperModel(frontendMapper),id);
    PendingVisual pending;
    {
        std::lock_guard<std::mutex> guard(visualMutex);
        if(!key.first)return;
        auto it=visualPending.find(key);
        if(it==visualPending.end())return;
        pending=std::move(it->second);
        visualPending.erase(it);
    }
    // Only the matched frontend mapper is released; other simulation/prediction
    // models may contain the same recycled entity id.
    Obj* mapper=pending.mapper?pending.mapper->Get():nullptr;
    if(pending.deferred && mapper && originalUnmapView)
        originalUnmapView(mapper,id,pending.removeMethod);
    static std::atomic<unsigned> logCount{0};
    if(logCount.fetch_add(1)<70)
        LOG_INFO("CustomComponents/Discard/FRONTEND_UNMAP_RELEASE: modelToken=%llu card=%d deferred=%d",
            static_cast<unsigned long long>(key.first),id,int(pending.deferred));
}
// A history processor may consume the matching move record before its
// timeline animation completes. Keep the mapper entry until the shared
// frontend queue reports two consecutive idle updates.
inline void HistoryRecordProcessed(int id,Obj* frontendMapper=nullptr,Obj* record=nullptr){
    if(id<0)return;
    if(!frontendMapper)frontendMapper=Core::SceneViewContext::RootedMapper();
    const auto key=KeyFor(MapperModel(frontendMapper),id);
    if(!key.first)return;
    std::lock_guard<std::mutex> guard(visualMutex);
    auto it=visualPending.find(key);
    if(it!=visualPending.end()){
        it->second.historyCompleted=true;
        it->second.historyRecord=record;
        it->second.idleUpdates=0;
        static std::atomic<unsigned> logs{0};
        if(logs.fetch_add(1)<128)
            LOG_INFO("CustomComponents/Discard/HISTORY_BOUND: card=%d record=%p mapper=%p model=%p deferred=%d",
                id,record,frontendMapper,MapperModel(frontendMapper),int(it->second.deferred));
    }
}
inline void AfterAnimationQueueUpdate(bool idle){
    struct Release {int id;Obj* mapper;const void* method;bool deferred;};
    std::vector<Release> ready;
    {
        std::lock_guard<std::mutex> guard(visualMutex);
        for(auto it=visualPending.begin();it!=visualPending.end();){
            auto& pending=it->second;
            if(!pending.historyCompleted){++it;continue;}
            if(!idle){pending.idleUpdates=0;++it;continue;}
            if(++pending.idleUpdates<2){++it;continue;}
            ready.push_back({it->first.second,pending.mapper?pending.mapper->Get():nullptr,
                pending.removeMethod,pending.deferred});
            it=visualPending.erase(it);
        }
    }
    for(const auto& item:ready){
        if(item.deferred && item.mapper && originalUnmapView)
            originalUnmapView(item.mapper,item.id,item.method);
        static std::atomic<unsigned> logCount{0};
        if(logCount.fetch_add(1)<70)
            LOG_INFO("CustomComponents/Discard/ANIMATION_IDLE: card=%d mapper=%p deferred=%d",
                item.id,item.mapper,int(item.deferred));
    }
}
inline int HistoryTarget(Obj* record){
    static auto getter=BNM::Class("PvZCards.Engine","Record").GetMethod("GetEntityId",1).cast<int>();
    return record && getter.IsValid()?getter[record](1):-1;
}
inline void(*originalBoardHistory)(Obj*,Obj*,const void*)=nullptr;
static void BoardHistory(Obj* self,Obj* record,const void* method){
    const int id=HistoryTarget(record);
    originalBoardHistory(self,record,method);
    HistoryRecordProcessed(id,nullptr,record);
}
static void CardMoveHistory(Obj* record){
    const int id=HistoryTarget(record);
    HistoryRecordProcessed(id,nullptr,record);
}

inline BNM::Class discardFromPlayType,moveToDiscardType;
inline BNM::Field<int> targetField;
inline void ObserveBoardDiscard(const Engine::EffectGuards::Context& context){
    if(!context.effect || (!BNM::IsA(context.effect,discardFromPlayType) && !BNM::IsA(context.effect,moveToDiscardType)))return;
    Obj* model=Core::ModelRegistry::Get(Engine::RouterContext::RouterModel(context.router));
    const int id=Core::Metadata::Bind(targetField,context.effect).Get();
    if(Engine::EntityAccess::Component(Engine::EntityAccess::Find(model,id),"LaneElement"))StageVisual(model,id);
}
inline bool Initialize(){
    discardFromPlayType=BNM::Class("PvZCards.Engine.Effects","DiscardFromPlayEffect");
    moveToDiscardType=BNM::Class("PvZCards.Engine.Effects","MoveToDiscardPileEffect");
    targetField=BNM::Class("PvZCards.Engine","Effect").GetField("TargetId").cast<int>();
    return discardFromPlayType.IsValid() && moveToDiscardType.IsValid() && targetField.IsValid() &&
        Engine::EffectGuards::before.Add(ObserveBoardDiscard) &&
        Core::Hooks::Install(BNM::Class("PvZCards.Game","AbilityRemovedRecordProcessor").GetMethod("ProcessRecord",1),RemoveRecord,originalRemoveRecord) &&
        Core::Hooks::Install(BNM::Class("PvZCards.Game","RemoveGrantableAbilityAnimation").GetMethod("OnStarted",0),RemoveAnimation,originalRemoveAnimation) &&
        Core::Hooks::Install(BNM::Class("PvZCards.Game","EntityViewMapper").GetMethod("RemoveViewObject",1),UnmapView,originalUnmapView) &&
        Core::Hooks::Install(BNM::Class("PvZCards.Game","DiscardFromPlayRecordProcessor").GetMethod("ProcessRecord",1),BoardHistory,originalBoardHistory) &&
        Engine::FrontendStateRefresh::afterMoveRecord.Add(CardMoveHistory);
}
}
