#pragma once
#include <string_view>
namespace CustomComponents::Pick3::AdditionalSelection {
inline constexpr int Inherit=-1,Invalid=-2,All=0,Manual=1,Random=2,PickFromThree=3,UniqueGuid=4;
inline int Parse(std::string_view name) {
    if(name=="Inherit" || name=="-1")return Inherit;
    if(name=="All" || name=="0")return All;
    if(name=="Manual" || name=="1")return Manual;
    if(name=="Random" || name=="2")return Random;
    if(name=="PickFromThree" || name=="3")return PickFromThree;
    if(name=="PickFromThreeUniqueGuidTargets" || name=="4")return UniqueGuid;
    return Invalid;
}
inline bool Interactive(int mode){return mode==Manual || mode==PickFromThree || mode==UniqueGuid;}
inline constexpr int OrderMarker=0x63000000;
inline int Encode(int target){return target>=0 && target<0x1000000?OrderMarker|target:0;}
inline int Decode(int order){return (order&0xff000000)==OrderMarker?order&0xffffff:-1;}
}
