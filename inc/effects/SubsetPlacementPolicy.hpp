#pragma once
#include <string_view>
namespace CustomComponents::Effects::SubsetPlacement {
enum class Position {Stock,Front,Back};
inline Position Parse(std::string_view text) {
    if(text=="Front" || text=="front")return Position::Front;
    if(text=="Back" || text=="back")return Position::Back;
    return Position::Stock;
}
inline int Order(Position position,int stockOrder) {
    return position==Position::Front?0:position==Position::Back?1:stockOrder;
}
struct Settings {bool teamup=false,aquatic=false;Position position=Position::Stock;};
inline int CreationOrder(Settings settings,bool stockFront,int stockOrder){
    if(settings.position!=Position::Stock)return Order(settings.position,stockOrder);
    return stockFront?0:settings.teamup?1:stockOrder;
}
template<class QueryFinder,class LaneFinder> int FindGuid(bool aquaticGrant,QueryFinder queryFinder,LaneFinder laneFinder) {
    // A new Aquatic grant replaces the finder's innate-Aquatic requirement.
    // Both branches still run exactly one stock query and RNG selection.
    return aquaticGrant?queryFinder():laneFinder();
}
inline bool Enabled(Settings value){return value.teamup || value.aquatic || value.position!=Position::Stock;}
}
