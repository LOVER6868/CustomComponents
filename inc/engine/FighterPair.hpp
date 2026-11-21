#pragma once
#include "engine/EntityAccess.hpp"
#include "engine/MatchAccess.hpp"
#include "engine/CardTypes.hpp"

namespace CustomComponents::Engine::FighterPair {
inline bool Live(EntityAccess::Obj* model,int id){
    auto* card=EntityAccess::Find(model,id);
    if(!card || !EntityAccess::Component(card,"LaneElement") ||
       !CardTypes::Fighter(card) ||
       EntityAccess::Component(card,"MarkedForDeath"))return false;
    auto* health=EntityAccess::Component(card,"Health");
    static auto calculate=BNM::Class("PvZCards.Engine.Components","Health").GetMethod("CalculateHealth",0).cast<int>();
    if(!health || !calculate.IsValid())return false;
    auto method=calculate;
    return method[health]()>0;
}
inline bool Legal(EntityAccess::Obj* model,int first,int second){
    return first>=0 && second>=0 && first!=second && Live(model,first) && Live(model,second);
}
}
