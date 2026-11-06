# Draw from a deck subset

`DrawCardFromSubsetInDeckEffectDescriptor` selects a real deck entry. Set `SubsetQueryUsesEntities` for live-card predicates. `PluckFailSafe: false` fizzles without a match; true permits stock subset/conjure fallback. `HeraldEntities` applies to the delivered card in either successful path. Herald `ids` reference array indices in the same EffectEntitiesDescriptor.

See the [examples or related guide](../../examples/draw-effects/README.md) and [feature catalog](../FEATURES.md).
