# Abilities and triggers

## Granting and removing

Custom grant names are Armor (`19`), Charge (`20`), Thrash (`21`) and RandomKeyword
(`22`). Use stock grant/remove descriptors with the registered ability name.
`ModifyOperation` supports Add/Set for numeric values. Durations are `Permanent`,
`EndOfTurn` and `NextFighter`.

Armor uses the stock numeric component. Charge stores its amount in inherited
counters. Thrash is a flag: overflow fighter damage goes to the hero opposing the
damage source. See [Armor](reference/armor.md), [Charge](reference/charge.md),
[Thrash](reference/thrash.md) and the [grant lifecycle](reference/grant-ability.md).

## Random keyword

RandomKeyword grants an eligible keyword the target does not already have.
`AbilityValue: 0` uses the faction default: Armor, Untrickable, Bullseye and
Strikethrough for plants; zombies additionally include Deadly and Frenzy.
A positive value selects this bitmask pool:

| Bit | Keyword |
| --- | --- |
| 1 | Armor |
| 2 | Untrickable |
| 4 | Bullseye |
| 8 | Strikethrough |
| 16 | Deadly |
| 32 | Frenzy |

The concrete keyword uses normal grants, durations and history. Source/duration
bookkeeping supports later RandomKeyword removal. Empty pools do nothing.
Selection uses the match RNG. RandomKeyword is a grant option, not an innate
component placed directly on a card.

## Board abilities

Target a stock `BoardAbility` with `DestroyCardEffectDescriptor` to spend it. The
module retains the hidden entity for bookkeeping, stops its triggers and
invalidates its continuous effects. Repeated destruction is harmless. Normal
trigger eligibility still runs before the spent-state check.

## Triggers

Put triggers on effect entities with their normal filters.

| Component | Purpose |
| --- | --- |
| `CustomTrigger` | Listen for `TriggerName` |
| `FireCustomTriggerEffectDescriptor` | Broadcast a named event |
| `FireTriggerEffectDescriptor` | Fire a selected card's named/built-in trigger; `IgnoreTriggerConditions` controls eligibility |
| `DiscardOutsidePlayTrigger` | React to custom hand/deck discard |
| `ReturnedFromDiscardPileTrigger` | React to return from discard |
| `PickFromThreeTrigger` | React to selection with committed picked-card identity |

Use a global filter to receive another card's event. Use a picked-entity query to
modify the exact PF3 card. See [trigger examples](../examples/triggers/README.md).
