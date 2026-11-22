#pragma once
#include <algorithm>
#include <optional>
namespace CustomComponents::Presentation::Stats {
struct Values {
    std::optional<int> attack,maxHealth,damage,cost;
    std::optional<int> CurrentHealth() const {
        if(!maxHealth || !damage || *maxHealth<0 || *damage<0) return {};
        return *damage>=*maxHealth?0:*maxHealth-*damage;
    }
};
// Reconciliation assigns a snapshot. It never replays an accumulated buff.
// Missing components leave the corresponding printed UI untouched.
template<class Frame> void Present(const Values& value,Frame& frame) {
    if(value.attack) frame.Attack(*value.attack);
    if(auto current=value.CurrentHealth()) frame.Health(*value.maxHealth,*current);
    if(value.cost) frame.Cost(std::max(0,*value.cost));
}
inline bool CanFlush(bool exactOwner,int normal,int front,bool historyCurrent) {
    return exactOwner && historyCurrent && normal==0 && front==0;
}
}

