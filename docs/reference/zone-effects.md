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
