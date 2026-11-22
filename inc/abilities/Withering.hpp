#pragma once
#include "abilities/CustomAbilityIds.hpp"
#include <cstdint>
#include <BNM/ClassesManagement.hpp>
#include "abilities/GrantableAbilityRuntime.hpp"
#include "abilities/GrantModesPolicy.hpp"
#include "core/CustomComponentRegistry.hpp"
#include "engine/MatchAccess.hpp"
namespace CustomComponents::Abilities::Withering {
using Obj=Engine::Match::Obj;
inline constexpr int Id=CustomAbilityIds::ToNative(CustomAbilityIds::Id::Withering);
struct Component:Obj {
    // Reserve the inherited GrantableAbility::Counters reference (offset 0x10).
    std::uint8_t inherited[8]{};
    BNM_CustomClass(Component,BNM::CompileTimeClassBuilder("PvZCards.Engine.Components","Withering","EngineLib.dll").Build(),BNM::CompileTimeClassBuilder("PvZCards.Engine.Components","GrantableAbility","EngineLib.dll").Build(),{});
};
static_assert(sizeof(Component) >= sizeof(Obj) + sizeof(void*),
    "Withering must reserve the inherited GrantableAbility::Counters slot");
inline void* Get(void* entity,bool create) {
    static auto type=BNM::Class("PvZCards.Engine.Components","Withering");
    return Engine::Match::Component(static_cast<Obj*>(entity),type.GetMonoType(),create);
}
inline int Mode(Obj* component) {
    if(!component)return 0;auto counters=GrantableAbilityRuntime::countersField;auto entries=GrantableAbilityRuntime::entriesField;
    Obj* group=static_cast<Obj*>(counters[component].Get());auto* list=group?static_cast<BNM::Structures::Mono::List<GrantableAbilityRuntime::CounterEntry>*>(entries[group].Get()):nullptr;
    if(!list || !list->items || list->size<0 || list->size>int(list->items->capacity))return 0;
    // Most recently granted surviving source wins; values are never summed.
    return GrantModes::LastWitheringMode(list->items->m_Items,list->size);
}
inline bool Apply(Obj* component,int duration,int source,int mode) {
    if(!component || !GrantModes::ValidWithering(mode))return false;
    auto remove=GrantableAbilityRuntime::removeCounter;auto add=GrantableAbilityRuntime::addCounter;
    remove[component](duration,source);add[component](duration,source,mode);return true;
}
inline bool Initialize() {
    return CustomComponentRegistry::Register({"Withering","PvZCards.Engine.Components","EngineLib.dll","GrantableAbility","PvZCards.Engine.Components","EngineLib.dll"});
}
}

