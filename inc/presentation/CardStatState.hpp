#pragma once
#include <BNM/Class.hpp>
#include <BNM/Field.hpp>
#include <BNM/Method.hpp>
#include <BNM/Exceptions.hpp>
#include "engine/EntityAccess.hpp"
#include "presentation/StatReconciliation.hpp"
namespace CustomComponents::Presentation::CardStats {
using Obj=BNM::IL2CPP::Il2CppObject;
namespace Policy=Stats;
inline Policy::Values Read(Obj* entity) {
    Policy::Values result;
    if(!entity) return result;
    static auto attack=BNM::Class("PvZCards.Engine.Components","Attack").GetMethod("CalculateAttack",0).cast<int>();
    static auto health=BNM::Class("PvZCards.Engine.Components","Health").GetField("MaxHealth").cast<Obj*>();
    static auto damage=BNM::Class("PvZCards.Engine.Components","Health").GetField("CurrentDamage").cast<int>();
    static auto cost=BNM::Class("PvZCards.Engine.Components","SunCost").GetField("SunCostValue").cast<Obj*>();
    static auto calculate=BNM::Class("PvZCards.Engine.Utilities","ModifiableIntCalculator").GetMethod("CalculateValue",1).cast<int>();
    auto attackMethod=attack;auto healthField=health;auto damageField=damage;auto costField=cost;
    if(auto* component=Engine::EntityAccess::Component(entity,"Attack");component && attackMethod.IsValid()) result.attack=attackMethod[component]();
    if(auto* component=Engine::EntityAccess::Component(entity,"Health");component && healthField.IsValid() && damageField.IsValid() && calculate.IsValid()) {
        if(auto* value=healthField[component].Get()) {result.maxHealth=calculate(value);result.damage=damageField[component].Get();}
    }
    if(auto* component=Engine::EntityAccess::Component(entity,"SunCost");component && costField.IsValid() && calculate.IsValid())
        if(auto* value=costField[component].Get()) result.cost=calculate(value);
    return result;
}
struct FrameMetadata {
    BNM::Field<int> attack,initialAttack,maximumHealth,health,initialHealth,cost,initialCost;
    BNM::Method<void> updateAttack,updateHealth,updateCost;
    bool Valid() const {
        return attack.IsValid() && initialAttack.IsValid() && maximumHealth.IsValid() && health.IsValid() &&
            initialHealth.IsValid() && cost.IsValid() && initialCost.IsValid() && updateAttack.IsValid() &&
            updateHealth.IsValid() && updateCost.IsValid();
    }
};
inline const FrameMetadata& Metadata() {
    static const auto metadata=[] {
        auto type=BNM::Class("PvZCards.Game","CardFrameViewComponent");
        return FrameMetadata{type.GetField("CurrentAttack").cast<int>(),type.GetField("InitialAttack").cast<int>(),
            type.GetField("MaxHealth").cast<int>(),type.GetField("CurrentHealth").cast<int>(),type.GetField("InitialHealth").cast<int>(),
            type.GetField("CurrentSunCost").cast<int>(),type.GetField("InitialSunCost").cast<int>(),
            type.GetMethod("UpdateAttackUI",2).cast<void>(),type.GetMethod("UpdateHealthUI",3).cast<void>(),type.GetMethod("UpdateSunCostUI",2).cast<void>()};
    }();
    return metadata;
}
struct Frame {
    Obj* object;FrameMetadata api=Metadata();
    void Attack(int value) {
        if(api.attack[object].Get()==value) return;
        api.attack[object].Set(value);api.updateAttack[object](api.initialAttack[object].Get(),value);
    }
    void Health(int maximum,int current) {
        if(api.maximumHealth[object].Get()==maximum && api.health[object].Get()==current) return;
        api.maximumHealth[object].Set(maximum);api.health[object].Set(current);
        api.updateHealth[object](api.initialHealth[object].Get(),maximum,current);
    }
    void Cost(int value) {
        if(api.cost[object].Get()==value) return;
        api.cost[object].Set(value);api.updateCost[object](api.initialCost[object].Get(),value);
    }
};
inline void Apply(Obj* frame,const Policy::Values& values) {
    static auto type=BNM::Class("PvZCards.Game","CardFrameViewComponent");
    if(!frame || !Metadata().Valid() || !BNM::IsA(frame,type)) return;
    Frame target{frame};Policy::Present(values,target);
}
}



