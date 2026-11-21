#pragma once
// The native entry points below each have exactly one Dobby owner. Other
// features observe the owner instead of installing another detour.
#include <BNM/BasicMonoStructures.hpp>
#include "core/Observers.hpp"

namespace CustomComponents::Core::SharedHookEvents {
using Obj = BNM::IL2CPP::Il2CppObject;
// before/after StartCardDragCommand.Execute, after the Level state update
inline Observers<Obj*, bool> startCardDrag;
// before/after PrimaryTargetEffectCreator.CreateEffectsBasedOnTargetMethod.
// The result is null on entry, and the returned effect list on exit.
inline Observers<Obj*, Obj*, Obj*, Obj*, Obj*, Obj*, bool> primaryTargetMethod;
// before/after HighlightPlayableLocationsOrTargetsForCardCommand.Execute;
// the post event also fires when Level deliberately skips the stock command.
inline Observers<Obj*, bool> highlightPlayableLocations;
}
