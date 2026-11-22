#pragma once
namespace CustomComponents::Abilities::GrantModes {
enum class WitheringZone : int {None=0,Hand=1,Deck=2,Board=3};
inline constexpr int ToNative(WitheringZone zone){return static_cast<int>(zone);}
inline constexpr WitheringZone ToZone(int mode){return static_cast<WitheringZone>(mode);}
constexpr bool ValidWithering(WitheringZone zone){
    return zone==WitheringZone::Hand || zone==WitheringZone::Deck || zone==WitheringZone::Board;
}
constexpr bool ValidWithering(int mode){return ValidWithering(ToZone(mode));}
constexpr bool MatchesZone(WitheringZone zone,bool hand,bool deck,bool board){
    switch(zone){
    case WitheringZone::Hand:return hand && !deck && !board;
    case WitheringZone::Deck:return deck && !hand && !board;
    case WitheringZone::Board:return board && !hand && !deck;
    default:return false;
    }
}
constexpr bool MatchesZone(int mode,bool hand,bool deck,bool board){return MatchesZone(ToZone(mode),hand,deck,board);}
template<class Entry> int LastWitheringMode(const Entry* entries,int count) {
    for(int i=count-1;i>=0;--i)if(ValidWithering(entries[i].value))return entries[i].value;
    return ToNative(WitheringZone::None);
}
constexpr bool KeepMarker(bool innate,bool granted){return innate || granted;}
}
