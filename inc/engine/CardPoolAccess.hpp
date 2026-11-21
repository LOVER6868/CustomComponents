#pragma once
#include "engine/MatchAccess.hpp"
namespace CustomComponents::Engine::CardPoolAccess {
using Obj=Match::Obj;
inline BNM::Method<Obj*> regular,superpowers;
inline bool Initialize(){
    const auto player=BNM::Class("PvZCards.Core.Utilities","PlayerUtils");
    regular=player.GetMethod("GetDeckEntityFromPlayer",2).cast<Obj*>();
    superpowers=player.GetMethod("GetSuperpowerPoolEntityFromPlayer",2).cast<Obj*>();
    return regular.IsValid() && superpowers.IsValid();
}
// Pools are associated with a player. Their entities need not have Owner.
inline Obj* Get(Obj* model,int playerId,bool powers){
    Obj* player=EntityAccess::Find(model,playerId);
    if(!player || !EntityAccess::Component(player,"Player"))return nullptr;
    const auto method=powers?superpowers:regular;
    return method.IsValid()?method(player,model):nullptr;
}
}