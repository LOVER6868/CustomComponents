#pragma once
#include <cmath>
namespace CustomComponents::Presentation::CombatFacingPolicy {
// Positive world-Z is counterclockwise. Screen side is independent of faction.
inline bool Clockwise(bool opponentSide,bool left,bool plant){
    const bool zombieClockwise=opponentSide!=left;
    return plant?!zombieClockwise:zombieClockwise;
}
inline float Target(float original,float desired,bool opponentSide,bool left,bool plant){
    const float magnitude=std::abs(std::remainder(desired-original,360.f));
    return original+(Clockwise(opponentSide,left,plant)?-magnitude:magnitude);
}
}
