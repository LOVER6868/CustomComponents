#pragma once
namespace CustomComponents::Core::CardTimelineStartCoordinator::TrackingPolicy {
// MatchRecorder's isolated CardViews are not registered with any live
// frontend context, and have CardId 0 until/unless wired to a model.
// Never suppress the stock timeline Start/End methods themselves.
constexpr bool IsDetachedReplayCard(int id,bool autoRegister,bool requiresContext){
    return id==0 && !autoRegister && !requiresContext;
}
}
