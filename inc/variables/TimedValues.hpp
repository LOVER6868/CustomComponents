#pragma once
#include "variables/Values.hpp"
#include "engine/EffectDurations.hpp"
namespace CustomComponents::Variables {
// Expiring operations are stored separately from permanent variable values.
// Shared EffectDurations registry supplies all temporary lifetimes.
struct TimedOperation {std::string name;std::string operation;int duration=0;int amount=0;bool operator==(const TimedOperation&)const=default;};
using TimedTable=std::vector<TimedOperation>;
inline bool ValidTimed(const TimedOperation& v){return Name(v.name) && (v.operation=="Set" || v.operation=="Add" || v.operation=="Subtract") && Engine::EffectDurations::Timed(v.duration);}
inline std::string EncodeTimed(const TimedTable& rows){
    if(rows.size()>256)return {};std::string out="T1;";
    for(auto& row:rows){if(!ValidTimed(row))return {};out+=row.name+":"+row.operation+":"+std::to_string(row.duration)+":"+std::to_string(row.amount)+";";}
    return out.size()<=MaxPayload?out:std::string{};
}
inline std::optional<TimedTable> DecodeTimed(std::string_view text){
    if(text.empty())return TimedTable{};if(text.size()>MaxPayload || !text.starts_with("T1;"))return {};text.remove_prefix(3);TimedTable rows;
    while(!text.empty()){
        auto end=text.find(';');if(end==text.npos || rows.size()>=256)return {};
        auto row=text.substr(0,end);text.remove_prefix(end+1);
        auto a=row.find(':'),b=a==row.npos?row.npos:row.find(':',a+1),c=b==row.npos?row.npos:row.find(':',b+1);
        if(c==row.npos)return {};TimedOperation r{std::string(row.substr(0,a)),std::string(row.substr(a+1,b-a-1)),0,0};
        auto parse=[](std::string_view s,int& n){auto res=std::from_chars(s.data(),s.data()+s.size(),n);return res.ec==std::errc{} && res.ptr==s.data()+s.size();};
        if(!parse(row.substr(b+1,c-b-1),r.duration) || !parse(row.substr(c+1),r.amount) || !ValidTimed(r))return {};
        rows.push_back(std::move(r));
    }return rows;
}
inline bool ApplyTimed(Table& table,const TimedTable& rows){
    for(auto& row:rows){Value v{Kind::Int,{row.amount}};if(!Assign(table,row.name,v,row.operation))return false;}
    return true;
}
}
