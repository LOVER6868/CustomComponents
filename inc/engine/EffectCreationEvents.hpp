#pragma once
#include "core/Observers.hpp"
#include <BNM/Il2CppHeaders.hpp>
namespace CustomComponents::Engine::EffectCreationEvents {
using Obj = BNM::IL2CPP::Il2CppObject;
// Owned by the existing EffectCreator adapter; features must not detour it again.
inline Core::Observers<Obj*, Obj*, Obj*> created;
}