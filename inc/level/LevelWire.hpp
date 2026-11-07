#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>
namespace CustomComponents::Level::Wire {
// Private, versioned GameCommandDto protobuf extension; native fields are untouched.
inline constexpr std::uint32_t Field=50001;
struct Choice {bool present=false;bool valid=true;int entry=-1;};
inline void Varint(std::vector<std::uint8_t>& bytes,std::uint64_t value){do{auto b=std::uint8_t(value&127);value>>=7;bytes.push_back(b|(value?128:0));}while(value);}
inline bool Read(const std::uint8_t* data,std::size_t size,std::size_t& pos,std::uint64_t& value){value=0;for(int shift=0;shift<64;shift+=7){if(pos==size)return false;auto b=data[pos++];if(shift==63 && b>1)return false;value|=std::uint64_t(b&127)<<shift;if(!(b&128))return true;}return false;}
inline std::vector<std::uint8_t> Encode(int entry) {std::vector<std::uint8_t> bytes;if(entry < -1 || entry>=1024)return bytes;Varint(bytes,std::uint64_t(Field)<<3);Varint(bytes,(std::uint64_t(1)<<32)|std::uint32_t(entry+1));return bytes;}
inline Choice Decode(const std::uint8_t* bytes,std::size_t size) {
    Choice result;std::size_t pos=0;
    while(pos<size){std::uint64_t key=0,value=0;if(!Read(bytes,size,pos,key) || key==0)return {true,false,-1};const auto field=key>>3;const auto type=key&7;
        if(type==0){if(!Read(bytes,size,pos,value))return {true,false,-1};if(field==Field){if(result.present || (value>>32)!=1 || (value&0xffffffff)>1024)return {true,false,-1};result={true,true,int(value&0xffffffff)-1};}}
        else if(type==1){if(size-pos<8)return {true,false,-1};pos+=8;}
        else if(type==2){if(!Read(bytes,size,pos,value) || value>size-pos)return {true,false,-1};pos+=std::size_t(value);}
        else if(type==5){if(size-pos<4)return {true,false,-1};pos+=4;}
        else return {true,false,-1};
    }return result;
}
}
