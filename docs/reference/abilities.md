# Abilities and triggers

## Grants and removal

Use stock `GrantAbilityEffectDescriptor` with `GrantableAbilityType` set to a registered ability name. The module adds these grantable types:

| Ability | ID | Behavior |
| --- | ---: | --- |
| `Armor` | 19 | Add/set armor using the stock armor stat |
| `Charge` | 20 | Adjust block-meter charge from damage |
| `Thrash` | 21 | Excess fighter damage reaches the opposing hero |
| `RandomKeyword` | 22 | Give one eligible missing keyword |
| `Withering` | 23 | Discard or destroy the card at turn end |
| `Multishot` | 24 | Use the game's multishot attack marker |
| `AttacksOnlyInAdjacentLanes` | 25 | Use the game's adjacent-lane marker |
| `SelfTeleport` | 26 | Permit extra phases for that specific card |
| `OverhealToHealth` | Custom registered ability | Turn excess healing into a stock health buff |

Armor also uses the game’s normal special-ability icon. Charge keeps its value in inherited counters; active counter values add together. Thrash sends excess resolved fighter damage to the hero opposing the damage source, including when the damage source hits itself. No overflow means no extra damage.

Common durations are `Permanent`, `EndOfTurn`, and `NextFighter`. Numeric grants can use `ModifyOperation: "Add"` or `"Set"`; marker abilities ignore the number. Use `RemoveGrantAbilityEffectDescriptor` with `Mode: "All"` to remove even innate state, or `Mode: "SourceAndDuration"` to remove only one source's grant. Ordinary duration expiry still works. The stock descriptor’s removal path is also used when continuous effects expire; explicit removals create the same kind of runtime removal effect. Generated runtime-effect shapes are for inspection, not card authoring. See [removal examples](../../examples/abilities/removal/README.md).

## RandomKeyword

An `AbilityValue` of 0 chooses the normal faction pool. Plants can receive Armor, Untrickable, Bullseye or Strikethrough; zombies also include Deadly and Frenzy. Set a positive bitmask for a restricted pool: Armor `1`, Untrickable `2`, Bullseye `4`, Strikethrough `8`, Deadly `16`, Frenzy `32`. Only available eligible keywords are considered. The granted keyword uses normal ability duration/source handling, so removal doesn't disturb unrelated grants of the same keyword.

## Withering

Use `AbilityValue: 1` to discard from hand, `2` to discard from deck, or `3` to destroy on the board at turn end. A card that leaves the expected zone before the queued action resolves is skipped. The latest surviving source/duration mode wins; the modes do not add together. An EndOfTurn grant active at the boundary still acts before its expiry.

## SelfTeleport

`SelfTeleport` belongs to one card, not the whole player. A zombie fighter can be played during its owner's surprise/tricks phase; a zombie card with `Surprise` can also be played during the owner's main phase. It can be innate or granted for a duration. `Innate: false` on the component means grants control the active state. Conditional continuous grants are withdrawn when their variable/query condition stops passing. This does not open a general player-wide play window.

See [SelfTeleport examples](../../examples/variables/README.md), including a conditional grant.

## Triggers and board abilities

Put a trigger component and its `TriggerTargetFilter` on an effect entity. `CustomTrigger` listens for a named event. `FireCustomTriggerEffectDescriptor` broadcasts one; `FireTriggerEffectDescriptor` fires a chosen built-in or named trigger and can respect or ignore conditions. The module also provides `DiscardOutsidePlayTrigger`, `ReturnedFromDiscardPileTrigger` and `PickFromThreeTrigger`.

Hidden `BoardAbility` entities can be spent by targeting them with `DestroyCardEffectDescriptor`. Their triggers and continuous effects stop after destruction. Refer to [trigger examples](../../examples/triggers/README.md) for the actual component nesting.
