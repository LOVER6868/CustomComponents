#pragma once
#include "engine/EntityAccess.hpp"
#include <optional>

namespace CustomComponents::Level {
// The existing Leveled marker stores *extra* sun selected via Level.Value.
// No marker is expected for a normal (unleveled) play: that is level 0.
// Do not infer this number from current cost, which can change independently.
inline std::optional<int> ReadLevelValue(Engine::EntityAccess::Obj* card) {
    if (!card) return std::nullopt;
    auto* marker = Engine::EntityAccess::Component(card, "Leveled");
    if (!marker) return 0;
    auto field = BNM::Class(marker).GetField("Value").cast<int>();
    if (!field.IsValid()) return std::nullopt;
    return field[marker].Get();
}
}
