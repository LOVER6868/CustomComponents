#pragma once
#include <vector>
#include <string>
#include <charconv>
#include <optional>
namespace CustomComponents::Abilities::KeywordPolicy {
struct Option {int mask,type,value;const char* component;};
inline constexpr Option options[]={{1,19,1,"Armor"},{2,17,0,"Untrickable"},{4,4,0,"Truestrike"},{8,5,0,"Strikethrough"},{16,2,0,"Deadly"},{32,3,0,"Frenzy"}};
struct Given {int source,type,duration;};
inline bool Known(int type){for(auto option:options)if(option.type==type)return true;return false;}
inline std::vector<Given> Decode(const std::string& text) {
    std::vector<Given> result;std::size_t start=0;
    while(start<text.size()) {
        auto end=text.find(';',start);if(end==std::string::npos)end=text.size();
        const char* cursor=text.data()+start;const char* limit=text.data()+end;Given entry{};bool valid=true;
        int* fields[]={&entry.source,&entry.type,&entry.duration};
        for(int i=0;i<3;++i) {auto parsed=std::from_chars(cursor,limit,*fields[i]);if(parsed.ec!=std::errc{} || parsed.ptr==cursor){valid=false;break;}cursor=parsed.ptr;if(i<2){if(cursor==limit || *cursor++!=':'){valid=false;break;}}}
        if(valid && cursor==limit && entry.source>=0 && Known(entry.type) && entry.duration>=0 && entry.duration<=2)result.push_back(entry);
        start=end+1;
    }
    return result;
}
inline std::string Encode(const std::vector<Given>& entries) {
    std::string result;for(auto entry:entries){if(!result.empty())result+=';';result+=std::to_string(entry.source)+":"+std::to_string(entry.type)+":"+std::to_string(entry.duration);}return result;
}
inline std::optional<int> Take(std::vector<Given>& entries,int source,int duration) {
    for(std::size_t i=entries.size();i-->0;)if(entries[i].source==source && entries[i].duration==duration){int type=entries[i].type;entries.erase(entries.begin()+i);return type;}return {};
}
template<class Has> std::vector<const Option*> Eligible(int mask,Has has) {
    std::vector<const Option*> result;for(const auto& option:options)if((mask&option.mask) && !has(option))result.push_back(&option);return result;
}
}
