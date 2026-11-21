# Lane checks, damage modifiers and cost buffs

These components are implemented in the native module, so cards using them need a matching CustomComponents build on every client.

## Lane queries

`LaneFighterCountQuery` checks a **fighter** against the number of fighters in that fighter's lane. Set `Faction` to `Plants`, `Zombies`, `SameAsTarget`, or `All`. `Minimum` is inclusive; `Maximum: -1` means no upper limit. For a lane with two Plants, use `Minimum: 2`, `Maximum: -1`, `Faction: "Plants"`. This includes the target fighter in the count. Put the query inside your existing `PrimaryTargetFilter` using `CompositeAllQuery`, not as an alternative to the faction/targetability checks.

`LanesRelativeToEntityQuery` selects **lane entities**, not fighters. `Direction` is `Left`, `Right`, or `Both`. `OriginEntityType` is `Self`, `Source`, or `Target`. `MaximumDistance: 0` takes every lane in the selected direction; `MaximumDistance: 1` takes only immediate neighbors. The origin lane is excluded. Use this where an effect expects lane targets (for example, stock `MoveCardToLanesEffectDescriptor`'s `SecondaryTargetFilter`). The left/right direction uses `Lane.CombatOrderIndex`.

Both queries fail closed when the target/source has no single identifiable lane. See the examples in `examples/new-card-features/`.

## Lane distance multiplier

`VariableMultiplier` accepts `Expression: "LaneDistance"` with `Owner: "PrimaryTarget"` and `Aggregation: "Single"`. It computes `abs(source lane order - primary target lane order)`. From lane 3 to lane 1 it is 2; within lane 3 it is 0. Since the source must be in a lane, the example uses a triggered fighter ability, not a trick played from hand.

## New grantable abilities

The next five `GrantableAbilityType` names and numeric IDs are registered after `OverhealToHealth` (27):

| Ability | ID | Meaning of value |
| --- | --- | --- |
| `DamageBuff` | 28 | Adds that many damage points to hits from this card |
| `NonCombatDamageBuff` | 29 | Adds that many points only to damage from `DamageEffectDescriptor` |
| `NonCombatTruestrike` | 30 | Ability-originated descriptor damage bypasses the hero's block; one active grant is sufficient |
| `Burn` | 31 | Adds that many damage points to hits received, before stock Armor reduces them |
| `BuffMultiplier` | 32 | Multiplies attack and health buff deltas by `1 + value`; value 1 doubles both positive and negative buffs |

You can put an ability directly on a card with `{"InnateValue": 2}` or grant it using `GrantAbilityEffectDescriptor`, `GrantableAbilityType`, `AbilityValue`, and `Duration`. A direct component with empty `$data` defaults to value 1. Both innate values and grant counters contribute to the total. Values are additive across grants; a `BuffMultiplier` of 2 makes buff amounts triple.

**Noncombat limitation:** the code distinguishes descriptor damage by looking up the runtime effect entity and confirming it has `DamageEffectDescriptor`. It does not treat combat attacks as noncombat just because they have an effect entity. Indirect or synthetic damage without that descriptor is not classified as noncombat. This needs device testing with each damage route you plan to use.

`[NonCombatTruestrike] UseTruestrikeIcons=false` (default) removes the Truestrike marker from the newly written damage record for these custom hits; the damage record itself is kept. `true` retains the regular marker. This is a **visual record** option, not a toggle for the unblockable gameplay rule.

## Damage history query

`WasDamagedThisTurnQuery` checks for a positive `DamageRecord` on the card since the most recent `TurnStartRecord`. Healing afterward does not erase the match. The stock metadata does not provide a reliable all-sources combat-only discriminator, so this is **any actual damage this turn**, not `TookCombatDamageQuery`. Like other card queries it can be included in a target filter.

## SunCost modifiers

Stock `ModifySunCostEffectDescriptor` already has `BuffDuration` (`Permanent`, `EndOfTurn`, `NextFighter`). The module now expires `EndOfTurn` cost modifiers with the stock end-of-turn removal, and `NextFighter` modifiers after a fighter enters play. It also prevents the stock `ResetSunCostOnPlaySystem` from clearing a card's cost modifications when the card is played. All remaining cost modifiers on a card are cleared when it leaves a lane by bounce/discard/destruction.

These are system-level hooks, not changes to the JSON serializer. They do not alter the resource cost already paid to play a card. Test board departure, bounced cards, copied effects and simultaneous triggers on-device; this integration has only passed a native C++ syntax check so far.
