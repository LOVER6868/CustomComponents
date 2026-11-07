#pragma once
#include <cstddef>
#include <optional>
namespace CustomComponents::Engine::HandMode {
inline constexpr int Order=0x4c560001;
inline std::optional<int> Decode(int type,int card,int player,int order,const int* targets,std::size_t count){
    if(type!=0 || card<0 || player<0 || order!=Order || !targets || count!=1 || targets[0]<0 || targets[0]>1024)return std::nullopt;
    return targets[0]-1;
}
}
