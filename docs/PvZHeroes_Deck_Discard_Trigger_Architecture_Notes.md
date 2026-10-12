# PvZ Heroes CustomComponents — Deck / Discard / Trigger Architecture

Reverse-engineering notes, confirmed contracts, design decisions, and implementation roadmap.

*Project: Sprig + Bloom CustomComponents | Working state: Phases 0–4 complete for tested scope; Phase 5 next. Historical planned sections retained.*

# 1. Purpose and status legend

This document preserves the conclusions reached while reverse-engineering PvZ Heroes' card-pool, draw, discard, query, trigger, targeting, and presentation systems. It is intentionally split between confirmed native behavior and planned custom behavior so later work does not accidentally treat a hypothesis as an engine fact.

| Label | Meaning |
| --- | --- |
| CONFIRMED | Observed in dump.cs, native ARM64 disassembly, or runtime logs. |
| DECISION | A deliberate mod architecture choice. |
| PLANNED | Intended custom behavior; not implemented/validated yet. |
| OPEN | Still requires native inspection or runtime validation. |

# 2. Core discovery: OrderedCardPool has two legal card states

CONFIRMED: CardPoolEntry contains both a card Guid and a runtime EntityId:

```text
CardPoolEntry
{
    int Guid;
    int EntityId;
}
```

The engine intentionally supports both of these representations:

```text
Virtual / Guid-only entry:
    Guid = N
    EntityId = -1
```

```text
Materialized / entity-backed entry:
    Guid = N
    EntityId = <real Entity.Id>
```

Runtime pool dumps from the current game showed the starting OrderedCardPool entries in the virtual state: valid Guids with EntityId=-1. This was the key reason the earlier InDeckQuery and DiscardFromDeck translation attempts could not find real deck entities.

# 3. PooledCardRetriever contract

CONFIRMED: PooledCardRetriever::GetNextCard(OrderedCardPool) reads the first CardPoolEntry. If EntityId is already valid, it returns Model.GetEntity(EntityId). If EntityId is -1, it materializes that entry.

```text
GetNextCard(OrderedCardPool pool)
    next = pool[0]
```

```text
    if next.EntityId != -1:
        return Model.GetEntity(next.EntityId)
```

```text
    return CreateNextCard(next)
```

CONFIRMED: CreateNextCard(CardPoolEntry) creates a real card from nextCard.Guid and then writes the created card's Id back into the SAME CardPoolEntry.EntityId.

```text
CreateNextCard(entry)
    card = CreateNextCard(entry.Guid)
    entry.EntityId = card.Id
    return card
```

Important consequence: once a deck entry is materialized, a later draw returns the same card entity. It is not recreated just because it moves from deck to hand.

# 4. CardEntityFactory and triggered abilities

CONFIRMED: CardEntityFactory::CreateCardEntity(CardData) creates/loads the real card entity and then calls GrantableTriggeredAbilityCreator::CreateAllAbilityEntitiesFor(card) before returning.

```text
CardEntityFactory::CreateCardEntity(...)
    -> instantiate/load card components
    -> create/add card entity
    -> GrantableTriggeredAbilityCreator::CreateAllAbilityEntitiesFor(card)
    -> return full card entity
```

This is crucial for while-in-deck mechanics: materializing a card is enough to create its triggered-ability/effect entities. The card does not need to be drawn first.

CONFIRMED: CreateCardEntityWithDefaultOwner(...) wraps normal creation and then invokes DefaultOwnerAssigner.

# 5. Ownership decision

CONFIRMED: DefaultOwnerAssigner determines the new card's faction, finds the first player entity of that faction through PlayerEntityFinder, and writes the resulting player Id into Owner.OwnerId.

```text
Plant card  -> Plant player  -> Owner.OwnerId = Plant player Id
Zombie card -> Zombie player -> Owner.OwnerId = Zombie player Id
```

DECISION: Preserve the game's expected faction invariant. Plants stay with the Plant player and Zombies stay with the Zombie player. We are not designing cross-faction deck ownership because substantial stock game logic assumes this invariant.

Implementation consequence: deck materialization should use stock CreateCardEntityWithDefaultOwner(Guid), not custom ownership inference.

# 6. Deck initialization and persistent materialization

CONFIRMED: normal starting-deck initialization completes through stock `DeckInitializer.InitializeDeck`. The production materializer runs **after** that stock method returns.

