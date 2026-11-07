#pragma once
#include "level/Leveled.hpp"
#include <limits>
#include "engine/MatchAccess.hpp"
namespace CustomComponents::Level::Stats {
inline constexpr const char* Group="CustomComponents/Level";
inline BNM::Structures::Mono::String* GroupName(){static auto root=std::make_shared<Core::ManagedRuntime::Root>(reinterpret_cast<Obj*>(BNM::CreateMonoString(Group)));return reinterpret_cast<BNM::Structures::Mono::String*>(root->Get());}
inline Obj* Value(Obj* card,const char* component,const char* field) {Obj* stat=Engine::EntityAccess::Component(card,component);auto property=BNM::Class("PvZCards.Engine.Components",component).GetField(field).cast<Obj*>();return stat && property.IsValid()?property[stat].Get():nullptr;}
// The engine calculator sums active additive modifiers. Exclude only our own
// group when quoting a new mode, retaining all other cost/stat changes.
inline int WithoutLevel(Obj* value){
    if(!value)return 0;
    auto type=BNM::Class("PvZCards.Engine.Components.Data","ModifiableInt");
    std::int64_t total=type.GetField("BaseValue").cast<int>()[value].Get();
    auto* list=type.GetField("Modifiers").cast<BNM::Structures::Mono::List<Obj*>*>()[value].Get();
    auto modifier=BNM::Class("PvZCards.Engine.Components.Data","IntModifier");
    auto group=modifier.GetField("GroupId").cast<BNM::Structures::Mono::String*>();auto amount=modifier.GetField("AdditiveAmount").cast<int>();auto active=modifier.GetField("ActivationTime").cast<int>();
    if(Engine::EntityAccess::Valid(list))for(int i=0;i<list->size;++i){Obj* item=list->items->m_Items[i];if(!item || active[item].Get()!=0)continue;auto* name=group[item].Get();if(!name || name->str()!=Group)total+=amount[item].Get();}
    return int(std::clamp<std::int64_t>(total,std::numeric_limits<int>::min(),std::numeric_limits<int>::max()));
}
inline void ClearValue(Obj* value,Obj* instances) {
    if(!value || !instances)return;
    auto type=BNM::Class("PvZCards.Engine.Components.Data","ModifiableInt");auto field=type.GetField("Modifiers").cast<BNM::Structures::Mono::List<Obj*>*>();auto remove=type.GetMethod("RemoveModifier",2).cast<void>();
    auto group=BNM::Class("PvZCards.Engine.Components.Data","IntModifier").GetField("GroupId").cast<BNM::Structures::Mono::String*>();auto* list=field[value].Get();if(!Engine::EntityAccess::Valid(list))return;
    for(int i=list->size-1;i>=0;--i){Obj* modifier=list->items->m_Items[i];auto* name=modifier?group[modifier].Get():nullptr;if(name && name->str()==Group)remove[value](modifier,instances);}
}
inline bool Add(Obj* value,Obj* instances,int amount) {
    if(!value)return amount==0;if(!amount)return true;
    auto type=BNM::Class("PvZCards.Engine.Components.Data","IntModifier");Obj* modifier=Core::ManagedPool::Acquire(instances,type);Core::ManagedRuntime::Root root(modifier);if(!root.Get())return false;
    type.GetField("GroupId").cast<BNM::Structures::Mono::String*>()[modifier].Set(GroupName());
    type.GetField("OriginalAmount").cast<int>()[modifier].Set(amount);type.GetField("AdditiveAmount").cast<int>()[modifier].Set(amount);type.GetField("Duration").cast<int>()[modifier].Set(0);type.GetField("ActivationTime").cast<int>()[modifier].Set(0);
    auto add=BNM::Class("PvZCards.Engine.Components.Data","ModifiableInt").GetMethod("AddModifier",1).cast<void>();add[value](modifier);return true;
}
inline void Clear(Obj* card) {
    auto* state=reinterpret_cast<Application*>(Engine::EntityAccess::Component(card,"LevelApplication"));
    if(state && state->InHandSelection && state->WasFree)Engine::Match::Component(card,BNM::Class("PvZCards.Engine.Components","IsFree").GetMonoType(),true);
    Obj* instances=Engine::Match::Instances(card);for(auto pair:{std::pair{"Attack","AttackValue"},std::pair{"Health","MaxHealth"},std::pair{"SunCost","SunCostValue"}})ClearValue(Value(card,pair.first,pair.second),instances);
    auto remove=BNM::Class("PvZCards.Engine","Entity").GetMethod("RemoveComponent",1).cast<Obj*>();auto type=BNM::Class("PvZCards.Engine.Components","Leveled");if(Engine::Match::Component(card,type.GetMonoType()))remove[card](type.GetMonoType());
    auto app=BNM::Class("PvZCards.Engine.Components","LevelApplication");if(Engine::Match::Component(card,app.GetMonoType()))remove[card](app.GetMonoType());
}
inline bool Apply(Obj* card,const Quote& quote,int entry) {
    auto* previous=reinterpret_cast<Application*>(Engine::EntityAccess::Component(card,"LevelApplication"));
    const bool wasFree=Engine::EntityAccess::Component(card,"IsFree") || (previous && previous->InHandSelection && previous->WasFree);
    const int costBonus=quote.total-WithoutLevel(Value(card,"SunCost","SunCostValue"));
    Clear(card);
    Obj* instances=Engine::Match::Instances(card);
    if(!Add(Value(card,"Attack","AttackValue"),instances,Engine::EntityAccess::Component(card,"Attack")?quote.attack:0) || !Add(Value(card,"Health","MaxHealth"),instances,Engine::EntityAccess::Component(card,"Health")?quote.health:0) || !Add(Value(card,"SunCost","SunCostValue"),instances,costBonus)){Clear(card);return false;}
    Obj* state=Engine::Match::Component(card,BNM::Class("PvZCards.Engine.Components","LevelApplication").GetMonoType(),true);if(!state){Clear(card);return false;}
    auto* leveled=reinterpret_cast<Application*>(state);leveled->Value=quote.extra;leveled->EntryIndex=entry;leveled->CostBonus=costBonus;leveled->WasFree=wasFree;leveled->AttackBonus=quote.attack;leveled->HealthBonus=quote.health;leveled->WasOnBoard=false;
    if(quote.extra>0){Obj* publicState=Engine::Match::Component(card,BNM::Class("PvZCards.Engine.Components","Leveled").GetMonoType(),true);if(!publicState){Clear(card);return false;}auto* marker=reinterpret_cast<Leveled*>(publicState);marker->Value=quote.extra;marker->EntryIndex=entry;}
    return true;
}
}
