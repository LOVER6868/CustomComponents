#pragma once
#include "level/LevelRuntime.hpp"
#include "level/LevelCostTint.hpp"
namespace CustomComponents::Level::FrameState {
struct Binding {Core::ModelRegistry::Token model=0;int card=-1;};
inline Core::WeakObjectSettings<Binding> frames;
inline Obj* Card(Obj* frame){auto binding=frames.Get(frame);Obj* model=Core::ModelRegistry::Get(binding.model);Obj* card=Engine::EntityAccess::Find(model,binding.card);return card && Engine::EntityAccess::Component(card,"HandElement") && DefinitionFor(card)?card:nullptr;}
inline int Initial(Obj* card,const char* component,const char* property,int fallback){Obj* value=Stats::Value(card,component,property);if(!value)return fallback;auto field=BNM::Class("PvZCards.Engine.Components.Data","ModifiableInt").GetField("BaseValue").cast<int>();return field[value].Get();}
inline void Bind(Obj* frame,Obj* model,int id){
    frames.Set(frame,{Core::ModelRegistry::Identify(model,true),id});
    Obj* card=Engine::EntityAccess::Find(model,id);auto api=Presentation::CardStats::Metadata();if(!card || !api.Valid())return;
    // A snapshot can already contain the selected numbers. Normalize original
    // values even when Frame::Apply will skip an unchanged current value.
    if(Engine::EntityAccess::Component(card,"Attack"))api.initialAttack[frame].Set(Initial(card,"Attack","AttackValue",api.initialAttack[frame].Get()));
    if(Engine::EntityAccess::Component(card,"Health"))api.initialHealth[frame].Set(Initial(card,"Health","MaxHealth",api.initialHealth[frame].Get()));
    if(Engine::EntityAccess::Component(card,"SunCost"))api.initialCost[frame].Set(Initial(card,"SunCost","SunCostValue",api.initialCost[frame].Get()));
}
inline void(*originalCost)(Obj*,int,int,const void*)=nullptr;
inline void(*originalAttack)(Obj*,int,int,const void*)=nullptr;
inline void(*originalHealth)(Obj*,int,int,int,const void*)=nullptr;
static void Cost(Obj* frame,int initial,int current,const void* method){
    Obj* card=Card(frame);auto api=Presentation::CardStats::Metadata();
    if(card){auto values=Presentation::CardStats::Read(card);if(values.cost){initial=Initial(card,"SunCost","SunCostValue",initial);current=*values.cost;api.initialCost[frame].Set(initial);api.cost[frame].Set(current);}}
    originalCost(frame,initial,current,method);
    if(card && SelectedEntry(card)>=0)CostTint::Apply(frame,true);
}
static void Attack(Obj* frame,int initial,int current,const void* method){
    if(Obj* card=Card(frame)){auto values=Presentation::CardStats::Read(card);if(values.attack){auto api=Presentation::CardStats::Metadata();initial=Initial(card,"Attack","AttackValue",initial);current=*values.attack;api.initialAttack[frame].Set(initial);api.attack[frame].Set(current);}}
    originalAttack(frame,initial,current,method);
}
static void Health(Obj* frame,int initial,int maximum,int current,const void* method){
    if(Obj* card=Card(frame)){auto values=Presentation::CardStats::Read(card);if(values.maxHealth){auto api=Presentation::CardStats::Metadata();initial=Initial(card,"Health","MaxHealth",initial);maximum=*values.maxHealth;current=values.CurrentHealth().value_or(maximum);api.initialHealth[frame].Set(initial);api.maximumHealth[frame].Set(maximum);api.health[frame].Set(current);}}
    originalHealth(frame,initial,maximum,current,method);
}
inline bool Initialize(){auto type=BNM::Class("PvZCards.Game","CardFrameViewComponent");return Core::Hooks::Install(type.GetMethod("UpdateSunCostUI",2),Cost,originalCost) && Core::Hooks::Install(type.GetMethod("UpdateAttackUI",2),Attack,originalAttack) && Core::Hooks::Install(type.GetMethod("UpdateHealthUI",3),Health,originalHealth);}
}