For each main-deck entry whose `EntityId` is not already positive:

```text
card = CardEntityFactory.CreateCardEntityWithDefaultOwner(entry.Guid)
entry.EntityId = card.Id
```

The existing `CardPoolEntry` object and list position are preserved. The materializer does not rebuild or reorder the deck.

CONFIRMED: this successfully materializes both 40-card starting main decks.

CONFIRMED: stock `RemoveExtraCardsInDeckSystem` is the mulligan-end virtualization step. It destroys remaining materialized deck cards and resets their `EntityId` values to `-1`.

DECISION / IMPLEMENTED: suppress that cleanup system so the same card entities survive final mulligan shuffle and normal gameplay.

Verified lifecycle:

```text
InitializeDeck
    ↓
40/40 main-deck entries materialized
    ↓
opening-hand / mulligan draws remove real entity IDs
    ↓
remaining entries stay materialized
    ↓
RemoveExtraCardsInDeckSystem suppressed
    ↓
final shuffle preserves EntityIds
    ↓
persistent entity-backed main deck
```

Superpower pools retain their normal lazy/virtual behavior.

Status: **Phase 0 complete**.


# 7. Draw path and persistence of deck buffs

CONFIRMED: DrawCardSystem obtains a card through PooledCardRetriever::GetNextCard(deck), then removes that real card Id from the deck and adds the same entity to hand.

```text
DrawCardSystem
    -> CardRetriever.GetNextCard(deck)
    -> DeckModifier.RemoveCard(card.Id, deck.Id)
    -> HandModifier.AddCard(card, ...)
```

No CardResetter call was observed in the inspected draw path.

CONCLUSION: if a materialized deck entity receives a one-time stat mutation/buff, drawing it should preserve that mutation because the same entity moves to hand.

Design distinction:

```text
"Cards in your deck GET +1/+1"
    -> one-time mutation; expected to persist after draw
```

```text
"Cards in your deck HAVE +1/+1"
    -> potentially continuous zone-dependent effect;
       may intentionally stop after leaving the deck
```

# 8. DeckModifier / OrderedCardPoolUtils contracts

CONFIRMED: OrderedCardPoolUtils exposes separate operations for real card entities and Guid-only cards.

```text
AddCard(Entity card, ...)
AddCardGuid(int guid, ...)
RemoveCard(int cardId, int deckId)
ShuffleCardPool(int poolEntityId)
```

CONFIRMED: RemoveCard compares the supplied cardId against CardPoolEntry.EntityId. It does NOT remove by Guid.

CONFIRMED: top, bottom, random insertion, and shuffle operate on CardPoolEntry objects, so Guid and EntityId travel together when the list is reordered.

# 9. DiscardFromDeck: exact stock contract and final implementation

CONFIRMED: `DiscardFromDeckSystem` treats `Effect.TargetId` as a real card entity ID. It resolves the target through the model, removes `target.Id` from the deck, and adds the same ID to the discard pile.

Required invariant:

```text
Effect.TargetId
    == live Entity.Id
    == CardPoolEntry.EntityId in Effect.DeckId
```

The earlier Guid-translation implementation existed only because the query was not yet returning actual deck entities.

FINAL IMPLEMENTATION: `InDeckQuery` now returns real materialized deck entities, so `DiscardFromDeck.TargetId` is already correct.

The custom system bridge now only:

```text
1. resolves the target owner's player entity,
2. reads DeckAssociation.DeckEntityId,
3. reads DiscardPileAssociation.DiscardPileId,
4. validates TargetId is present in that deck,
5. fills DeckId / DiscardPileId,
6. delegates to stock DiscardFromDeckSystem.ProcessEffect.
```

The Guid-to-entity translation path and full pool-dump diagnostics have been removed.

Runtime proof:

```text
MATCH TRUE entityId=45
ProcessEffect TargetId=45
RemoveCard cardId=45
AddCard CardId=45
```

The same identity equality was observed again with entity `19`.

Status: **Phase 1 complete**.


# 10. Discard pile representation

CONFIRMED: DiscardPile stores List<int> CardIds, and those values are real entity IDs.

CONFIRMED: AddCard appends to the list, so insertion order gives:

```text
Card A discarded
Card B discarded
Card C discarded
```

```text
CardIds[0] = A   // oldest
CardIds[1] = B
CardIds[2] = C   // newest / top
```

