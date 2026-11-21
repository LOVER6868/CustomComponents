#pragma once
#include <BNM/Il2CppHeaders.hpp>
#include "core/Observers.hpp"
namespace CustomComponents::Engine::EffectLifecycleEvents {
using Obj=BNM::IL2CPP::Il2CppObject;
inline Core::Observers<Obj*,Obj*> copied,damageInherited;
inline Core::Observers<Obj*> returned;
}
