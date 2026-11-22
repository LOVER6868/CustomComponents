#pragma once

// One duration vocabulary for stock GrantAbility, Buff and ModifySunCost
// descriptors and for CustomComponents' extended grants. Durations are stored
// as the existing EffectDuration integer in native counters/modifiers, so they
// survive the game's own deep-copy, prediction and history serialization.
#include <array>
#include <charconv>
#include <string_view>
#include <BNM/Class.hpp>
#include <BNM/Field.hpp>
#include <BNM/Method.hpp>
#include "core/EnumSerializationSupport.hpp"
#include "core/HookRegistry.hpp"
#include "core/Performance.hpp"
#include "engine/AbilityDurationEvents.hpp"
#include "engine/EffectGuards.hpp"
#include "engine/RouterContext.hpp"
#include "log.h"

namespace CustomComponents::Engine::EffectDurations {
using Obj=BNM::IL2CPP::Il2CppObject;
struct Definition {std::string_view name;int id;};
inline constexpr int Permanent=0;
inline constexpr int EndOfTurn=1;
inline constexpr int NextFighter=2;
inline constexpr int StartOfTricks=3;
inline constexpr std::array<Definition,4> definitions{{
    {"Permanent",Permanent}, {"EndOfTurn",EndOfTurn},
    {"NextFighter",NextFighter}, {"StartOfTricks",StartOfTricks}
}};
inline bool Known(int duration){
    for(const auto& value:definitions)if(value.id==duration)return true;
    return false;
}
inline bool Timed(int duration){return duration!=0 && Known(duration);}
inline int Parse(std::string_view name){
    for(const auto& value:definitions)if(name==value.name)return value.id;
    if(name=="UntilEndOfTurn")return 1;
    if(name=="UntilNextFighter")return 2;
    if(name=="UntilStartOfTricks")return 3;
    int result=-1;
    auto parse=std::from_chars(name.data(),name.data()+name.size(),result);
    return parse.ec==std::errc{} && parse.ptr==name.data()+name.size() && Known(result)?result:-1;
}
inline bool RegisterEnums(){
    const auto type=BNM::Class("PvZCards.Engine.Effects","EffectDuration");
    if(!type.IsValid() || !type.GetMonoType())return false;
    // Keep stock enum-name parsing untouched. Only unsupported new values
    // need interception by DefaultDeserializer.
    for(const auto& definition:definitions)if(definition.id>2)
        if(!EnumSerializationSupport::Register(type.GetMonoType(),definition.name.data(),definition.id))return false;
    return true;
}
// All grant, triggered-grant, keyword, stat, and cost expiration consumers
// subscribe to this single channel. Duration=0 is never an expiry boundary.
inline void Emit(Obj* model,int duration){
    if(!model || !Timed(duration))return;
    AbilityDurationEvents::expire.Notify(model,duration);
}

// EffectGuards already owns the router lifecycle and has a simulation-local
// router -> EntityModel mapping. Reuse that proven integration instead of
// installing competing hooks on History or the shared EffectRouter.
// Outermost-scope detection avoids duplicate notifications if two existing
// wrappers both establish a ProcessingScope for the same phase effect.
inline thread_local unsigned tricksDepth=0;
inline bool IsTricksEffect(const EffectGuards::Context& context){
    return context.effect && context.effect->klass && context.effect->klass->name &&
        std::string_view(context.effect->klass->name)=="SurprisePhaseStartEffect";
}
inline void BeforeEffect(const EffectGuards::Context& context){
    if(!IsTricksEffect(context))return;
    if(tricksDepth++)return;
    Obj* model=Core::ModelRegistry::Get(RouterContext::RouterModel(context.router));
    if(model)Emit(model,StartOfTricks);
    else LOG_ERR("CustomComponents/Duration: StartOfTricks router has no EntityModel");
}
inline void AfterEffect(const EffectGuards::Context& context){
    if(IsTricksEffect(context) && tricksDepth)--tricksDepth;
}
inline bool Initialize(){
    return EffectGuards::before.Add(BeforeEffect) && EffectGuards::after.Add(AfterEffect);
}
} // namespace CustomComponents::Engine::EffectDurations
