#pragma once
#include "engine/BoardPlacementPolicy.hpp"
namespace CustomComponents::Engine::SwapPolicy {
struct Rules {bool ignoreTerrain=false,ignoreOccupancy=false;};
inline bool Fits(BoardPlacementPolicy::Card card,int residents,bool residentTeamup,bool water,Rules rules){
    if(!card.fighter || card.environment || residents<0)return false;
    if(water && !card.aquatic && !rules.ignoreTerrain)return false;
    return rules.ignoreOccupancy || BoardPlacementPolicy::Fits(card,residents,residentTeamup,false);
}
}
