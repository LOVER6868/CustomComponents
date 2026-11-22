#pragma once
#include <algorithm>
#include <cstdint>
#include <optional>
#include <string_view>
#include "engine/EntityAccess.hpp"
#include "level/LevelValue.hpp"

namespace CustomComponents::Engine::CardStatValues {
using Obj=EntityAccess::Obj;
enum class Stat : std::int32_t {
    Invalid=-1, Attack=0, Health=1, Cost=2, BaseAttack=3,
    BaseHealth=4, BaseCost=5, CurrentHealth=6, DamageTaken=7, Level=8
};
inline constexpr Stat Parse(std::string_view name) {
    if(name=="Attack")return Stat::Attack;
    if(name=="Health")return Stat::Health;
    if(name=="Cost")return Stat::Cost;
    if(name=="BaseAttack")return Stat::BaseAttack;
    if(name=="BaseHealth")return Stat::BaseHealth;
    if(name=="BaseCost")return Stat::BaseCost;
    if(name=="CurrentHealth")return Stat::CurrentHealth;
    if(name=="DamageTaken")return Stat::DamageTaken;
    if(name=="Level")return Stat::Level;
    return Stat::Invalid;
}
inline bool IsStat(std::string_view name){return Parse(name)!=Stat::Invalid;}
// Health is calculated maximum; CurrentHealth is remaining health.
inline std::optional<int> Read(Obj* card,Stat stat) {
    if(!card)return {};
    if(stat==Stat::Level)return Level::ReadLevelValue(card);
    const bool base=stat==Stat::BaseAttack || stat==Stat::BaseHealth || stat==Stat::BaseCost;
    if(stat==Stat::DamageTaken || stat==Stat::CurrentHealth) {
        Obj* health=EntityAccess::Component(card,"Health");
        if(!health)return {};
        auto damage=BNM::Class("PvZCards.Engine.Components","Health").GetField("CurrentDamage").cast<int>();
        if(!damage.IsValid())return {};
        if(stat==Stat::DamageTaken)return damage[health].Get();
        auto maximum=Read(card,Stat::Health);
        return maximum?std::optional<int>(std::max(0,*maximum-damage[health].Get())):std::nullopt;
    }
    const char* component=nullptr;const char* field=nullptr;
    switch(stat){
    case Stat::Attack:case Stat::BaseAttack:component="Attack";field="AttackValue";break;
    case Stat::Health:case Stat::BaseHealth:component="Health";field="MaxHealth";break;
    case Stat::Cost:case Stat::BaseCost:component="SunCost";field="SunCostValue";break;
    default:return {};
    }
    Obj* statComponent=EntityAccess::Component(card,component);
    if(!statComponent)return {};
    auto valueField=BNM::Class("PvZCards.Engine.Components",component).GetField(field).cast<Obj*>();
    Obj* value=valueField.IsValid()?valueField[statComponent].Get():nullptr;
    if(!value)return {};
    if(base){
        auto f=BNM::Class("PvZCards.Engine.Components.Data","ModifiableInt").GetField("BaseValue").cast<int>();
        return f.IsValid()?std::optional<int>(f[value].Get()):std::nullopt;
    }
    if(stat==Stat::Cost && EntityAccess::Component(card,"FreeSunCost"))return 0;
    auto calc=BNM::Class("PvZCards.Engine.Utilities","ModifiableIntCalculator").GetMethod("CalculateValue",1).cast<int>();
    if(!calc.IsValid())return {};
    const int number=calc(value);
    return stat==Stat::Cost?std::optional<int>(std::max(0,number)):std::optional<int>(number);
}
inline std::optional<int> Read(Obj* card,std::string_view stat){return Read(card,Parse(stat));}
inline std::optional<int> Difference(Obj* card,Stat stat){
    Stat base=Stat::Invalid;
    switch(stat){
    case Stat::Attack:base=Stat::BaseAttack;break;
    case Stat::Health:base=Stat::BaseHealth;break;
    case Stat::Cost:base=Stat::BaseCost;break;
    default:return {};
    }
    auto current=Read(card,stat),original=Read(card,base);
    return current && original?std::optional<int>(*current-*original):std::nullopt;
}
inline std::optional<int> Difference(Obj* card,std::string_view stat){return Difference(card,Parse(stat));}
}
