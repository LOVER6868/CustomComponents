# Investigation log

## 2026-10-05 — Thrash startup hang corrected

The apparent Thrash-hook startup hang was traced to card data, not the first tracer.
Thrash had been added prematurely to the stock `special_abilities` data. Removing that
entry restored startup and the first Thrash tracer was verified working.

Consequences:

- first Thrash tracer: **verified working**;
- Thrash component registration: not the hang cause;
- Thrash ability ID 21 registration: not the hang cause;
- Charge DamageTrace modification: not the hang cause;
- earlier alternative-hook hang conclusions: **confounded/inconclusive**;
- custom Thrash in stock `special_abilities`: **known startup-hang trigger in the tested form**.

Future investigations must change one variable at a time and record card-data changes
alongside native-code changes.

## 2026-10-07 — persistent deck architecture completed

Starting decks were successfully converted from Guid-only `CardPoolEntry` values to live entity-backed entries. Both main decks materialized all 40 cards, opening-hand removal preserved the remaining identities, mulligan cleanup was suppressed, and the final shuffled decks remained fully materialized.

The critical stock virtualization point was confirmed as `RemoveExtraCardsInDeckSystem`: it destroys the remaining materialized deck entities and resets `EntityId=-1`. Suppressing that one cleanup produced the required persistent deck lifetime.

Result: **Phase 0 complete**.

## 2026-10-07 — real `InDeckQuery` lifecycle and dispatch completed

The custom query investigation exposed two separate failures.

First, stock `InstanceManager` pooling called the runtime type through an Activator-style factory and threw:

```text
MissingMethodException: Default constructor not found for type
PvZCards.Engine.Queries.InDeckQuery
```

Adding a real managed parameterless `.ctor` that calls `InHandQuery::.ctor` fixed startup and match entry.

Second, `InDeckQuery.Matches` still was not called. Native inspection showed that `EntityFinder.RunQuery` performs `EntityQuery` interface dispatch. The BNM-created method existed, but the inherited interface vtable entry still pointed to `InHandQuery::Matches`.

An explicit initialization-time repair now redirects:

```text
concrete Matches slot
EntityQuery.Matches interface slot
```

to the custom `InDeckQuery.Matches` pointer.

Runtime logs then confirmed real custom dispatch.

## 2026-10-07 — Phase 1 deck membership and discard completed

`InDeckQuery.Matches` now tests live membership by comparing candidate `Entity.Id` values against `OrderedCardPool.CardPoolEntry.EntityId`.

The final proof showed:

```text
InDeckQuery MATCH TRUE entityId=45
DiscardFromDeck TargetId=45
Deck RemoveCard cardId=45
DiscardPile AddCard CardId=45
```

and independently:

```text
InDeckQuery MATCH TRUE entityId=19
DiscardFromDeck TargetId=19
Deck RemoveCard cardId=19
DiscardPile AddCard CardId=19
```

The old Guid translation layer therefore became redundant and was removed in the production cleanup.

Result: **Phase 1 complete**.

## Production cleanup after Phase 1

Removed diagnostics:

- query lifecycle/acquire tracing,
- constructor/dispatch proof logs,
- per-match membership logs,
- deck mutation snapshots,
- discard AddCard/history tracers,
- Guid translation and pool dumps.

Retained required behavior:

- deck materialization after stock initialization,
- mulligan virtualization suppression,
- real `InDeckQuery`,
- explicit interface-dispatch repair,
- target-owner deck/discard association resolution,
- validation that `TargetId` is actually present in the resolved deck,
- delegation to stock `DiscardFromDeckSystem`.


## Phase 2 — ListPosition targeting

Confirmed the existing stock target-scoping pipeline is sufficient. No custom sorter is needed. Native inspection showed `CalculateSortedValueFromEntityId` only recognizes Attack=1 and Health=2, making value 3 a safe custom extension in this build. The custom hook handles only `ListPosition=3`; every other sort value delegates to stock.

Confirmed deck orientation from stock helpers: index 0 is top/next draw and the last index is bottom. Confirmed discard orientation: `DiscardPileModifier.AddCard` appends, so index 0 is oldest and the last index is newest/top. Literal zero-based list index therefore gives the desired Lowest/Highest semantics in both zones.

Reused the enum-serialization discovery originally proven by custom GrantableAbility values: `DefaultDeserializer.GetValue(Type, object)` is the correct narrow boundary. The production cleanup moves this into shared `EnumSerializationSupport` so GrantableAbility names and `TargetScopeSortValue.ListPosition` do not install competing hooks.

Phase 2 is complete. Phase 3 is `InDiscardPileQuery`.


## 2026-10-08 — Phase 3 query-domain split and discard viewer completed

Implemented `InDiscardPileQuery` as a real BNM managed query using exact live membership against `DiscardPile.CardIds`. While testing stock `DrawCardFromSubset`, observed intermittent unrelated results and fizzles. Native inspection proved the subset finder evaluates `SubsetQuery` over CardDatabase backing entities, then returns a Guid and creates a new card.

The apparent "garbage" was an ID-domain collision: a CardDatabase backing `Entity.Id` could numerically equal a live discarded `EntityId` even though they were unrelated objects. The fix was to separate query types by domain instead of adding fallback behavior.

Final split:

```text
InDeckQuery              live EntityId membership
InDeckGuidQuery          Card.Guid membership
InDiscardPileQuery       live EntityId membership
InDiscardPileGuidQuery   Card.Guid membership
```

A serialized `MatchByGuid` boolean was tested and rejected. The custom query construction path does not populate arbitrary custom `$data` fields, so the mode remained false. Query type identity now selects the behavior directly.

Additional native inspection proved `DiscardPileCleaner` can destroy all entities referenced by a pile and clear `CardIds`. Runtime tracing did not observe that cleaner during the tested normal sequence, so no suppression was added. It remains a lifecycle risk to revisit only if a real test exposes disappearing discard entities.

For visibility/debugging, reused the stock in-battle card inspection overlay. The utility converts a tapped player's `DiscardPile.CardIds` to live entities and then card Guids, creates stock `InspectionItem`/`CardInspectionOverlayConfig` data, and dispatches the stock `ShowCardInspectionOverlaySignal`. No synthetic Unity UI objects are created.

Hero-tap diagnostics exposed an important engine invariant: the local player uses entity/player ID `0` in the tested match. Earlier `> 0` validation rejected the local hero while the enemy hero (`105` in that run) worked. Entity ID zero must therefore be treated as valid; `-1` is the invalid sentinel in these paths.

Phase 3 is considered complete for current card-design work.

