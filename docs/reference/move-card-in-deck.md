# Move a deck card

`MoveCardInDeckEffectDescriptor` targets an entity-backed deck card. `Mode: Add` interprets Position as an offset; `Mode: Move` uses an absolute index. Zero is the top; Move -1 is the bottom. Results clamp to deck bounds. Unknown modes do nothing. Identity and modifiers are retained.

See the [examples or related guide](../../examples/deck-order/README.md) and [feature catalog](../FEATURES.md).
