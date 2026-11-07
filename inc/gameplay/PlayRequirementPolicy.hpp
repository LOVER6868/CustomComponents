#pragma once
#include <charconv>
#include <string>
#include <string_view>
#include <vector>
namespace CustomComponents::PlayRequirements {
struct Decision {int effect;bool required;bool allowed;bool targets=true;};
inline bool HasRequirements(const std::vector<Decision>& decisions){for(auto d:decisions)if(d.required)return true;return false;}
inline bool MayPlay(const std::vector<Decision>& decisions){if(!HasRequirements(decisions))return true;for(auto d:decisions)if(d.allowed)return true;return false;}
inline bool MayCast(const std::vector<Decision>& decisions){if(!HasRequirements(decisions))return true;for(auto d:decisions)if(d.allowed && d.targets)return true;return false;}
inline std::string Encode(const std::vector<Decision>& decisions){std::string text;for(auto d:decisions)if(d.required){text+=std::to_string(d.effect);text+=d.allowed?":1;":":0;";}return text;}
inline bool Lookup(std::string_view text,int effect,bool& allowed) {
    while(!text.empty()){
        int id=-1;auto result=std::from_chars(text.data(),text.data()+text.size(),id);
        if(result.ec!=std::errc{} || text.data()+text.size()-result.ptr<3 || result.ptr[0]!=':' || (result.ptr[1]!='0' && result.ptr[1]!='1') || result.ptr[2]!=';')return false;
        if(id==effect){allowed=result.ptr[1]=='1';return true;}
        text.remove_prefix(std::size_t(result.ptr+3-text.data()));
    }
    return false;
}
}
