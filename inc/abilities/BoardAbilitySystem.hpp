#include "abilities/BoardAbilityPolicy.hpp"
#pragma once
#include "engine/MatchAccess.hpp"
#include "core/HookRegistry.hpp"
namespace CustomComponents::Abilities::BoardAbility {
using Obj=Engine::Match::Obj;
inline constexpr const char* spent="sprig:spent";
inline bool IsSpent(Obj* card) {std::string value;return Engine::Match::Tag(card,spent,value) && value.empty();}
inline void(*originalDestroy)(Obj*,Obj*,const void*)=nullptr;
inline bool(*originalValidate)(Obj*,Obj*,Obj*,const void*)=nullptr;
static void Destroy(Obj* self,Obj* effect,const void* method) {
    auto modelField=BNM::Class("PvZCards.Core.Systems","DestroyCardSystem").GetField("<Entities>k__BackingField").cast<Obj*>();
    auto targetField=BNM::Class("PvZCards.Engine","Effect").GetField("TargetId").cast<int>();
    auto cancel=BNM::Class("PvZCards.Engine","Effect").GetMethod("IsCancelled",0).cast<bool>();
    Obj* model=self && modelField.IsValid()?modelField[self].Get():nullptr;
    Obj* target=effect && targetField.IsValid()?Engine::EntityAccess::Find(model,targetField[effect].Get()):nullptr;
    if(!Engine::EntityAccess::Component(target,"BoardAbility") || !cancel.IsValid() || cancel[effect]()) {originalDestroy(self,effect,method);return;}
    if(!IsSpent(target) && !Engine::Match::SetTag(target,spent,""))
        LOG_ERR("CustomComponents/BoardAbility: could not mark board ability %d spent",Engine::Match::Id(target));
}
static bool Validate(Obj* self,Obj* effectEntity,Obj* context,const void* method) {
    if(!originalValidate(self,effectEntity,context,method)) return false;
    auto modelField=BNM::Class("PvZCards.Engine","EntityQueryContext").GetField("<Model>k__BackingField").cast<Obj*>();
    auto source=BNM::Class("PvZCards.Engine.Components","EffectEntity").GetField("SourceId").cast<int>();
    Obj* link=Engine::EntityAccess::Component(effectEntity,"EffectEntity");
    Obj* model=context && modelField.IsValid()?modelField[context].Get():nullptr;
    Obj* card=link && source.IsValid()?Engine::EntityAccess::Find(model,source[link].Get()):nullptr;
    return BoardPolicy::MayTrigger(true,Engine::EntityAccess::Component(card,"BoardAbility")!=nullptr,IsSpent(card));
}
inline bool Initialize() {
    auto validate=BNM::Class("PvZCards.Core.Systems.Utilities","TriggerValidator").GetMethod("ShouldTriggerEffect",2);
    auto destroy=BNM::Class("PvZCards.Core.Systems","DestroyCardSystem").GetMethod("ProcessEffect",1);
    if(!Core::Hooks::Install(validate,Validate,originalValidate)) return false;
    return Core::Hooks::Install(destroy,Destroy,originalDestroy);
}
}

