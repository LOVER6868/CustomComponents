#pragma once

#include <optional>
#include <string_view>

namespace CustomComponents::Queries::CardClassificationPolicy {

// Actual PvZCards.Engine.Components.RarityValue values (dump.cs).
// Do not equate these with CardData.Rarity or infer them from setAndRarityKey.
inline std::optional<int> ParseRarity(std::string_view rarity) {
    if (rarity == "R1") return 0;
    if (rarity == "R2") return 1;
    if (rarity == "R3") return 2;
    if (rarity == "R4") return 3;
    if (rarity == "R0") return 4;
    if (rarity == "Event") return 5;
    return std::nullopt;
}

inline bool HasRarity(int actual, std::string_view expected) {
    const auto parsed = ParseRarity(expected);
    return parsed && actual == *parsed;
}

// Actual CardColor bit flags from dump.cs. Multi-color definitions can match
// each class bit; no parsing of tags, prefab names, or arbitrary strings.
inline std::optional<int> ParseClass(std::string_view name) {
    if (name == "Kabloom") return 1;
    if (name == "MegaGro") return 2;
    if (name == "Guardian") return 4;
    if (name == "Smarty") return 8;
    if (name == "Solar") return 16;
    if (name == "Brainy") return 32;
    if (name == "Hearty") return 64;
    if (name == "Sneaky") return 128;
    if (name == "Hungry") return 256;
    if (name == "Madcap") return 512;
    return std::nullopt;
}

// CardData.Set is plain metadata, not an Engine component or tag.
// Match exactly as authored in card_data (e.g. "Set3" or "Token").
inline bool HasSet(std::string_view actual, std::string_view expected) {
    return !actual.empty() && !expected.empty() && actual == expected;
}

} // namespace CustomComponents::Queries::CardClassificationPolicy
