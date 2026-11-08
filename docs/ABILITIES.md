# Abilities and triggers

## Granting and removing

Custom grant names are Armor (`19`), Charge (`20`), Thrash (`21`) and RandomKeyword
(`22`), Withering (`23`), Multishot (`24`) and
AttacksOnlyInAdjacentLanes (`25`). Use the stock grant descriptor with the registered ability name. Native removal
uses the same descriptor's ToRemovalEffect path (for example, continuous grants),
plus normal duration expiry; the module adds an explicit RemoveGrantAbilityEffectDescriptor below.
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

## Withering

`Withering` runs at the end-of-turn boundary for either player. Set the grant's
`AbilityValue` to 1 for hand discard, 2 for deck discard, or 3 for board destruction.
A queued action skips a card that has left its required zone. Hand/deck actions
use this module's discard provenance, herald and listener handling; board mode
uses normal destruction. Withering is captured before EndOfTurn grant expiry,
so that duration still activates once before being removed.

Modes do not add together. The latest surviving grant wins; granting again from
the same source/duration replaces that source's mode. Removing/expiring it restores
an older surviving mode. Invalid modes are rejected. Innate examples use the
normal persistent `Counters` data. A grant created during resolution waits until
the next boundary. See the [ability examples](../examples/abilities/README.md).

## Grantable attack markers

Grant `Multishot` or `AttacksOnlyInAdjacentLanes` with the ordinary grant descriptor.
Their values are ignored; use `AbilityValue: 0`. Both retain the game's native
combat behavior. Separate source/duration tracking makes temporary grants expire
without removing an innate marker or another active grant. These are grants, not
new damage multipliers; multiple grants do not multiply the marker's effect.

## Explicit removal

`RemoveGrantAbilityEffectDescriptor` is an authorable component in
`PvZCards.Engine.Components`. It creates the game's existing
`PvZCards.Engine.Effects.RemoveGrantAbilityEffect` runtime effect.

Set `GrantableAbilityType` to any stock or registered custom ability name.
`Mode: All` is the default and removes the ability completely, including an innate
copy. For marker grants it removes both native marker and tracked grants. For
RandomKeyword it removes that system's recorded grants, preserving unrelated
sources of the generated keywords. Active continuous effects can grant abilities
again through their normal rules.

`Mode: SourceAndDuration` removes only the current source card's grant for the
specified `Duration`. That mode preserves innate copies and other sources.
`AbilityValue` is unused. See the [removal examples](../examples/abilities/README.md).

For selectable hand costs and stat bonuses, see [Level](LEVEL.md).

## SelfTeleport

`SelfTeleport` is a card-local grantable permission (registry value 26). Zombie
fighters gain their owner's surprise window; zombie cards with `Surprise` also
gain their owner's main window. It does not grant a player-wide permission.
See [SelfTeleport](SELF_TELEPORT.md) for innate, temporary and reactive grants.
