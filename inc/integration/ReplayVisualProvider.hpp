#pragma once
#include <matchrecorder/replay_visual_api.h>
#include "presentation/CardStatState.hpp"
#include "presentation/DiscardPileViewer.hpp"
namespace CustomComponents::Integration::ReplayVisualProvider {
inline Presentation::Stats::Values Values(const MatchRecorder::ReplayVisualCard& c){
    Presentation::Stats::Values v;if(c.attack>=0)v.attack=c.attack;if(c.cost>=0)v.cost=c.cost;
    if(c.maxHealth>=0)v.maxHealth=c.maxHealth;if(c.health>=0&&c.maxHealth>=0)v.damage=std::max(0,c.maxHealth-c.health);return v;
}
inline bool Apply(void* frame,const MatchRecorder::ReplayVisualCard& c){if(!frame)return false;Presentation::CardStats::Apply(static_cast<BNM::IL2CPP::Il2CppObject*>(frame),Values(c));return true;}
inline bool Inspect(void* signal,const MatchRecorder::ReplayVisualCard* cards,std::size_t count,int initial){
    if(!cards||!count||count>2048)return false;
    std::vector<int> guids;std::vector<Presentation::InspectionStats::Snapshot> values;guids.reserve(count);values.reserve(count);
    for(std::size_t i=0;i<count;++i){guids.push_back(cards[i].guid);Presentation::InspectionStats::Snapshot s;s.guid=cards[i].guid;s.stats=Values(cards[i]);s.discarded=cards[i].discarded;s.reconcileStats=true;values.push_back(s);}
    return Utilities::DiscardPileViewer::ShowCardGuidList(static_cast<BNM::IL2CPP::Il2CppObject*>(signal),nullptr,guids.data(),int(count),initial,values.data());
}
inline bool Register(){return MatchRecorder::RegisterReplayVisualProvider({Apply,Inspect});}
}