This directly supports the production `InDiscardPileQuery`: live discard membership is exact `Entity.Id` membership in `DiscardPile.CardIds`. Phase 3 later added a separate Guid-based variant for CardDatabase-backed subset queries.

# 11. InDeckQuery

FINAL IMPLEMENTATION: `InDeckQuery` is a real JSON-visible BNM managed class:

```text
PvZCards.Engine.Queries.InDeckQuery : PvZCards.Engine.Queries.InHandQuery
```

`InHandQuery` is used only as a concrete stock lifecycle base because it already satisfies `EntityQuery`, `Poolable`, and `DeepCopyable`.

Membership is real entity-backed deck membership:

```text
candidate Entity.Id
    ↓
EntityQueryContext.Model
    ↓
scan OrderedCardPool components
    ↓
any CardPoolEntry.EntityId == candidate Id
    ↓
true
```

### Constructor requirement

CONFIRMED: stock `InstanceManager` eventually creates pooled objects through an Activator-style factory. The custom runtime type must expose its own public parameterless `.ctor`.

Without it, runtime creation failed with:

```text
MissingMethodException:
Default constructor not found for type PvZCards.Engine.Queries.InDeckQuery
```

The final `.ctor` calls `InHandQuery::.ctor`.

### Interface-dispatch requirement

CONFIRMED: `EntityQuery` is an interface, and `EntityFinder.RunQuery` invokes `Matches` through the interface vtable rather than through a simple method-name lookup.

In the investigated build:

```text
EntityQuery.Matches          interface slot 0
InHandQuery.Matches          concrete slot 4
InHandQuery.Matches RVA      0x228E8C4
EntityFinder.RunQuery RVA    0x22818A4
```

The BNM-created custom `Matches` method existed, but the inherited interface entry still pointed to stock `InHandQuery::Matches`.

`BNM_CustomMethodSkipTypeMatch(Matches)` was not sufficient.

FINAL FIX: initialization explicitly rewrites both:

```text
the inherited concrete Matches vtable slot
the EntityQuery.Matches interface vtable slot
```

to the custom method pointer.

This is required production infrastructure, not a diagnostic hook.

### Rejected approaches

Do not restore:

- registration-only / marked `InHandQuery` backing,
- broad global `InHandQuery::Matches` hooks,
- `EntityFinder.RunQuery` or `FindEntities` BasicHook as the query implementation,
- injection bypass,
- direct Guid-based fake deck targeting.


# 12. Ordered target selection: ListPosition

DECISION: extend TargetScopeSortValue with a custom value named ListPosition rather than DeckPosition, because the concept should work for any ordered card container.

```text
TargetScopeSortValue
    None         = 0
    Attack       = 1
    Health       = 2
    ListPosition = 3   // custom
```

Generic semantics:

```text
Deck:
    lowest ListPosition  = top / next-to-draw
    highest ListPosition = bottom
```

```text
Discard:
    lowest ListPosition  = oldest
    highest ListPosition = newest / top
```

This allows query and sort responsibilities to stay separate:

```text
InDeckQuery + ListPosition + Lowest
    -> first/topmost qualifying deck card
```

```text
InDeckQuery + ListPosition + Highest
    -> last/bottommost qualifying deck card
```

```text
InDiscardPileQuery + ListPosition + Lowest
    -> oldest qualifying discarded card
```

```text
InDiscardPileQuery + ListPosition + Highest
    -> newest/top qualifying discarded card
```

CONFIRMED / IMPLEMENTED: native inspection verified Lowest/Highest comparison behavior and `ListPosition=3` is implemented through `TriggeredEffectTargetScoper.CalculateSortedValueFromEntityId`. JSON enum conversion accepts the custom name through the shared enum serializer.

# 13. Trigger eligibility from deck/discard

CONFIRMED: at least one stock trigger family uses `WillTriggerEffectsQuery`, which contains explicit stock zone gating such as `InLaneQuery`, `InOneTimeEffectZoneQuery`, and `IsActiveQuery`.

IMPORTANT: not every triggered ability family uses the same registration/query path. Some card abilities use source/self filters or other trigger-specific eligibility checks.

DECISION: off-board triggering must be reverse-engineered **per trigger family**. Do not globally assume that extending `WillTriggerEffectsQuery` makes every trigger work from deck/discard.

Planned investigation pattern:

