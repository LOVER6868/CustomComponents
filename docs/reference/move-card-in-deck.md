# MoveCardInDeckEffectDescriptor (experimental)

Moves an **existing targeted card** to another position **within its owner's live deck**.
It does not draw, create, discard, swap ownership, or change the card's `EntityId`.

The JSON descriptor has **exactly two fields**:

```json
{
  "$type": "PvZCards.Engine.Components.MoveCardInDeckEffectDescriptor, EngineLib, Version=1.0.0.0, Culture=neutral, PublicKeyToken=null",
  "$data": {"Mode": "Add", "Position": -2}
}
```

| Mode | Position | Meaning |
| --- | --- | --- |
| `Add` | Signed integer | Shift the targeted card by that many **indices** (negative = towards top) |
| `Move` | `0` | Make the card the top / next-drawn card |
| `Move` | `1` | Place it second from the top |
| `Move` | `-1` | Make it bottom / last card |
| `Move` | `-2` | Second from bottom |

Indices are zero-based. Both modes **clamp to `[0, deck_size-1]`**. `Move`
negative values are relative to the end, not offsets from the card's current
position. On a five-card deck `[A,B,C,D,E]`, targeting `C`:

- `Add -2` -> `[C,A,B,D,E]`
- `Add +1` -> `[A,B,D,C,E]`
- `Move 0` -> `[C,A,B,D,E]`
- `Move -1` -> `[A,B,D,E,C]`

`Mode` omitted or empty defaults to `Add`; an unknown mode produces a safe no-op.
`Position` defaults to `0`. Inputs outside the 27-bit runtime carrier range
are saturated before the effect is queued, well beyond realistic deck sizes.

## Usage

Use `PrimaryTargetFilter` with `InDeckQuery` to select the *actual entity-backed
card* in a deck, not a card GUID. Samples:

- `examples/deck-order/move-card-in-deck-add-minus-two.json`
- `examples/deck-order/move-card-in-deck-move-to-top.json`
- `examples/deck-order/move-card-in-deck-move-to-bottom.json`

The minimal examples assume the same `InDeckQuery` infrastructure required by
existing `DiscardFromDeckEffectDescriptor` examples. Add your usual target
constraints (faction/ownership or sorting) if the target must come from only
one deck. The effect itself looks up the **target card's owner** and verifies
that exact entity is in the owner's current `OrderedCardPool.Entries`.

## Engine integration

`ReturnCardToPlayersDeckEffectDescriptor.ToEffect` and
`ReturnCardToPlayersDeckSystem.ProcessEffect` already have a **single hook
owner** in `ReturnDiscardToHandSystem.hpp`. We reuse those hooks and transport
the custom Mode/Position in a reserved `ReturnCardToPlayersDeckEffect.DeckPosition`
value, rather than installing an overlapping hook. The stock runtime's
DeepCopyFrom preserves that integer, so no sidecar map or custom runtime
EffectRouter registration is needed.

At processing time, the existing hook detects the marker, resolves the target
entity -> its owner -> owner's deck -> `OrderedCardPool`, then calls the managed
list's `RemoveAt` and `Insert` on the **same CardPoolEntry instance**. All
other stock deck effects use the original hook path unchanged.

The descriptor declares managed `Mode` (`System.String`) and `Position`
(`System.Int32`) fields. `CustomFieldBridgeSystem` registers those fields for
JSON deserialization, and custom descriptor lifecycle methods preserve/reset
them during deep copy and pool return.

## Required device tests (not yet verified)

1. Start with five distinct known-order cards and target the middle one.
2. Verify `Add -2`, `Add +1`, and oversized positive/negative shifts.
3. Verify `Move 0`, `Move -1`, `Move -2`.
4. Draw cards after each move and verify the **actual** order.
5. Verify targeting a card that left the deck is a no-op.
6. Verify the original return-from-discard and stock return-to-deck effects.
7. Verify local/opponent deck ownership and multiplayer if applicable.
8. Check descriptor deep copy, pooled reuse, and effect-group progression.

This is **source-validated, not runtime-confirmed**. No animation hook or
special presentation record is added for an invisible deck reorder.
