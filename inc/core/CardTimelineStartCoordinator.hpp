#pragma once

#include <BNM/Class.hpp>

#include "core/HookRegistry.hpp"
#include "log.h"
#include <mutex>
#include <unordered_map>
#include "core/MetadataBinding.hpp"
#include "core/SceneViewContext.hpp"

namespace CustomComponents::Core::CardTimelineStartCoordinator {
using Obj = BNM::IL2CPP::Il2CppObject;

inline void (*originalStart)(Obj*, const void*) = nullptr;
inline void (*originalEnd)(Obj*, const void*) = nullptr;
inline BNM::Field<Obj*> timelineCard;
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
    const int id=CardId(player);
    // End dispatches CompleteSignal, which can start the next timeline on the
    // same player synchronously. Retire this generation before that signal.
    if(id>=0){std::lock_guard<std::mutex> guard(stateLock);SyncScene();auto found=runningPlayers.find(player);
        if(found!=runningPlayers.end()){auto& state=states[found->second];if(state.running)--state.running;runningPlayers.erase(found);
            if(traceSamples++<256)LOG_INFO("CustomComponents/CardTimeline/END: card=%d player=%p remaining=%u",id,player,state.running);}}
    originalEnd(player,method);
}

static void StartHook(Obj* player, const void* method) {
    // CardTimelinePlayer owns animator readiness and completion. Intercepting
    // this call with a guessed view-level readiness check suppressed ordinary
    // card animations and could complete their queues before they played.
    const int id=CardId(player);
    if(id>=0){std::lock_guard<std::mutex> guard(stateLock);SyncScene();
        if(runningPlayers.emplace(player,id).second){auto& state=states[id];++state.generation;++state.running;
            if(traceSamples++<256)LOG_INFO("CustomComponents/CardTimeline/START: card=%d player=%p generation=%llu",id,player,state.generation);}}
    if (originalStart) originalStart(player, method);
}

inline bool Initialize() {
    const auto timeline = BNM::Class("PvZCards.Game", "CardTimelinePlayer");
    timelineCard=timeline.GetField("Card").cast<Obj*>();
    if (!timeline.IsValid() ||
        !Core::Hooks::Install(timeline.GetMethod("Start", 0), StartHook, originalStart) ||
        !originalStart || !timelineCard.IsValid() ||
        !Core::Hooks::Install(timeline.GetMethod("End",0),EndHook,originalEnd)) {
        LOG_ERR("CustomComponents/CardTimeline: stock Start passthrough hook unavailable");
        return false;
    }
    LOG_INFO("CustomComponents/CardTimeline: stock Start passthrough active");
    return true;
}
} // namespace CustomComponents::Core::CardTimelineStartCoordinator