```text
pick one known trigger family
    ↓
identify how its owner/source becomes eligible
    ↓
extend only that family's zone rule if needed
    ↓
verify execution from deck/discard
    ↓
only then inspect presentation behavior
```

Do not implicitly extend death-trigger behavior or unrelated trigger families.


# 14. Off-board triggered ability presentation

Presentation fixes are **conditional**, not a prerequisite for deck/discard triggers.

If a working off-board trigger executes without requesting an invalid source animation, no presentation patch should be added.

Only if a verified deck/discard trigger softlocks or misbehaves because the source card is not visible should we patch the narrow source-presentation path.

Preferred behavior if a fix is required:

```text
source in visible stock zone:
    stock source presentation
```

```text
source in deck/discard:
    execute stock/custom gameplay
    suppress only unsafe source-card presentation
    preserve target/effect visuals where safe
```

Candidate presentation classes previously identified include `SourceEffectTagAnimationFactory`, `EffectTagAnimationFactory`, `VisualEffectEventAction`, and `DamageEventAction`, but no production hook should be selected until an actual working off-board trigger demonstrates the need.


# 15. Current implementation status

| Area | Status |
| --- | --- |
| Phase 0 — persistent entity-backed main decks | **COMPLETE** |
| Phase 1 — real `InDeckQuery` | **COMPLETE** |
| Phase 1 — stock-compatible `DiscardFromDeck` | **COMPLETE** |
| Phase 1 production logging cleanup | **COMPLETE** |
| Phase 2 — `ListPosition` targeting | **COMPLETE** |
| Phase 3 — discard membership + Guid query variants + viewer | **COMPLETE** |
| Phase 4 — persistent deck modifications | PLANNED |
| Phase 5 — off-board trigger families | PLANNED |
| Phase 6 — presentation fix | ONLY IF NEEDED |
| Phase 7 — generalized deck/discard mechanics | PLANNED |


# 16. Verified Phase 0 / Phase 1 proof

Phase 0 proof:

```text
main deck 1: 40/40 materialized
main deck 106: 40/40 materialized
opening hand removes real IDs
mulligan cleanup suppressed
final main decks: materialized=36 virtual=0
```

Phase 1 query proof:

```text
InDeckQuery custom interface dispatch reached
real deck members returned by Entity.Id
```

Phase 1 discard proof:

```text
TargetId == selected deck Entity.Id
TargetId == CardPoolEntry.EntityId
stock RemoveCard uses same ID
DiscardPile stores same ID
```

Two independently observed examples were `45 -> 45` and `19 -> 19`.

Production cleanup subsequently removed these high-volume proof logs while keeping the behavior and error checks.


# 17. Roadmap

## Phase 0 — persistent entity-backed decks — COMPLETE

- materialize main-deck Guid-only entries,
- preserve stock faction ownership,
- suppress mulligan-end virtualization,
- keep canonical `OrderedCardPool` ordering.

## Phase 1 — real deck query + discard — COMPLETE

- real custom `InDeckQuery`,
- working stock pool/constructor lifecycle,
- explicit interface-dispatch repair,
- `Entity.Id == CardPoolEntry.EntityId` membership,
- `DiscardFromDeck.TargetId` already equals real deck entity ID,
- obsolete Guid translation removed,
- production diagnostics cleaned.

## Phase 2 — generic ordered-zone targeting — COMPLETE

Implement custom `TargetScopeSortValue.ListPosition = 3`.

Deck semantics:

```text
Lowest  = top / next-to-draw
Highest = bottom
```

Discard semantics:

```text
Lowest  = oldest
Highest = newest / top
```

## Phase 3 — discard membership and CardDatabase query variants — COMPLETE

Live targeting uses `InDiscardPileQuery` with exact `Entity.Id` membership in `DiscardPile.CardIds`.

Stock `DrawCardFromSubset` evaluates its subset against CardDatabase backing entities rather than live match entities, so Guid membership is a separate domain. Production uses distinct query types:

```text
InDeckQuery              -> live EntityId membership
InDiscardPileQuery       -> live EntityId membership
InDeckGuidQuery          -> Card.Guid membership
InDiscardPileGuidQuery   -> Card.Guid membership
```

`InDiscardPileGuidQuery` resolves each live discarded entity through the match `EntityModel`, reads its `Card.Guid`, and compares that Guid with the CardDatabase candidate.

