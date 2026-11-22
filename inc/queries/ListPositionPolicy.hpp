#pragma once
#include <string_view>
namespace CustomComponents::Queries::ListPosition {
enum class Pool { Invalid,Deck,DiscardPile };
inline Pool ParsePool(std::string_view value) {
    if(value=="Deck")return Pool::Deck;
    if(value=="DiscardPile")return Pool::DiscardPile;
    return Pool::Invalid;
}
inline bool Compare(int index,int count,int position,bool fromEnd,std::string_view op) {
    if(count<=0 || index<0 || index>=count || position<0)return false;
    const int actual=fromEnd?count-1-index:index;
    if(op=="Equal")return actual==position;
    if(op=="LessOrEqual")return actual<=position;
    if(op=="GreaterOrEqual")return actual>=position;
    if(op=="NotEqual")return actual!=position;
    if(op=="Less")return actual<position;
    if(op=="Greater")return actual>position;
    return false;
}
}
