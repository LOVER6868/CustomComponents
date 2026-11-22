# Lane attacks, death interception and superpowers

These are shared mechanics. No card GUID receives special gameplay behavior.
The examples below are components, not complete cards. Use the usual filters
and conditions on their effect entities.

## Choose attack lanes

Add `PvZCards.Engine.Components.AttackAlsoInLanes` or `AttackOnlyInLanes`, with
`"Lanes": [0, 2, 4]`. Lane numbers are zero-based combat-order indexes, not entity
IDs. Repeated indexes count once. An empty Only list prevents lane attacks.

Also unions its lanes with the normal attack, Multishot and adjacent-lane
attacks. Only replaces those choices, including attacks in all lanes. Native
splash damage still belongs to each actual attack lane.

To grant either marker, use the ordinary `GrantAbilityEffectDescriptor`:

```json
{
  "$type": "PvZCards.Engine.Components.GrantAbilityEffectDescriptor, EngineLib",
  "$data": {
    "GrantableAbilityType": "AttackAlsoInLanes",
    "Lanes": [0, 2, 4],
    "Duration": "EndOfTurn"
  }
}
```

The usual source/duration removal applies. Add grants union lane lists from the
same source; `ModifyOperation: "Set"` replaces that source's list.

Custom combat with ordinary/Also attacks uses the selected pair. Multishot and
adjacent attacks expand around the selected target's lane. With Only or the
native `AttacksInAllLanes`, the first fighter attacks its native opposing side
in the chosen lanes; the selected second fighter does not redirect its aim.
Those lane encounters do not rotate fighters. A manual primary with several
secondary fighters produces sequential encounters in lane order.

Rotation for ordinary selected-pair encounters is presentation-only:

```ini
[CombatPresentation]
RotateFighters=true
```

Same-lane encounters never rotate. Combat remains available when rotation is off.

## Intercept a death

`PvZCards.Engine.Components.PreDiscardFromPlayTrigger` fires while a fighter is
marked for death and still on the board, before the native discard action.
It covers lethal damage, lethal health changes and destruction. Use normal
trigger-source/target filters, conditions and recipient queries. Select the
actual event target; a query that explicitly excludes marked-for-death cards
will intentionally exclude it.

`PvZCards.Engine.Components.CancelDeathEffectDescriptor` cancels that pending
death for its selected recipient. `Health` is optional and defaults to 1; it is
clamped to at least 1 and the card's current modified maximum health.

```json
{
  "$type": "PvZCards.Engine.Components.CancelDeathEffectDescriptor, EngineLib",
  "$data": {"Health": 1}
}
```

The cancellation has an effect only inside this pending-death window. A normal
bounce/zone-transfer effect may instead move the marked fighter out of play;
the continuation rechecks its zone before discarding. Frontend recreation of
the same physical card waits for retirement of its previous Unity view.

## Grant a triggered ability

The stock descriptor lives in `PvZCards.Engine.Effects`, despite being a card
component. It now accepts `Duration` and `ModifyOperation`:

```json
{
  "$type": "PvZCards.Engine.Effects.GrantTriggeredAbilityEffectDescriptor, EngineLib",
  "$data": {
    "AbilityGuid": 123,
    "AbilityValueType": "None",
    "AbilityValueAmount": 0,
    "Duration": "EndOfTurn",
    "ModifyOperation": "Set"
  }
}
```

Replace 123 with the GUID of an actual grantable ability definition. Durations
are `Permanent`, `EndOfTurn` and `NextFighter`, matching ordinary grants.
Set replaces the value; Add adds to it. If the operation is omitted, the native
strongest-value rule remains in force. Independent sources retain independent
contributions. Duration expiry uses native triggered-ability removal and
restores the value from surviving contributions.

Regranting a GUID must reuse its existing live ability entities. Leaving play
clears temporary grant bookkeeping along with the native card reset. Innate
persistent counters are not replaced by a separate card-specific implementation.

## Query and create superpowers

These query types live in `PvZCards.Engine.Queries`:

| Query | Matches |
| --- | --- |
| `InSuperpowerDeckQuery` | A physical entity currently represented in a superpower pool |
| `InSuperpowerDeckGuidQuery` | A definition whose GUID is currently represented in a superpower pool |
| `StartingSuperpowersCardGuidQuery` | A definition from either player's original superpower pool, even after drawing it |
| `SuperpowerDeckSizeQuery` | A player or owned card whose owner's superpower pool has the requested size |
| `RegularDeckSizeQuery` | The equivalent count for the regular deck only |

Size queries use the stock fields `NumCards` and `ComparisonOperator`, with
`Equal`, `LessOrEqual` or `GreaterOrEqual`. Membership GUID queries search the
match's pools, like `InDeckGuidQuery`; they do not invent an owner for a database
definition. Add normal selection filters where appropriate.

Use `PvZCards.Engine.Components.CreateCardInSuperpowersFromSubset` on an effect
entity targeting the recipient hero/player. It accepts `SubsetQuery`,
`AmountToCreate` (default 1), and native `DeckPosition` (`Random`, `Top`, `Bottom`).
It selects one definition and inserts that many copies, as regular subset-deck
creation does. It modifies the superpower pool, not the regular deck.

The shield display follows the remaining superpower count. Adding powers
reactivates an exhausted display; counts above three use one golden shield and
a label cloned from the game's native count text on each side. This UI still
requires a device check for alignment and animation interactions.

## Two selection extensions

`PrimaryTargetsGuidQuery` has the same `FromAbilityGroupId` field and ambiguity
rules as `PrimaryTargetsQuery`. The ordinary query matches selected physical
entities; the GUID query matches definitions of those selected cards.

Place `IgnoreCheatTagQuery` directly in the subset query, or inside a
`CompositeAllQuery`, to omit the native subset finder's automatic `cheats` tag
exclusion. Other constraints still apply. The opt-in is not discovered through
OR branches or negations, which would otherwise widen unrelated branches.
It does not remove tags from card definitions.

## Verify before release

A compile is not a gameplay result. See [release validation](RELEASE_VALIDATION.md)
for the combined capture. In particular, verify death interception/revive,
repeated ability grants, large superpower pools and multiplayer agreement on
both clients before merging.