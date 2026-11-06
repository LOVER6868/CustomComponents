#pragma once
#include <BNM/Il2CppHeaders.hpp>
#include "core/Observers.hpp"
namespace CustomComponents::Engine::Events {
using Obj=BNM::IL2CPP::Il2CppObject;
struct Effect {Obj* system;Obj* model;Obj* effect;};
struct View {Obj* mapper;Obj* model;int entityId;Obj* view;};
struct Deck {Obj* initializer;Obj* playerData;Obj* player;};
// Borrowed objects are valid only during notification. A subscriber needing
// asynchronous access must acquire its own managed lease.
inline Core::Observers<const Effect&> afterBuff;
inline Core::Observers<const View&> afterViewAdded;
inline Core::Observers<Obj*> afterShowInHand;
inline Core::Observers<const Deck&> beforeDeckInitialize,afterDeckInitialize;
}
