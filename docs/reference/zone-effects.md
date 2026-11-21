# Card and zone effects

The main distinction is whether an effect moves a card that already exists or creates a new one from a definition. That determines which queries and identities you should use.

## Moving an existing card

`DiscardFromDeckEffectDescriptor` and `DiscardFromHandEffectDescriptor` discard a physical card and record its origin (`Source: 2` for deck, `1` for hand). `RemoveFromDiscardPileEffectDescriptor` removes a current discard member. `ReturnFromDiscardPileEffectDescriptor` returns the same card to hand; `ReturnFromDiscardPileToDeckEffectDescriptor` sends it back to a chosen deck position.

`ReturnFromDiscardPileToLaneEffectDescriptor` returns an original discarded fighter or environment. It needs a primary card target and a secondary lane target. Fighter placement checks capacity, Team-Up and Aquatic; environments use their normal slot rules. It is a return, not a fresh card definition. The return records play/return events but suppresses return-origin `MoveTrigger`; the compatibility `ForceFaceDown` setting does not create a different face-down return path.

`PlayFromDeckToLaneEffectDescriptor` places an existing deck card on the board with its physical identity. `MoveCardInDeckEffectDescriptor` reorders a card in the physical deck. `Mode: "Add"` applies `Position` as a signed offset; `Mode: "Move"` uses an absolute index. Zero means top; `Move` with `Position: -1` means bottom. Results clamp to deck bounds, retaining the card’s identity and modifiers.

## Drawing or creating cards

`DrawCardFromSubsetInDeckEffectDescriptor` tutors an existing deck card using a subset query. `SubsetQueryUsesEntities` selects live-card predicates; `PluckFailSafe: false` fizzles without a match, while true permits the stock fallback. `HeraldEntities` refers to the delivered card, and its `ids` use effect-entity indices within the same descriptor. `CreateCardInDeckFromSubsetEffectDescriptor` instead samples a *definition* and inserts new copies. `CreateCardFromSubsetEffectDescriptor` can apply Team-Up/Aquatic options or choose front/back placement, subject to the stock system's legal-lane checks.

Use database queries when sampling definitions, and physical zone queries when choosing exact deck/discard members. If you need a picked physical card later, store its entity ID rather than assuming any copy of its GUID is equivalent.

[Zone-effect examples](../../examples/zone-effects/README.md) · [Drawing examples](../../examples/draw-effects/README.md) · [Deck-order examples](../../examples/deck-order/README.md)

### PlayFromDeckToLane creation options and stock subset fallback

`PlayFromDeckToLaneEffectDescriptor` can use the same creation settings as
`CreateCardFromSubsetEffectDescriptor`. Its new JSON `$data` fields are:

- `PluckFailSafe` (bool, default `false`): when the selected physical card
  cannot be taken from the real deck, try the stock
  `CreateCardSystem.ProcessEffect(CreateCardFromSubsetEffect)` instead.
  When `false` the transfer fizzles; it **never** conjures by default.
- `SubsetQuery` (EntityQuery, optional when fail-safe is false): defines the
  fallback card subset, exactly as in stock `CreateCardFromSubsetEffectDescriptor`.
  A fail-safe with no query cannot conjure. When a Pick-from-Three effect
  selected the guid, `PickedFromThreeQuery` is generally appropriate.
- `ForceFaceDown` (bool): face-down physical fighter placement and stock
  face-down behavior when the fallback creates a card.
- `GrantTeamup`, `GrantAquatic` (bool): grant those abilities for a physically
  transferred fighter or when stock subset creation is used.
- `CreationPosition` (`"Stock"`, `"Front"`, or `"Back"`): prefer the given
  position for the physical transfer and pass it to stock subset creation.

The **physical transfer always takes precedence** and preserves the original
card identity and buffs. Only a missing or unplaceable physical source at the
pre-mutation stage can enter the fallback; a transfer failure after deck removal
is not allowed to conjure a duplicate. When fallback occurs, the *stock*
subset creation code chooses and creates the card with the same lane and
creation settings; unlike a deck transfer, it does not remove a real card.

A physical-card primary target must still pass its target-selection query.
`PluckFailSafe` does not make an empty `InDeckQuery` produce targets, and does
not bypass a manual lane-selection check. For Pick-from-Three, the fallback
`SubsetQuery` can use `PickedFromThreeQuery` to carry the selected guid.

See:
`examples/zone-effects/play-from-deck-to-lane-strict.json`,
`play-from-deck-to-lane-fallback-teamup-aquatic-front.json`,
`play-from-deck-to-lane-fallback-facedown-back.json`, and
`pf3-play-fighter-from-deck-to-lane-with-fallback.json`.
