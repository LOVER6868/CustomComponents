#pragma once
#include "engine/MatchAccess.hpp"
#include "core/Performance.hpp"

namespace CustomComponents::Engine::ContinuousEffects {
enum class Boundary {Ready,Pending,Unavailable};
// A staged action must read stats after native continuous application effects
// have drained, rather than before the router's all-effect processors run.
inline Boundary BeforeAction(Match::Obj* model,Match::Obj* instances,Match::Obj* effect){
    using Obj=Match::Obj;
    Core::Performance::Scope timing(Core::Performance::Area::Maintenance,"continuous-action-boundary");
    static auto checker=BNM::Class("PvZCards.Core.Systems","ContinuousEffectSystem");
    static auto queuer=BNM::Class("PvZCards.Core.Systems","ContinuousEffectQueuingSystem");
    static auto collection=BNM::Class("PvZCards.Engine","ContinuousEffectCollection");
    static auto owner=checker.GetField("<Model>k__BackingField").cast<Obj*>();
    static auto check=checker.GetMethod("CheckContinuousEffects",1).cast<void>();
    static auto enqueue=queuer.GetMethod("QueueContinuousEffects",0).cast<void>();
    static auto accepted=collection.GetField("CanAcceptMoreEffects").cast<bool>();
    static auto effects=collection.GetField("Effects").cast<EntityAccess::List*>();
    if(!owner.IsValid() || !check.IsValid() || !enqueue.IsValid() || !accepted.IsValid() || !effects.IsValid())return Boundary::Unavailable;
    Obj* pending=Match::Provide(instances,collection.GetMonoType());
    if(!pending)return Boundary::Unavailable;
    if(!Core::Metadata::Bind(accepted,pending).Get())return Boundary::Pending;
    auto system=Match::Service(instances,checker);
    if(!system || Core::Metadata::Bind(owner,system->Get()).Get()!=model)return Boundary::Unavailable;
    Core::Metadata::Bind(check,system->Get())(effect);
    auto* list=Core::Metadata::Bind(effects,pending).Get();
    if(!EntityAccess::Valid(list))return Boundary::Unavailable;
    if(!list->size)return Boundary::Ready;
    auto queue=Match::Service(instances,queuer);
    if(!queue)return Boundary::Unavailable;
    const int count=list->size;
    Core::Metadata::Bind(enqueue,queue->Get())();
    LOG_INFO("CustomComponents/ContinuousEffects: before-action queued=%d",count);
    return Boundary::Pending;
}
}