The attempted serialized `MatchByGuid` field was abandoned because arbitrary custom query `$data` fields are not populated by the current custom query construction path.

## Phase 4 — persistent deck modifications

Use the persistent entity identity from Phase 0 for one-time mutations such as:

```text
All cards in your deck get +1/+1
```

The same entity should retain that mutation when later drawn.

## Phase 5 — off-board triggers

Investigate trigger eligibility per trigger family rather than applying one global query assumption.

## Phase 6 — presentation only if required

Patch source presentation only if a verified off-board trigger requires it.

## Phase 7 — generalized mechanics

Compose queries, sort scopes, and ordinary effects to support deck/discard movement, buffs, destruction, ability grants, and ordered/random targeting.


# 18. Architectural end state

```text
OrderedCardPool (canonical order)
        |
        | every usable deck CardPoolEntry has EntityId
        v
Actual card Entity
(Card / stats / custom components / ability entities)
        |
        +--------------------+---------------------+
        |                    |                     |
        v                    v                     v
   InDeckQuery          Trigger systems       Target filtering
        |                    |                     |
        v                    v                     v
 entity membership      while-in-deck/        Random / ListPosition
                        discard abilities      Lowest / Highest
                             |
                             v
                      presentation bridge
                             |
                   source in deck/discard?
                       /             \
                     yes             no
                      |               |
              suppress source      stock source
                animation           animation
```

# 19. Important invariants to preserve

• Plants remain owned by the Plant player; Zombies remain owned by the Zombie player.

• OrderedCardPool remains the canonical source of deck order.

• Do not manually reorder or rebuild CardPoolEntry objects when materializing.

• Use real EntityIds for stock RemoveCard / DiscardFromDeck contracts.

• Do not reintroduce broad global InHandQuery hooks.

• Do not suppress all ability animations; only suppress unsafe off-board source-card presentation.

• Do not extend death-trigger zone behavior implicitly.

• Prefer stock factories/modifiers and their injected live instances over manually constructing engine utilities.

# 20. Production Phase 0 / Phase 1 files

Core files:

```text
inc/utilities/DeckMaterializer.hpp
inc/queries/InDeckQuery.hpp
inc/systems/DiscardFromDeckSystem.hpp
inc/components/DiscardFromDeckEffectDescriptor.hpp
inc/DiscardFromDeck.hpp
main.cpp
```

Production cleanup removes investigation-only mutation/query/discard tracing while preserving:

```text
DeckMaterializer:
    post-InitializeDeck materialization
    RemoveExtraCardsInDeckSystem suppression

InDeckQuery:
    real managed type
    default constructor
    entity-backed Matches
    explicit interface dispatch repair

DiscardFromDeck:
    descriptor bridge
    target-owner DeckId / DiscardPileId resolution
    target-in-deck validation
    stock ProcessEffect delegation
```

This document should be updated whenever a planned item becomes confirmed or an engine assumption is disproven by logs/native inspection.


## 21. Phase 2 completion update

Phases 0–2 are now complete and supersede the earlier diagnostic-state wording retained above as investigation history.

- Phase 0: main decks remain persistently entity-backed through mulligan/gameplay.
- Phase 1: real `InDeckQuery` membership and stock-compatible `DiscardFromDeck` use actual deck entity IDs.
- Phase 2: generic `ListPosition=3` sorting is implemented through the stock target-scoping pipeline.
- Phase 3: live discard membership, database-facing Guid query variants, and a stock inspection-overlay discard viewer are complete for current needs.

Production enum serialization is shared: one narrow `DefaultDeserializer.GetValue(Type, object)` hook translates registered custom enum names to numeric strings and delegates final conversion/boxing to stock. This covers custom `GrantableAbilityType` names and `TargetScopeSortValue.ListPosition` without competing serializer hooks.

Production logging policy after proof: successful/hot-path diagnostic logging is removed from GrantAbility serialization, TargetScope ListPosition resolution, and Thrash gameplay; error logging remains for actual initialization, lookup, routing, and invariant failures.


## 22. Phase 3 completion update — 2026-10-08

Phase 3 resolved two separate problems that initially looked like one query issue: live zone membership and CardDatabase subset membership.

### 22.1 Live versus CardDatabase query domains

Live target selection receives runtime match entities. Production membership is exact identity:

