#pragma once

#include <BNM/Class.hpp>
#include <BNM/Field.hpp>
#include <BNM/Method.hpp>
#include <BNM/Exceptions.hpp>

#include "core/HookRegistry.hpp"
#include "core/MatchRecorderPresentationObserver.hpp"
#include "log.h"
#include <mutex>
#include <unordered_map>
#include "core/MetadataBinding.hpp"
#include "core/SceneViewContext.hpp"
#include "core/CardTimelineTrackingPolicy.hpp"

// Optional same-process observer; never require MatchRecorder to be installed.
// The compiler resolves these weak calls only when that Bloom module is built.
extern "C" void MatchRecorder_ObserveNativeTimelineEnd(void*) __attribute__((weak));
extern "C" void MatchRecorder_EnableNativeTimelineEndObserver() __attribute__((weak));

namespace CustomComponents::Core::CardTimelineStartCoordinator {
using Obj = BNM::IL2CPP::Il2CppObject;

inline void (*originalStart)(Obj*, const void*) = nullptr;
inline void (*originalEnd)(Obj*, const void*) = nullptr;
inline BNM::Field<Obj*> timelineCard;
// Replay creates detached CardViewComponents. Their private views deliberately
// have no live model/context and GetCardEntityId() is 0. Do not accumulate
// those timelines in the LIVE frontend combat-facing generation ledger.
inline BNM::Method<bool> viewAutoRegister, viewRequiresContext;
inline unsigned skippedReplayStarts=0;
inline bool DetachedReplayCard(Obj* player,int id){
    if(id!=0 || !player || !timelineCard.IsValid() ||
       !viewAutoRegister.IsValid() || !viewRequiresContext.IsValid())return false;
    auto* card=Core::Metadata::Bind(timelineCard,player).Get();
    if(!card)return false;
    bool shouldIgnore=false;
    const auto exception=BNM::TryInvoke([&]{
        const bool automatic=Core::Metadata::Bind(viewAutoRegister,card)();
        const bool needsContext=Core::Metadata::Bind(viewRequiresContext,card)();
        shouldIgnore=TrackingPolicy::IsDetachedReplayCard(id,automatic,needsContext);
    });
    return !exception.IsValid() && shouldIgnore;
}
struct State {unsigned long long generation=0;unsigned running=0;};
inline std::mutex stateLock;
inline std::unordered_map<int,State> states;
inline std::unordered_map<const void*,int> runningPlayers;
inline std::uintptr_t scene=0;
inline unsigned traceSamples=0;
inline void SyncScene(){auto current=SceneViewContext::ModelIdentity();if(current!=scene){states.clear();runningPlayers.clear();traceSamples=0;scene=current;}}
inline int CardId(Obj* player){
    Obj* card=player?Core::Metadata::Bind(timelineCard,player).Get():nullptr;
    auto get=card?BNM::Class(card).GetMethod("GetCardEntityId",0).cast<int>():BNM::Method<int>{};
    return get.IsValid()?Core::Metadata::Bind(get,card)():-1;
}
inline State Read(int card){std::lock_guard<std::mutex> guard(stateLock);SyncScene();auto it=states.find(card);return it==states.end()?State{}:it->second;}
inline bool Busy(){std::lock_guard<std::mutex> guard(stateLock);SyncScene();for(const auto& [id,state]:states)if(state.running)return true;return false;}
static void EndHook(Obj* player,const void* method){
    CustomComponents::Core::ObserveReplayPresentation("card-timeline-end",player,nullptr);
    // The Card reference may be cleared or replaced during animation exit.
    // Always retire the *ID stored at Start*, never re-resolve it at End.
    // End dispatches CompleteSignal synchronously; retire before invoking it.
    if(MatchRecorder_ObserveNativeTimelineEnd)MatchRecorder_ObserveNativeTimelineEnd(player);
    {std::lock_guard<std::mutex> guard(stateLock);SyncScene();
        auto found=runningPlayers.find(player);
        if(found!=runningPlayers.end()){
            const int id=found->second;
            auto& state=states[id];if(state.running)--state.running;
            runningPlayers.erase(found);
            if(traceSamples++<256)LOG_INFO("CustomComponents/CardTimeline/END: card=%d player=%p remaining=%u",id,player,state.running);
        }
    }
    if(originalEnd)originalEnd(player,method);
}

static void StartHook(Obj* player, const void* method) {
    // CardTimelinePlayer owns animator readiness and completion. Intercepting
    // this call with a guessed view-level readiness check suppressed ordinary
    // card animations and could complete their queues before they played.
    const int id=CardId(player);
    CustomComponents::Core::ObserveReplayPresentation("card-timeline-start",player,nullptr,-1,id);
    const bool detached=DetachedReplayCard(player,id);
    if(detached){
        if(skippedReplayStarts++<16)
            LOG_INFO("CustomComponents/CardTimeline: ignored detached replay player=%p card=%d",player,id);
    }else if(id>=0){std::lock_guard<std::mutex> guard(stateLock);SyncScene();
        if(runningPlayers.emplace(player,id).second){auto& state=states[id];++state.generation;++state.running;
            if(traceSamples++<256)LOG_INFO("CustomComponents/CardTimeline/START: card=%d player=%p generation=%llu",id,player,state.generation);}}
    if (originalStart) originalStart(player, method);
}

inline bool Initialize() {
    const auto timeline = BNM::Class("PvZCards.Game", "CardTimelinePlayer");
    timelineCard=timeline.GetField("Card").cast<Obj*>();
    const auto view=BNM::Class("strange.extensions.mediation.impl","View");
    viewAutoRegister=view.GetMethod("get_autoRegisterWithContext",0).cast<bool>();
    viewRequiresContext=view.GetMethod("get_requiresContext",0).cast<bool>();
    // If these accessors aren't available, the original Start/End hooks still
    // run, but replay exclusion is disabled rather than guessed.
    if(!viewAutoRegister.IsValid() || !viewRequiresContext.IsValid())
        LOG_WARN("CustomComponents/CardTimeline: detached replay view detection unavailable");
    if (!timeline.IsValid() ||
        !Core::Hooks::Install(timeline.GetMethod("Start", 0), StartHook, originalStart) ||
        !originalStart || !timelineCard.IsValid() ||
        !Core::Hooks::Install(timeline.GetMethod("End",0),EndHook,originalEnd)) {
        LOG_ERR("CustomComponents/CardTimeline: stock Start passthrough hook unavailable");
        return false;
    }
    if(MatchRecorder_EnableNativeTimelineEndObserver)
        MatchRecorder_EnableNativeTimelineEndObserver();
    LOG_INFO("CustomComponents/CardTimeline: stock Start passthrough active; replay end observer=%d",
             MatchRecorder_ObserveNativeTimelineEnd?1:0);
    return true;
}
} // namespace CustomComponents::Core::CardTimelineStartCoordinator
