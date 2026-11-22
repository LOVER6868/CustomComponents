#pragma once
#include "level/LevelComponent.hpp"
namespace CustomComponents::Level {
struct Application:Obj {
    int Value=0,EntryIndex=-1,CostBonus=0,AttackBonus=0,HealthBonus=0;bool WasOnBoard=false,InHandSelection=false,WasFree=false;
    BNM_CustomClass(Application,BNM::CompileTimeClassBuilder("PvZCards.Engine.Components","LevelApplication","EngineLib.dll").Build(),BNM::CompileTimeClassBuilder("PvZCards.Engine.Components","MarkerComponent","EngineLib.dll").Build(),{});
    BNM_CustomField(Value,BNM::Defaults::Get<int>(),"Value");
    BNM_CustomField(EntryIndex,BNM::Defaults::Get<int>(),"EntryIndex");
    BNM_CustomField(CostBonus,BNM::Defaults::Get<int>(),"CostBonus");
    BNM_CustomField(AttackBonus,BNM::Defaults::Get<int>(),"AttackBonus");
    BNM_CustomField(HealthBonus,BNM::Defaults::Get<int>(),"HealthBonus");
    BNM_CustomField(WasOnBoard,BNM::Defaults::Get<bool>(),"WasOnBoard");
    BNM_CustomField(InHandSelection,BNM::Defaults::Get<bool>(),"InHandSelection");
    BNM_CustomField(WasFree,BNM::Defaults::Get<bool>(),"WasFree");
    void DeepCopyFrom(Obj* source,Obj*){auto* s=reinterpret_cast<Application*>(source);if(s){Value=s->Value;EntryIndex=s->EntryIndex;CostBonus=s->CostBonus;AttackBonus=s->AttackBonus;HealthBonus=s->HealthBonus;WasOnBoard=s->WasOnBoard;InHandSelection=s->InHandSelection;WasFree=s->WasFree;}else{Value=CostBonus=AttackBonus=HealthBonus=0;EntryIndex=-1;WasOnBoard=InHandSelection=WasFree=false;}}
    void HandleReturnToPool(Obj* instances){DeepCopyFrom(nullptr,instances);}
    BNM_CustomMethod(DeepCopyFrom,false,BNM::Defaults::Get<void>(),"DeepCopyFrom",BNM::Defaults::Get<Obj*>(),BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
    BNM_CustomMethod(HandleReturnToPool,false,BNM::Defaults::Get<void>(),"HandleReturnToPool",BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
};
struct Leveled:Obj {
    int Value=0,EntryIndex=-1,CostBonus=0,AttackBonus=0,HealthBonus=0;bool WasOnBoard=false,InHandSelection=false,WasFree=false;
    BNM_CustomClass(Leveled,BNM::CompileTimeClassBuilder("PvZCards.Engine.Components","Leveled","EngineLib.dll").Build(),BNM::CompileTimeClassBuilder("PvZCards.Engine.Components","MarkerComponent","EngineLib.dll").Build(),{});
    BNM_CustomField(Value,BNM::Defaults::Get<int>(),"Value");
    BNM_CustomField(EntryIndex,BNM::Defaults::Get<int>(),"EntryIndex");
    BNM_CustomField(CostBonus,BNM::Defaults::Get<int>(),"CostBonus");
    BNM_CustomField(AttackBonus,BNM::Defaults::Get<int>(),"AttackBonus");
    BNM_CustomField(HealthBonus,BNM::Defaults::Get<int>(),"HealthBonus");
    BNM_CustomField(WasOnBoard,BNM::Defaults::Get<bool>(),"WasOnBoard");
    BNM_CustomField(InHandSelection,BNM::Defaults::Get<bool>(),"InHandSelection");
    BNM_CustomField(WasFree,BNM::Defaults::Get<bool>(),"WasFree");
    void DeepCopyFrom(Obj* source,Obj*){auto* s=reinterpret_cast<Leveled*>(source);if(s){Value=s->Value;EntryIndex=s->EntryIndex;CostBonus=s->CostBonus;AttackBonus=s->AttackBonus;HealthBonus=s->HealthBonus;WasOnBoard=s->WasOnBoard;InHandSelection=s->InHandSelection;WasFree=s->WasFree;}else{Value=CostBonus=AttackBonus=HealthBonus=0;EntryIndex=-1;WasOnBoard=InHandSelection=WasFree=false;}}
    void HandleReturnToPool(Obj* instances){DeepCopyFrom(nullptr,instances);}
    BNM_CustomMethod(DeepCopyFrom,false,BNM::Defaults::Get<void>(),"DeepCopyFrom",BNM::Defaults::Get<Obj*>(),BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
    BNM_CustomMethod(HandleReturnToPool,false,BNM::Defaults::Get<void>(),"HandleReturnToPool",BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
};
inline bool InitializeLeveled(){return Core::ComponentLifecycle::Patch(BNM::Class("PvZCards.Engine.Components","LevelApplication"),BNM::Class("PvZCards.Engine.Components","MarkerComponent")) && CustomComponentRegistry::Register({"LevelApplication","PvZCards.Engine.Components","EngineLib.dll","MarkerComponent","PvZCards.Engine.Components","EngineLib.dll"}) && Core::ComponentLifecycle::Patch(BNM::Class("PvZCards.Engine.Components","Leveled"),BNM::Class("PvZCards.Engine.Components","MarkerComponent")) && CustomComponentRegistry::Register({"Leveled","PvZCards.Engine.Components","EngineLib.dll","MarkerComponent","PvZCards.Engine.Components","EngineLib.dll"});}
}
