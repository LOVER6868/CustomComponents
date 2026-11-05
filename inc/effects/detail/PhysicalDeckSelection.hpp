#pragma once
#include <vector>
#include <utility>
namespace CustomComponents::DeckSelection {
struct Entry { int entity=-1,guid=-1; bool reserved=false; };
inline int FirstExactEntry(const std::vector<Entry>& deck,const std::vector<std::pair<int,int>>& matches) {
    for(int i=0;i<int(deck.size());++i) {
        const auto& entry=deck[i];
        if(entry.reserved || entry.entity<=0 || entry.guid<=0) continue;
        for(const auto& match:matches)
            if(match.first==entry.entity && match.second==entry.guid) return i;
    }
    return -1;
}
}
