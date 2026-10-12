# DrawCardFromSubsetInDeck

`DrawCardFromSubsetInDeckEffectDescriptor` adds a real-deck "pluck" policy to stock
`DrawCardFromSubset` behavior.

## Card-data field

```json
"PluckFailSafe": false
```

`PluckFailSafe` controls only what happens when the requested subset cannot be satisfied
from the player's live deck.

- `false`: fizzle/cancel. Do not conjure a replacement.
- `true`: fall back to ordinary stock `DrawCardFromSubset`, which may conjure a card from
  the subset.

If a matching card exists in the real deck, both values physically draw that card.

## Verified behavior

| Match in real deck | PluckFailSafe | Behavior |
| --- | --- | --- |
| yes | false | real deck draw |
| yes | true | real deck draw |
| no | false | no draw; effect fizzles |
| no | true | stock subset/conjure fallback |

Herald behavior is verified in both successful delivery modes: Herald works on a card
physically plucked from the deck and on a card produced by the stock conjure fallback.

## Implementation overview

The engine currently converts the custom descriptor through the inherited stock
`DrawCardFromSubsetEffectDescriptor.ToEffect`, producing a stock
`DrawCardFromSubsetAbilityEffect`. The module therefore keeps the engine's stock runtime
lifecycle and carries `PluckFailSafe` beside it in native bookkeeping.

```text
custom descriptor
  → preserve PluckFailSafe
  → inherited stock ToEffect
stock DrawCardFromSubsetAbilityEffect
  → mark generated subset children with PendingDraw policy
stock DrawCardFromSubsetEffect
  → pre-check subset against live deck
     ├─ match: move exact entry to deck top, queue DrawCardsFromDeck, cancel delegated child
     ├─ no match + false: cancel/fizzle
     └─ no match + true: original stock ProcessEffect (conjure fallback)
```

## Why the policy has sidecar state

The stock serializer/copy/pool implementation was written before the custom
`PluckFailSafe` field existed. Testing showed that a true value could be lost and arrive
at the descriptor-to-runtime bridge as false.

The system therefore explicitly preserves the policy across descriptor population,
deep-copy, pool return, stock runtime creation, and child-effect correlation. Do not
remove this bookkeeping merely because the custom descriptor also declares a
`BNM_CustomField`.

## Real deck bookkeeping

Ordinary stock draw traffic is used to learn which deck belongs to each hand/player.
During a custom pluck, the module reads the live deck `CardPool`, filters the stock subset
finder's candidate choice to GUIDs physically present in that deck, moves the selected
entry to the top, reserves it, and asks stock `DrawCardAbilitySystem.DrawCardsFromDeck`
to perform the actual draw.

The delegated subset child is cancelled after the real draw is successfully queued. This
is intentional: allowing that child to continue through stock subset delivery caused the
effect group to hang after the physical card entered the hand.

## Herald

There is deliberately no custom Herald bridge. A genuine stock `DrawCardEffect` already
participates in the engine's Herald pipeline. Manual Herald calls and late parent-effect
`DrawnCardId` mutation were tested and rejected.

## Maintenance rules

1. Keep ordinary stock subset effects untouched unless they are correlated to a custom
   descriptor runtime.
2. Keep `PluckFailSafe=false` fail-closed: never silently conjure when the requested card
   is absent or real-deck delivery fails.
3. Keep `PluckFailSafe=true` fallback delegated to the original stock subset processor.
4. Do not assume `DrawCardsFromDeck` completes synchronously.
5. Do not manually run Herald processing.
6. Preserve descriptor policy across copy/pooling and clean sidecar entries on return.
7. Treat any dump RVAs used during investigation as build-specific evidence, not API.

## Relationship to persistent entity-backed decks

Phase 0 now keeps ordinary main-deck cards materialized as real entities through mulligan and gameplay.

`DrawCardFromSubsetInDeck` predates that foundation and remains valid: it still uses stock deck order/manipulation and stock draw delivery. Future cleanup may be able to simplify some real-deck bookkeeping by relying on the persistent entity-backed invariant, but no such simplification should be made without preserving the verified `PluckFailSafe` and delegated-child behavior documented above.
