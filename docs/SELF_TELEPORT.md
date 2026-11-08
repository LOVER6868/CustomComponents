# SelfTeleport

SelfTeleport is a grantable permission on one card, not the player. Add
`PvZCards.Engine.Components.SelfTeleport` to a card, or use
`GrantAbilityEffectDescriptor` with `GrantableAbilityType: SelfTeleport` and
`AbilityValue: 1`. Authored instances are innate by default; explicit
`Innate: false` represents a component relying on granted counters.

| Card with active SelfTeleport | Extra play window |
| --- | --- |
| Zombie fighter | Zombie surprise/tricks phase |
| Zombie card with `Surprise` | Zombie main phase, including tricks and environments |
| Card without either characteristic | No additional window |

Cards retain their usual play windows. Resource costs, owner, turn, placement,
targets, Unplayable and per-ability play requirements remain authoritative.
The stock player-wide `CanPlayFighterInSurprisePhase` permission is unchanged.

Triggered grants support the existing durations and removal system. A
`RemoveGrantAbilityEffectDescriptor` removes it using the usual modes, including
innate removal. Continuous grants can depend on a
`VariableComparisonQuery` in `SelfEntityFilter`; they are recalculated after
simulation changes and stop applying when their conditions fail.
See [variables](VARIABLES.md) for reactive source zones and constraints.

Examples: [innate](../examples/variables/self-teleport.json),
[temporary grant](../examples/variables/grant-self-teleport.json),
[removal](../examples/variables/remove-self-teleport.json), and
[conditional grant](../examples/variables/conditional-self-teleport.json).

The implementation is in `abilities/SelfTeleport.hpp` and
`engine/PhasePermissions.hpp`. Phase exceptions are scoped to the relevant
native validators rather than bypassing the complete play validator. Both PvP
clients require matching code and definitions; first-time device/PvP validation
should cover grants expiring and unrelated cards retaining their stock windows.