```text
InDeckQuery:
    candidate.Id == CardPoolEntry.EntityId

InDiscardPileQuery:
    candidate.Id == any DiscardPile.CardIds entry
```

`DrawCardFromSubset` is different. `CardFromSubsetFinder` queries `CardDatabase.FindEntities`, chooses a backing card definition, returns its Guid, and `DrawCardFromSubsetSystem` creates a new card from that Guid. A live-ID query used in this context can false-match through accidental numeric ID collisions.

Production therefore uses explicit database-facing variants:

```text
InDeckGuidQuery:
    candidate Card.Guid == any CardPoolEntry.Guid

InDiscardPileGuidQuery:
    candidate Card.Guid == Card.Guid of any live entity referenced by DiscardPile.CardIds
```

There is intentionally no automatic ID-to-Guid fallback. JSON type identity selects the required domain.

### 22.2 Rejected serialized mode flag

The attempted `MatchByGuid` boolean never became true from query `$data`; constructor and `Matches` diagnostics always observed the default false value. The custom query construction path creates the mapped custom runtime type but does not populate arbitrary custom instance fields from `$data`.

Do not restore the serialized mode-flag design. Use the four explicit query types.

### 22.3 Discard pile persistence caveat

`DiscardPileCleaner.DestroyEntitiesInDiscardPile` is capable of destroying every card entity referenced by a pile and clearing `CardIds`. Runtime tracing did not observe the cleaner in the tested normal sequence.

Current policy:

```text
known capability: documented
normal tested path: discard entities remain live
production suppression: none
revisit condition: an actual lifecycle test shows discarded entities disappearing
```

### 22.4 Stock discard viewer

For a safe viewable zone, reuse the stock card inspection overlay rather than constructing Unity UI objects from native code. The game already supports this overlay during battle.

```text
plain tap on either hero/player
    -> identify tapped player entity
    -> DiscardPileAssociation.DiscardPileId
    -> DiscardPile.CardIds
    -> EntityModel.GetEntityById(cardId)
    -> Card.Guid
    -> stock InspectionItem list
    -> stock CardInspectionOverlayConfig
    -> ShowCardInspectionOverlaySignal
```

The mod creates stock managed data/config objects only. The game's own screen/prefab lifecycle creates and destroys the actual UI. This is the preferred pattern for future zone viewers until a safe custom UGUI lifecycle is understood.

### 22.5 Entity ID zero is valid

Tap diagnostics proved the local player can have `PlayerId=0` and `TappedEntityId=0`. The enemy used a positive entity ID in the same match.

Important invariant:

```text
EntityId 0   = valid
EntityId -1  = invalid/unassigned sentinel in the relevant pool/query paths
```

Do not use `> 0` as a generic entity-validity test. Use the actual sentinel contract for the subsystem being handled.

### 22.6 Current phase status

```text
Phase 0  persistent entity-backed decks                   COMPLETE
Phase 1  live InDeckQuery + stock-compatible discard     COMPLETE
Phase 2  ListPosition ordered-zone targeting              COMPLETE
Phase 3  discard membership + Guid variants + viewer      COMPLETE
Phase 4  persistent deck mutations                        NEXT
```



# 23. 2026-10-11 Phase 4 final status — supersedes earlier roadmap tables

The earlier tables calling Phase 4 persistent deck modifications "planned" describe the original roadmap, not the later implementation. The completed Phase 4 scope now includes custom discard origin tracking, deck/discard effect processing, return-to-hand/lane, custom return triggers, `DiscardedFromQuery` numeric source matching, and off-board stat/trait presentation work. User-tested discard-to-lane and screenshot-confirmed discard-to-hand stat presentation were successful; not all durations or exotic listener chains were verified.

**New invariants:** separate gameplay mutation time from presentation time; track actual target zone, not origin/source zone; never assume a missing `CardView` means an effect failed; keep normal visible hand/lane visuals stock; narrow the null-source presentation exception to an explicitly authorized off-board trigger; do not skip an entire shared effect queue waiting for one listener. Consult `PHASE4_OFFBOARD_ANIMATION_HANDOFF.md` for the detailed timeline, RVAs and deferred bugs.

**Deferred (not fixed):** targeting text/hand-card greying UI, inconsistent attack-threshold trigger ordering, and named string values for `DiscardedFromQuery.Source`. Numeric source remains verified. Do not ship experimental global Int32 aliases or named-source preconversion hooks.
