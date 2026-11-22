#pragma once
namespace CustomComponents::Engine::BoardPlacementPolicy {
struct Card {bool fighter=false,environment=false,plant=false,teamup=false,aquatic=false;};
inline bool Fits(Card card,int residents,bool residentTeamup,bool water) {
    if(card.fighter==card.environment)return false;
    if(card.environment)return true;
    if(water && !card.aquatic)return false;
    if(residents<0 || residents>1)return false;
    return residents==0 || (card.plant && (card.teamup || residentTeamup));
}
}
