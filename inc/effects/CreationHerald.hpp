#pragma once
#include "diagnostics/DiagnosticLogging.hpp"
#include "engine/MatchAccess.hpp"
#include "engine/EntityAccess.hpp"

namespace CustomComponents::Effects::CreationHerald {
using Obj=Engine::Match::Obj;

// The stock creation notification carries the new card ID and the source ID,
// but leaves out the producing effect entity that owns HeraldEntities.
inline void Apply(Obj* model,Obj* card,Obj* effect){
    if(!effect || !effect->klass)return;
    const std::string_view kind=effect->klass->name;
    if(kind!="CreateCardEffect" && kind!="CreateCardFromSubsetEffect")return;

    static auto definition=BNM::Class("PvZCards.Engine","Effect").GetField("EffectEntityId").cast<int>();
    static const auto herald=BNM::Class("PvZCards.Core.Systems","HeraldSystem");
    static auto check=herald.GetMethod("CheckForHeraldAbilities",{"effect","effectEntityId","targetId"}).cast<void>();
    if(!definition.IsValid() || !check.IsValid())return;
    const int effectId=definition[effect].Get();
    auto* ability=Engine::EntityAccess::Find(model,effectId);
    if(!Engine::EntityAccess::Component(ability,"HeraldEntities"))return;
    auto* instances=Engine::Match::Instances(card);
    auto* system=Engine::Match::Provide(instances,herald.GetMonoType());
    if(!system){
        CC_DIAG("CustomComponents/CreationHerald: stock service missing effect=%d card=%d",effectId,Engine::Match::Id(card));
        return;
    }
    check[system](effect,effectId,Engine::Match::Id(card));
}
}

