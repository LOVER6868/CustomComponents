#pragma once
#include <cstdint>

// Custom IDs are persisted in authored JSON and native AbilityRegistry entries.
// Never renumber: new IDs must be allocated after the existing range.
namespace CustomComponents::Abilities::CustomAbilityIds {
enum class Id : std::int32_t {
    Armor=19, Charge=20, Thrash=21, RandomKeyword=22, Withering=23,
    Multishot=24, AttacksOnlyInAdjacentLanes=25, SelfTeleport=26,
    OverhealToHealth=27, DamageBuff=28, NonCombatDamageBuff=29,
    NonCombatTruestrike=30, Burn=31, BuffMultiplier=32,
    AttackAlsoInLanes=33, AttackOnlyInLanes=34, ExtraDrawPerTurn=35
};
inline constexpr int ToNative(Id id){return static_cast<int>(id);}
} // namespace CustomComponents::Abilities::CustomAbilityIds
