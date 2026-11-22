#pragma once
#include <cstdint>
namespace CustomComponents::Abilities::LaneAttackPolicy {
constexpr std::uint32_t Bit(int lane){return lane>=0 && lane<31?std::uint32_t{1}<<lane:0;}
constexpr std::uint32_t Plan(bool only,std::uint32_t onlyLanes,std::uint32_t additional,int center,bool adjacent,bool adjacentOnly,bool all){
    if(only)return onlyLanes&0x7fffffffu;
    if(all)return 0x7fffffffu;
    auto lanes=additional;
    if(!adjacentOnly)lanes|=Bit(center);
    if(adjacent || adjacentOnly)lanes|=Bit(center-1)|Bit(center+1);
    return lanes&0x7fffffffu;
}
}