#pragma once
#include "core/AtomicSnapshot.hpp"
#include <mutex>
#include <vector>
#include <BNM/Class.hpp>
#include <BNM/Method.hpp>
#include <BNM/Field.hpp>
namespace CustomComponents::Engine::AbilityConversion {
using Obj=BNM::IL2CPP::Il2CppObject;using Converter=bool(*)(Obj*,Obj*,Obj*,Obj*&);
inline std::mutex lock;
inline Core::AtomicSnapshot<std::vector<Converter>> converters{std::make_shared<const std::vector<Converter>>()};
inline bool Add(Converter callback){if(!callback)return false;std::lock_guard<std::mutex> guard(lock);auto prior=converters.load();for(auto item:*prior)if(item==callback)return true;auto next=std::make_shared<std::vector<Converter>>(*prior);next->push_back(callback);converters.store(next);return true;}
inline bool Convert(Obj* descriptor,Obj* instances,Obj* entity,Obj*& result){for(auto callback:*converters.load())if(callback(descriptor,instances,entity,result))return true;return false;}
}
