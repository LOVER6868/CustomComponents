#pragma once
#include "core/Observers.hpp"
#include "core/HookRegistry.hpp"
#include "engine/MatchAccess.hpp"
namespace CustomComponents::Engine::ZoneChanges {
using Obj=BNM::IL2CPP::Il2CppObject;
inline Core::Observers<Obj*,int> afterLaneRemove,afterDiscardAdd,beforeHandAdd,beforeHandRemove;
inline void(*originalLane)(Obj*,int,const void*)=nullptr;
inline void(*originalDiscard)(Obj*,int,int,const void*)=nullptr;
inline void(*originalHand)(Obj*,Obj*,Obj*,const void*)=nullptr;
inline void(*originalHandRemove)(Obj*,Obj*,Obj*,const void*)=nullptr;
inline Obj* Model(Obj* self){auto field=BNM::Class(self).GetField("<Entities>k__BackingField").cast<Obj*>();return field.IsValid()?field[self].Get():nullptr;}
static void Lane(Obj* self,int card,const void* method){Obj* model=Model(self);originalLane(self,card,method);afterLaneRemove.Notify(model,card);}
static void Discard(Obj* self,int card,int pile,const void* method){Obj* model=Model(self);originalDiscard(self,card,pile,method);afterDiscardAdd.Notify(model,card);}
static void Hand(Obj* self,Obj* hand,Obj* card,const void* method){beforeHandAdd.Notify(Model(self),Match::Id(card));originalHand(self,hand,card,method);}
static void HandRemove(Obj* self,Obj* hand,Obj* card,const void* method){beforeHandRemove.Notify(Model(self),Match::Id(card));originalHandRemove(self,hand,card,method);}
inline bool Initialize(){return Core::Hooks::Install(BNM::Class("PvZCards.Core.Systems.Utilities","HandModifier").GetMethod("RemoveCard",2),HandRemove,originalHandRemove) && Core::Hooks::Install(BNM::Class("PvZCards.Core.Systems.Utilities","LaneModifier").GetMethod("RemoveCardFromLane",1),Lane,originalLane) && Core::Hooks::Install(BNM::Class("PvZCards.Core.Systems.Utilities","DiscardPileModifier").GetMethod("AddCard",2),Discard,originalDiscard) && Core::Hooks::Install(BNM::Class("PvZCards.Core.Systems.Utilities","HandModifier").GetMethod("AddCard",2),Hand,originalHand);}
}
