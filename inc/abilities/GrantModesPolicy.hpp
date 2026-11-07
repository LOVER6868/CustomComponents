#pragma once
namespace CustomComponents::Abilities::GrantModes {
constexpr bool ValidWithering(int mode) { return mode >= 1 && mode <= 3; }
constexpr bool MatchesZone(int mode,bool hand,bool deck,bool board) {
    return mode==1 ? hand && !deck && !board : mode==2 ? deck && !hand && !board : mode==3 && board && !hand && !deck;
}
template<class Entry> int LastWitheringMode(const Entry* entries,int count) {
    for(int i=count-1;i>=0;--i)if(ValidWithering(entries[i].value))return entries[i].value;
    return 0;
}
constexpr bool KeepMarker(bool innate,bool granted) { return innate || granted; }
}
