#pragma once
namespace CustomComponents::Effects::SubsetCreationPolicy {
inline bool Valid(int amount,int position,bool query){return amount>=0 && position>=0 && position<=2 && query;}
inline bool Execute(int amount){return amount>0;}
}
