# Deck / discard implementation status

## Complete

### Phase 0 — persistent entity-backed decks

Main-deck `CardPoolEntry` objects keep real `EntityId` values from initialization through mulligan and gameplay.

Required production behavior:

- materialize after stock `DeckInitializer.InitializeDeck`,
- create cards through stock `CardEntityFactory.CreateCardEntityWithDefaultOwner`,
- preserve existing pool entries/order,
- suppress `RemoveExtraCardsInDeckSystem` mulligan cleanup.

### Phase 1 — real `InDeckQuery` + `DiscardFromDeck`

`InDeckQuery` is a real managed class inheriting stock `InHandQuery` for lifecycle only.

Membership:

```text
Entity.Id == OrderedCardPool.CardPoolEntry.EntityId
```

The explicit vtable/interface dispatch repair is required in the investigated build.

`DiscardFromDeck` receives the selected real deck entity ID directly. The custom bridge resolves only `DeckId` and `DiscardPileId`, validates membership, then delegates to stock processing.

### Phase 2 — `ListPosition`

Generic ordered-zone sorting is complete:

```text
None         = 0
Attack       = 1
Health       = 2
ListPosition = 3
```

Deck: Lowest=top, Highest=bottom.

Discard: Lowest=oldest, Highest=newest/top.

Implementation contract:

- JSON name `ListPosition` is translated to enum value 3 by the shared enum serializer.
- `TriggeredEffectTargetScoper.CalculateSortedValueFromEntityId` is hooked only for value 3.
- stock Attack/Health sorting and stock Lowest/Highest comparison remain untouched.
- the returned value is the literal zero-based index in `OrderedCardPool.Entries` or `DiscardPile.CardIds`.

### Phase 3 — discard membership, database-query variants, and stock discard viewer

Phase 3 is complete for the current card-design requirements.

Live-zone membership uses exact runtime entity identity:

```text
InDeckQuery
    candidate Entity.Id == OrderedCardPool.CardPoolEntry.EntityId

InDiscardPileQuery
    candidate Entity.Id == any DiscardPile.CardIds entry
```

Stock `DrawCardFromSubset` does **not** evaluate its subset query over live match entities. `CardFromSubsetFinder` calls `CardDatabase.FindEntities`, so the query candidate is a CardDatabase backing entity and the final result is a Guid used to create a new card.

Separate JSON-visible query types handle that domain explicitly:

```text
InDeckGuidQuery
    candidate Card.Guid == any CardPoolEntry.Guid

InDiscardPileGuidQuery
    candidate Card.Guid == Card.Guid of any live entity referenced by DiscardPile.CardIds
```

Do not merge these modes back into one query with a serialized flag. Custom query `$data` was proven not to populate arbitrary custom fields in the current construction path.

Discard viewer proof uses only stock UI infrastructure:

```text
plain hero tap
    -> stock card-inspection command path
    -> resolve that player's DiscardPileAssociation
    -> DiscardPile.CardIds -> live Entity -> Card.Guid
    -> stock InspectionItem list
    -> stock CardInspectionOverlayConfig
    -> ShowCardInspectionOverlaySignal
```

Either player's discard pile can be viewed. No synthetic Unity `GameObject`, Canvas, or custom screen is created by the mod.

Important runtime invariant discovered while wiring local hero taps: **EntityId 0 is valid** and is commonly the local player entity. Player/entity checks must use `>= 0` when `-1` is the invalid sentinel; do not assume valid IDs are strictly positive.

Known lifecycle risk retained for later: `DiscardPileCleaner` can destroy entities in discard piles and clear `CardIds`. It was not observed firing during the tested normal sequence, so it remains a documented risk rather than a current production suppression.

## Phase 4 — complete for tested scope (2026-10-11)

Persistent off-board card identity, deck/discard gameplay modifications, discard-origin marker, discard-to-hand and discard-to-lane returns, `ReturnedFromDiscardPileTrigger`, `DiscardOutsidePlayTrigger`, and `DiscardedFromQuery` (numeric Source) have been tested successfully in the exercised cases. Selective stat synchronization now distinguishes off-board mutations from later hand/lane stock animations. `DiscardedFromQuery.Source` values: `1` Hand; `2` Deck. Strings are **not** supported in the verified baseline.

Tests do not establish full coverage of every duration/expiry, complex multi-listener queue or repeat-return case. Do not describe untested edges as verified.

## Deferred issues / regression list

1. **Targeting UI**: manual gameplay targeting and legal-target indication work, but targeting text sometimes disappears and hand cards fail to dim during Deck/Discard manual targeting. Presentation-only issue, unresolved.
2. **Attack comparison timing**: a discard-related card's `AttackComparisonQuery >= 2` return ability often resolved only on reaching attack 3, occasionally at 2. `BuffTrigger` is a workaround, not a verified engine repair. Unresolved.
3. **DiscardedFromQuery string serialization**: `"Source": "Hand"` / `"Deck"` produced loading stalls; numeric `1`/`2` worked after `DeepCopyFrom` dispatch repair. Revert experimental named-source hooks; defer strings.

## Phase 5 handoff

Phase 4 is closed for scope management. Phase 5 is the next work stage; **its exact feature scope is not defined by this document**. Preserve the Phase 4 working baseline and read `PHASE4_OFFBOARD_ANIMATION_HANDOFF.md` before new effects or animation hooks.
