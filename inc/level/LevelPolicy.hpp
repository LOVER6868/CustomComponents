#pragma once
#include <cstdint>
#include <algorithm>
#include <limits>
#include <vector>
namespace CustomComponents::Level {
struct Stat {int value=0;bool x=false;};
struct Entry {int sun=0;Stat attack,health;};
struct Definition {std::vector<Entry> entries;};
struct Quote {bool valid=false;bool affordable=false;int total=0,extra=0,attack=0,health=0;};
inline Quote Calculate(const Entry& entry,int base,int available) {
    if(base<0 || available<0 || entry.sun < -1)return {};
    const std::int64_t total=entry.sun==-1?available:std::int64_t(base)+entry.sun;
    if(total<0 || total>std::numeric_limits<int>::max())return {};
    const int extra=std::max(0,int(total)-base);
    return {true,total>=base && total<=available,int(total),extra,entry.attack.x?extra:entry.attack.value,entry.health.x?extra:entry.health.value};
}
inline bool Dynamic(const Entry& entry){return entry.sun==-1 || entry.attack.x || entry.health.x;}
inline bool ResetDynamic(const Entry& entry,int base,int available){return Dynamic(entry) && available<=base;}
inline int Next(int entry,int count){return count>0 && entry>=-1 && entry<count ? (entry+1==count?-1:entry+1):-1;}
inline bool CanHighlight(bool stock,int entry,const Quote& quote){return stock && (entry<0 || (quote.valid && quote.affordable));}
}
