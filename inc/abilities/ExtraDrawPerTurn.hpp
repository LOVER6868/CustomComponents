#pragma once
#include "abilities/CustomAbilityIds.hpp"
#include <algorithm>
#include <cstdint>
#include <limits>
#include <BNM/ClassesManagement.hpp>
#include "abilities/GrantableAbilityRuntime.hpp"
#include "core/AbilityRegistry.hpp"
#include "core/CustomComponentRegistry.hpp"
#include "core/HookRegistry.hpp"
#include "engine/BoardPlacement.hpp"

namespace CustomComponents::Abilities::ExtraDrawPerTurn {
using Obj=Engine::Match::Obj;
inline constexpr int Id=CustomAbilityIds::ToNative(CustomAbilityIds::Id::ExtraDrawPerTurn);
struct Component:Obj {
    // GrantableAbility stores a managed Counters reference immediately after Il2CppObject.
    // Reserve that inherited slot before registering the BNM-derived class.
    std::uint8_t inherited[8]{};
    BNM_CustomClass(Component,
        BNM::CompileTimeClassBuilder("PvZCards.Engine.Components","ExtraDrawPerTurn","EngineLib.dll").Build(),
        BNM::CompileTimeClassBuilder("PvZCards.Engine.Components","GrantableAbility","EngineLib.dll").Build(),{});
};
static_assert(sizeof(Component) >= sizeof(Obj) + sizeof(void*),
    "ExtraDrawPerTurn must reserve the inherited GrantableAbility::Counters slot");
inline void* Get(void* entity,bool create){
    auto* player=static_cast<Obj*>(entity);
    if(!Engine::EntityAccess::Component(player,"Player"))return nullptr;
    static const auto type=BNM::Class("PvZCards.Engine.Components","ExtraDrawPerTurn");
    return Engine::Match::Component(player,type.GetMonoType(),create);
}
inline int(*originalCount)(Obj*,Obj*,const void*)=nullptr;
inline BNM::Field<Obj*> drawInstances;
inline BNM::MonoType* entityModelType=nullptr;
inline int Count(Obj* system,Obj* hand,const void* method){
    const int stock=originalCount(system,hand,method);
    const int owner=Engine::BoardPlacement::Owner(hand);
    if(owner<0)return stock;
    auto* model=Engine::Match::Provide(drawInstances[system].Get(),entityModelType);
    auto* player=Engine::EntityAccess::Find(model,owner);
    const int extra=GrantableAbilityRuntime::SumValues(Get(player,false));
    return static_cast<int>(std::clamp<std::int64_t>(std::int64_t(stock)+std::max(0,extra),0,
        std::numeric_limits<int>::max()));
}
inline bool Initialize(){
    const auto system=BNM::Class("PvZCards.Core.Systems","DrawCardsEachTurnSystem");
    drawInstances=system.GetField("<Instances>k__BackingField").cast<Obj*>();
    entityModelType=BNM::Class("PvZCards.Engine","EntityModel").GetMonoType();
    return drawInstances.IsValid() && entityModelType &&
        CustomComponentRegistry::Register({"ExtraDrawPerTurn","PvZCards.Engine.Components","EngineLib.dll",
            "GrantableAbility","PvZCards.Engine.Components","EngineLib.dll"}) &&
        AbilityRegistry::Register({Id,"ExtraDrawPerTurn",AbilityStorage::Grantable,Get,nullptr}) &&
        Core::Hooks::Install(system.GetMethod("GetNumCardsToDraw",1),Count,originalCount);
}
}
