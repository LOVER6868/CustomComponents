#pragma once

// One duration vocabulary for stock GrantAbility, Buff and ModifySunCost
// descriptors and for CustomComponents' extended grants. Durations are stored
// as the existing EffectDuration integer in native counters/modifiers, so they
// survive the game's own deep-copy, prediction and history serialization.
#include <array>
#include <charconv>
#include <cstdint>
#include <optional>
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
enum class Duration : std::int32_t {Permanent=0,EndOfTurn=1,NextFighter=2,StartOfTricks=3};
inline constexpr int ToNative(Duration duration){return static_cast<int>(duration);}
struct Definition {std::string_view name;Duration id;};
inline constexpr std::array<Definition,4> definitions{{
    {"Permanent",Duration::Permanent}, {"EndOfTurn",Duration::EndOfTurn},
    {"NextFighter",Duration::NextFighter}, {"StartOfTricks",Duration::StartOfTricks}
}};
inline constexpr bool Known(Duration duration){
    for(const auto& value:definitions)if(value.id==duration)return true;
    return false;
}
inline constexpr bool Known(int duration){return Known(static_cast<Duration>(duration));}
inline constexpr bool Timed(Duration duration){return duration!=Duration::Permanent && Known(duration);}
inline constexpr bool Timed(int duration){return Timed(static_cast<Duration>(duration));}
inline std::optional<Duration> ParseDuration(std::string_view name){
    for(const auto& value:definitions)if(name==value.name)return value.id;
    if(name=="UntilEndOfTurn")return Duration::EndOfTurn;
    if(name=="UntilNextFighter")return Duration::NextFighter;
    if(name=="UntilStartOfTricks")return Duration::StartOfTricks;
    int result=-1;
    auto parse=std::from_chars(name.data(),name.data()+name.size(),result);
    if(parse.ec==std::errc{} && parse.ptr==name.data()+name.size() && Known(result))return static_cast<Duration>(result);
    return {};
}
// Native IL2CPP fields, saved modifier counters and legacy integrations use
// int; preserve those signatures as explicit conversion boundaries.
inline int Parse(std::string_view name){auto parsed=ParseDuration(name);return parsed?ToNative(*parsed):-1;}
inline constexpr int Permanent=ToNative(Duration::Permanent);
inline constexpr int EndOfTurn=ToNative(Duration::EndOfTurn);
inline constexpr int NextFighter=ToNative(Duration::NextFighter);
inline constexpr int StartOfTricks=ToNative(Duration::StartOfTricks);
inline bool RegisterEnums(){
    const auto type=BNM::Class("PvZCards.Engine.Effects","EffectDuration");
    if(!type.IsValid() || !type.GetMonoType())return false;
    // Keep stock enum-name parsing untouched. Only unsupported new values
    // need interception by DefaultDeserializer.
    for(const auto& definition:definitions)if(ToNative(definition.id)>ToNative(Duration::NextFighter))
        if(!EnumSerializationSupport::Register(type.GetMonoType(),definition.name.data(),ToNative(definition.id)))return false;
    return true;
}
// All grant, triggered-grant, keyword, stat, and cost expiration consumers
// subscribe to this single channel. Duration=0 is never an expiry boundary.
inline void Emit(Obj* model,Duration duration){
    if(!model || !Timed(duration))return;
    AbilityDurationEvents::expire.Notify(model,ToNative(duration));
}
inline void Emit(Obj* model,int duration){
    Emit(model,static_cast<Duration>(duration));
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
    if(model)Emit(model,Duration::StartOfTricks);
    else LOG_ERR("CustomComponents/Duration: StartOfTricks router has no EntityModel");
}
inline void AfterEffect(const EffectGuards::Context& context){
    if(IsTricksEffect(context) && tricksDepth)--tricksDepth;
}
inline bool Initialize(){
    return EffectGuards::before.Add(BeforeEffect) && EffectGuards::after.Add(AfterEffect);
}
} // namespace CustomComponents::Engine::EffectDurations
