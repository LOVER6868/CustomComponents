#pragma once
#include <algorithm>
#include <optional>
#include <string_view>
#include "engine/EntityAccess.hpp"
#include "level/LevelValue.hpp"

namespace CustomComponents::Engine::CardStatValues {
using Obj=EntityAccess::Obj;
// CurrentHealth means remaining health; Health means calculated MAX health.
// DamageTaken is separate and is NOT a health nerf.
inline std::optional<int> Read(Obj* card,std::string_view stat) {
    if (!card) return {};
    if (stat=="Level") return Level::ReadLevelValue(card);
    const bool base=stat=="BaseAttack" || stat=="BaseHealth" || stat=="BaseCost";
    const bool remaining=stat=="CurrentHealth";
    if (stat=="DamageTaken" || remaining) {
        Obj* health=EntityAccess::Component(card,"Health");
        if(!health)return {};
        auto damage=BNM::Class("PvZCards.Engine.Components","Health").GetField("CurrentDamage").cast<int>();
        if(!damage.IsValid())return {};
        if(stat=="DamageTaken")return damage[health].Get();
        auto maximum=Read(card,"Health");
        return maximum?std::optional<int>(std::max(0,*maximum-damage[health].Get())):std::nullopt;
    }
    const char* component=nullptr;const char* field=nullptr;
    if(stat=="Attack" || stat=="BaseAttack"){component="Attack";field="AttackValue";}
    else if(stat=="Health" || stat=="BaseHealth"){component="Health";field="MaxHealth";}
    else if(stat=="Cost" || stat=="BaseCost"){component="SunCost";field="SunCostValue";}
    else return {};
    Obj* statComponent=EntityAccess::Component(card,component);
    if(!statComponent)return {};
    auto valueField=BNM::Class("PvZCards.Engine.Components",component).GetField(field).cast<Obj*>();
    Obj* value=valueField.IsValid()?valueField[statComponent].Get():nullptr;
    if(!value)return {};
    if(base){
        auto f=BNM::Class("PvZCards.Engine.Components.Data","ModifiableInt").GetField("BaseValue").cast<int>();
        return f.IsValid()?std::optional<int>(f[value].Get()):std::nullopt;
    }
    if(stat=="Cost" && EntityAccess::Component(card,"IsFree"))return 0;
    auto calc=BNM::Class("PvZCards.Engine.Utilities","ModifiableIntCalculator").GetMethod("CalculateValue",1).cast<int>();
    if(!calc.IsValid())return {};
    const int number=calc(value);
    return stat=="Cost"?std::optional<int>(std::max(0,number)):std::optional<int>(number);
}
inline std::optional<int> Difference(Obj* card,std::string_view stat){
    const char* current=nullptr;const char* base=nullptr;
    if(stat=="Attack"){current="Attack";base="BaseAttack";}
    else if(stat=="Health"){current="Health";base="BaseHealth";}
    else if(stat=="Cost"){current="Cost";base="BaseCost";}
    else return {};
    auto a=Read(card,current),b=Read(card,base);
    return a && b?std::optional<int>(*a-*b):std::nullopt;
}
}
