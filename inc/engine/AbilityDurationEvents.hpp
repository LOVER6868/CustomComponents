#pragma once
#include "core/Observers.hpp"
#include <BNM/Il2CppHeaders.hpp>
namespace CustomComponents::Engine::AbilityDurationEvents {
using Obj=BNM::IL2CPP::Il2CppObject;
// The existing ordinary-ability expiry owner publishes the same boundaries.
inline Core::Observers<Obj*,int> expire;
}