# Card and zone effects

Transfers operate on an existing entity. Subset creation samples a definition
and produces a new card. Keep this distinction when choosing queries and listeners.

## Discard and return

`DiscardFromDeckEffectDescriptor` and `DiscardFromHandEffectDescriptor` move a
physical card to discard, record its origin and emit the discard event. Provenance
Source is `2` for deck and `1` for hand. The module supplies the `discarded` herald
origin independently of HeraldModifiers.

`RemoveFromDiscardPileEffectDescriptor` removes a current discard member.
`ReturnFromDiscardPileEffectDescriptor` and
`ReturnFromDiscardPileToDeckEffectDescriptor` return the original card; the deck
variant accepts its deck-position setting.

`ReturnFromDiscardPileToLaneEffectDescriptor` supports fighters and environments.
Primary selects the discarded card; secondary selects a lane. Fighter capacity
and Team-Up rules apply; water requires active Aquatic. Successful returns emit
play/return events and suppress return-origin MoveTrigger delivery. See
[return-to-lane details](reference/return-to-lane.md).

## Tutor from a deck

`DrawCardFromSubsetInDeckEffectDescriptor` finds a physical deck entry.
`SubsetQueryUsesEntities: true` evaluates live cards; false uses the ordinary
subset-definition domain. `PluckFailSafe: false` fizzles without a match; true
permits the stock subset/conjure fallback.

`HeraldEntities` can modify the delivered card. Herald `ids` are indices in the
same `EffectEntitiesDescriptor.entities` array, not group IDs or card GUIDs.
See [draw examples](../examples/draw-effects/README.md).

## Create from a subset

The stock `CreateCardFromSubsetEffectDescriptor` accepts these extra fields:

| Field | Default | Behavior |
| --- | --- | --- |
| `GrantTeamup` | `false` | Grant Team-Up and use it for fighter placement |
| `GrantAquatic` | `false` | Grant Aquatic and permit water-lane creation |
| `CreationPosition` | Stock rules | `Front`/`Back` overrides ordering; omitted/null retains stock ordering and `CreateInFront` |

False does not remove an innate trait. Grants apply to fighters; environments use
stock placement. Authored subset constraints remain. An Aquatic grant replaces
the lane finder's innate-Aquatic requirement without discarding the subset query
or sampling twice.

`CreateCardInDeckFromSubsetEffectDescriptor` creates `AmountToCreate` copies of
one sampled definition at `DeckPosition: Top`, `Bottom` or `Random`. Amount defaults
to one and position to Top. Zero creates nothing. Invalid inputs or an empty subset
produce no card. Do not enable the old standalone ShuffleRandomCard module too.

## Reorder a physical deck card

`MoveCardInDeckEffectDescriptor` accepts `Mode: Add` or `Move` and signed `Position`.
Add is an offset; Move is an absolute index, with `-1` at the bottom. Results clamp
to deck bounds. Identity and modifiers are retained. See
[deck-order examples](../examples/deck-order/README.md).

## Play an existing deck card into a lane

`PlayFromDeckToLaneEffectDescriptor` takes the physical deck card as its primary
target and the lane as its secondary target. It supports fighters and environments,
uses PF3's existing selection/order handling, and costs no additional hand-play
sun. Authored filters decide which deck cards can be offered; placement checks
still enforce physical ownership, Aquatic, capacity and Team-Up.

The original entity and its modifiers are retained. Successful placement publishes
PlayTrigger once and suppresses origin MoveTrigger delivery. It does not draw,
create a copy, or publish ReturnedFromDiscardPileTrigger. A failed transfer restores
the card's deck position when recovery can safely do so. Empty eligible sets do
nothing. See the [zone examples](../examples/zone-effects/README.md).
