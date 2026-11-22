#pragma once
#include <algorithm>
#include <charconv>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
namespace CustomComponents::Variables {
enum class Kind {Int,Guid,Entity,Ints,Guids,Entities};
inline constexpr std::size_t MaxVariables=64,MaxArray=256,MaxPayload=65536;
struct Value {
    Kind kind=Kind::Int;std::vector<int> items;
    bool operator==(const Value&)const=default;
    bool Array()const{return int(kind)>=int(Kind::Ints);}
    bool Valid()const{return int(kind)>=0 && int(kind)<=5 && items.size()<=MaxArray && (Array() || items.size()==1) && (kind==Kind::Int || kind==Kind::Ints || std::all_of(items.begin(),items.end(),[](int n){return n>=0;}));}
    std::optional<int> Integer()const{return kind==Kind::Int && items.size()==1?std::optional<int>(items[0]):std::nullopt;}
};
using Table=std::map<std::string,Value,std::less<>>;
inline bool Name(std::string_view name){return !name.empty() && name.size()<=64 && std::all_of(name.begin(),name.end(),[](unsigned char c){return (c>='a' && c<='z') || (c>='A' && c<='Z') || (c>='0' && c<='9') || c=='_';}) && !(name[0]>='0' && name[0]<='9');}
inline std::optional<Kind> ParseKind(std::string_view s){if(s=="Int")return Kind::Int;if(s=="Guid")return Kind::Guid;if(s=="Entity")return Kind::Entity;if(s=="IntArray")return Kind::Ints;if(s=="GuidArray")return Kind::Guids;if(s=="EntityArray")return Kind::Entities;return {};}
inline std::string Encode(const Table& table){
    if(table.size()>MaxVariables)return {};std::string out="V1;";
    for(auto& [name,value]:table){if(!Name(name) || !value.Valid())return {};out+=name+":"+std::to_string(int(value.kind))+":";for(std::size_t i=0;i<value.items.size();++i){if(i)out+=",";out+=std::to_string(value.items[i]);}out+=';';}
    return out.size()<=MaxPayload?out:std::string{};
}
inline std::optional<Table> Decode(std::string_view text){
    if(text.empty())return Table{};if(text.size()>MaxPayload || !text.starts_with("V1;"))return {};text.remove_prefix(3);Table table;
    while(!text.empty()){
        auto end=text.find(';');if(end==text.npos || table.size()==MaxVariables)return {};auto row=text.substr(0,end);text.remove_prefix(end+1);
        auto first=row.find(':'),second=first==row.npos?row.npos:row.find(':',first+1);if(second==row.npos || !Name(row.substr(0,first)))return {};
        int kind=-1;auto k=row.substr(first+1,second-first-1);auto kr=std::from_chars(k.data(),k.data()+k.size(),kind);if(kr.ec!=std::errc{} || kr.ptr!=k.data()+k.size() || kind<0 || kind>5)return {};
        Value value{Kind(kind),{}};auto numbers=row.substr(second+1);
        while(!numbers.empty()){auto comma=numbers.find(',');auto part=numbers.substr(0,comma);int n=0;auto read=std::from_chars(part.data(),part.data()+part.size(),n);if(read.ec!=std::errc{} || read.ptr!=part.data()+part.size() || value.items.size()==MaxArray)return {};value.items.push_back(n);if(comma==numbers.npos)break;numbers.remove_prefix(comma+1);if(numbers.empty())return {};}
        if(!value.Valid() || !table.emplace(std::string(row.substr(0,first)),std::move(value)).second)return {};
    }return table;
}
inline bool Compare(int a,int b,std::string_view op){if(op=="Equal")return a==b;if(op=="NotEqual")return a!=b;if(op=="Greater")return a>b;if(op=="GreaterOrEqual")return a>=b;if(op=="Less")return a<b;if(op=="LessOrEqual")return a<=b;return false;}
inline bool Assign(Table& table,std::string name,const Value& value,std::string_view operation){
    if(!Name(name) || !value.Valid())return false;
    if(operation=="Remove"){table.erase(name);return true;}
    auto found=table.find(name);
    if(operation=="Set"){if(found==table.end() && table.size()==MaxVariables)return false;table[std::move(name)]=value;return true;}
    if(operation=="Add" || operation=="Subtract"){
        auto amount=value.Integer();if(!amount)return false;int previous=0;if(found!=table.end()){auto n=found->second.Integer();if(!n)return false;previous=*n;}else if(table.size()==MaxVariables)return false;
        std::int64_t result=std::int64_t(previous)+(operation=="Add"?std::int64_t(*amount):-std::int64_t(*amount));if(result<INT32_MIN || result>INT32_MAX)return false;table[name]={Kind::Int,{int(result)}};return true;
    }
    if(found==table.end() || !found->second.Array())return false;
    auto& target=found->second;
    if(operation=="Clear"){target.items.clear();return true;}
    if(operation=="RemoveAt"){auto index=value.Integer();if(!index || *index<0 || *index>=int(target.items.size()))return false;target.items.erase(target.items.begin()+*index);return true;}
    if(operation=="RemoveValue"){if(int(value.kind)%3!=int(target.kind)%3)return false;target.items.erase(std::remove_if(target.items.begin(),target.items.end(),[&](int n){return std::find(value.items.begin(),value.items.end(),n)!=value.items.end();}),target.items.end());return true;}
    if(operation=="Append"){if(int(value.kind)%3!=int(target.kind)%3 || target.items.size()+value.items.size()>MaxArray)return false;target.items.insert(target.items.end(),value.items.begin(),value.items.end());return true;}
    return false;
}
}
