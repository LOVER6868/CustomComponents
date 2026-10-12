# Engine findings and investigation log

This file records reusable findings that should survive individual component refactors.

## Known-good gameplay hook architecture

Charge proves that ordinary BNM hooks can safely integrate with these boundaries:

```text
PvZCards.Core.Systems.HeroBlockingSystem
    ProcessBlockableDamage(DamageEffect, Entity)

PvZCards.Core.Systems.Utilities.HeroBlockMeterModifier
    GetChargeAmount(Entity)
```

The current Charge system resolves `DamageEffect.SourceId`, `Effect.TargetId`,
`DamageEffect.LaneId`, `DamageEffect.get_DamageAmount()`,
`HeroBlockingSystem.<EntityModel>k__BackingField`, and
`EntityModel.GetEntityById(int)` around this boundary.

This is currently the preferred place to investigate damage information before adding a
new hook elsewhere.

## Thrash damage-hook investigation — 2026-10-05

Several seemingly reasonable inline interception points were tested and caused the game
to hang even when the replacement hook did almost no work.

### Rejected: DamageSystem::DamageTarget

A minimal tracer was tried with the generated native shape:

```text
self
DamageEffect*
Health*
damageAmount
MethodInfo*
```

Both BNM and direct-hook investigation were abandoned after hangs. The issue persisted
even after all DamageEffect/Health dereferencing was removed.

### Rejected: DamageSystem::ProcessEffect

A minimal ENTER → original → EXIT tracer also caused the game to hang. Replacing the BNM
hook with a direct Dobby hook did not solve it.

Known RVA from the investigated game build:

```text
ProcessEffect  0x21FDB9C
```

Treat RVAs as build-specific documentation, not stable API.

### Rejected: FighterDamageEffectCreator::CreateEffectsForAttackingInLane

Exact class identity discovered:

```text
PvZCards.Core.Systems.Utilities.FighterDamageEffectCreator
```

The public overload investigated was:

```text
CreateEffectsForAttackingInLane(int sourceCardId, int laneId, int damage)
RVA 0x220EADC
```

Its primitive/reference ABI looked attractive for tracing, but a direct Dobby hook still
produced the same hang before useful runtime trace output. The hook was removed from the
production Thrash system.

Related methods found during investigation:

```text
FighterDamageEffectCreator
  0x220E394  CreateEffectsForFighter(int)
  0x220E5DC  CreateEffectsForFighterInLane(Entity, int)
  0x220E924  private CreateEffectsForAttackingInLane overload using AttackData
  0x220EADC  public CreateEffectsForAttackingInLane(int, int, int)

CombatEffectCreator
  0x22190C4  CreateEffectsForFighter(int)
  0x22190DC  CreateEffectsForFighterInLane(Entity, int)
  0x22190F4  CreateEffectsForAttackingInLane(int, int, int)
  0x2219114  CreateCombatEffectsForLane(int)
```

Again, these addresses are version-specific.

## Damage data model discovered

The investigated dump showed a useful combat chain:

```text
AttackValueProvider
    ↓ AttackData
LaneCombatData
    ↓ DamageData
DamageListBuilder
    ↓ DamageEffect
```

Important value-type observations:

```text
AttackData
  DamageAmount
  IsOverriden

DamageData
  SourceId
  TargetId
  LaneId
  Attack
```

`LaneCombatData` exposes `ToDamageData()`.

Do not casually inline-hook methods taking these structs by value. The first
`DamageListBuilder::CreateAndAddDamageEffect(DamageData, bool)` tracer was abandoned
because guessing an ARM64 IL2CPP value-type ABI is unnecessary risk.

## Practical hooking rule

For future systems:

1. Reuse an existing proven hook when it exposes enough information.
2. Prefer managed reference/pointer and primitive arguments.
3. Verify exact namespace and generated signature before hooking.
4. Avoid guessing value-type ABI.
5. Make experimental tracers non-fatal when possible.
6. If merely installing an inline hook causes a hang before the replacement logs, stop
   iterating on that boundary and document it here.
7. Keep build-specific RVAs as investigation notes only; do not make architecture depend
   on them unless absolutely necessary.

## Current Thrash investigation boundary

The next safe experiment is the already-working Charge
`HeroBlockingSystem::ProcessBlockableDamage` boundary. A concise damage trace can report:

```text
source
target
lane
damage
targetEntity
```

The purpose is to determine whether this stable boundary sees fighter-target damage or
only hero/blockable damage. That result determines whether Thrash can share the Charge
pipeline or needs a different *proven* sibling system.

---

## Thrash final architecture — verified after initial investigation

The earlier Thrash investigation notes above document useful failed boundaries, but they no longer describe the implementation status.

Final verified lessons:

1. Thrash targets fighters, but its destination hero is resolved relative to the **source card**, not the victim.
2. Synthetic hero damage must preserve the actual Thrash card as `Effect.SourceId`.
3. Calling a low-level health/damage endpoint directly can bypass block-meter and other stock damage processing.
4. Creating a real pooled `DamageEffect` and routing it through `EffectRouter` restores stock source-sensitive processing.
5. Team-Up must use one shared overflow pool rather than independent per-source overflow.
6. Team-Up allocation is deferred until the complete contribution group is known and assigned back-to-front.
7. A zero allocation must create no effect object and make no router call; zero-damage effects can still have processing side effects.
8. Separate positive Thrash shots should remain separate effects so Bullseye/block behavior can differ by source.
9. The original Team-Up combat hit is not rewritten; its stock one-charge block-meter behavior remains intact.
10. Source-relative opponent resolution intentionally supports friendly-fire/self-sacrifice card combinations.

See `Thrash.md` for exact examples.

## Effect architecture investigation — 2026-10-06

Stock effects are split into descriptor, runtime message, router, and behavior system:

```text
EffectDescriptor → ToEffect → runtime Effect → EffectRouter → EffectSystem handler(s)
```

Simple runtime effects are mostly data and commonly derive from `NonBlockingEffect`.
`EffectSystem` registers handlers with `EffectRouter`, and the router supports type-based registration including `RegisterForEffect(Type, Action<Effect>)`.

Native inspection confirmed that simple descriptors such as Damage, Heal, and ChargeBlockMeter use `InstanceManager.Acquire(runtimeType, true)`, copy their payload, and return the runtime effect.

`InstanceManager` maintains pools keyed by managed `System.Type` and lazily creates a pool for a previously unseen type. This strongly supports testing a BNM-created runtime effect derived from `NonBlockingEffect` rather than trying to patch a stock effect type.

The first custom-effect proof should be harmless: deserialize a custom descriptor, acquire a custom runtime effect, copy one integer payload, route it, log it in a registered handler, and validate pooling/deep-copy/resolve behavior before adding gameplay semantics.

See `Effects.md` for the detailed plan.

## Persistent deck and query findings — 2026-10-07

### OrderedCardPool supports persistent entities

`CardPoolEntry` stores both `Guid` and `EntityId`. The main deck can legally remain entity-backed if its existing entries are preserved and `EntityId` stays positive.

Stock draw behavior already understands this state: when a pool entry has a real `EntityId`, the card retriever resolves and returns that existing entity instead of creating another one.

### Mulligan cleanup is the virtualization boundary

`RemoveExtraCardsInDeckSystem` is the routine that normally destroys materialized cards remaining in the deck at mulligan end and resets their `CardPoolEntry.EntityId` values to `-1`.

Suppressing only this cleanup preserves the materialized main deck through final mulligan shuffle and normal gameplay.

### `DiscardFromDeckSystem` requires a real deck entity ID

Stock `DiscardFromDeckSystem` resolves `Effect.TargetId` as an entity, removes `target.Id` from the specified deck, then adds the same ID to the discard pile.

Therefore the correct contract is:

```text
Effect.TargetId == live Entity.Id == CardPoolEntry.EntityId
```

A Guid is not accepted by `OrderedCardPoolUtils.RemoveCard`.

### `EntityQuery` dispatch is interface-vtable dispatch

`EntityQuery` is an interface. In the investigated build:

```text
EntityQuery.Matches                         interface slot 0
InHandQuery.Matches                        concrete slot 4
InHandQuery.Matches RVA                    0x228E8C4
EntityFinder.RunQuery RVA                  0x22818A4
```

`EntityFinder.RunQuery` resolves the query's `EntityQuery` interface offset and indirect-calls the corresponding vtable entry.

This explains why several earlier hooks/overrides appeared to exist but never saw calls.

### BNM custom-query lifecycle findings

A JSON-visible BNM query type used by the stock `InstanceManager` must expose its own public parameterless `.ctor`.

The stock pool factory reaches `Activator.CreateInstance(Type)`. A BNM class that can be manually allocated with `CreateNewInstance()` can still fail here if the managed type has no default constructor.

For `InDeckQuery`, the final working managed shape is:

```text
InDeckQuery : InHandQuery
```

with:

- an explicit `.ctor` that invokes `InHandQuery::.ctor`,
- a custom `Matches(Entity, EntityQueryContext)`,
- an explicit dispatch repair that redirects both the inherited concrete `Matches` slot and the `EntityQuery.Matches` interface slot.

`BNM_CustomMethodSkipTypeMatch(Matches)` alone was not sufficient in this build; the inherited interface entry still pointed at stock `InHandQuery::Matches`.

### Rejected query approaches

The following approaches are investigation history and should not be restored:

1. registration-only `InDeckQuery` backed by marked `InHandQuery` instances;
2. broad global hooks on `InHandQuery::Matches`;
3. `EntityFinder.RunQuery` / `FindEntities` BasicHook interception as the query implementation;
4. direct `EntityQuery` implementation from `System.Object` before the stock lifecycle was understood;
5. disabling `InstanceManager` injection as a suspected fix;
6. Guid-to-live-entity translation inside `DiscardFromDeck`.

The production design uses a real managed query type, persistent deck entities, and stock-compatible target IDs.

### Diagnostic cleanup rule

Once a native contract is proven, remove high-volume tracing that no longer protects behavior. The Phase 0/1 production pass removed:

- query construction/acquire lifecycle spam,
- per-candidate `MATCH TRUE` logs,
- deck Add/Remove/Shuffle mutation snapshots,
- discard pile AddCard/history trace hooks,
- full OrderedCardPool dump diagnostics.

Keep initialization failures and narrow validation errors.


## Phase 2 ordered target sorting

Confirmed from dump/native inspection:

- `TargetScopeSortValue`: None=0, Attack=1, Health=2; value 3 is unused in the investigated build.
- `TriggeredEffectTargetScoper.CalculateSortedValueFromEntityId` RVA `0x2212194` handles Attack and Health explicitly and falls through to 0 for unknown values.
- `TriggeredEffectTargetScoper.Compare` obtains both sort values through that method and delegates direction to `StatComparisonEvaluator.CompareValueForSort` RVA `0x228FD88`.
- `TargetScopeSortMethod`: Lowest=1, Highest=2; stock comparison is retained.
- `OrderedCardPoolUtils.AddCardToTop` RVA `0x21DD844` inserts at index 0.
- `OrderedCardPoolUtils.AddCardToBottom` RVA `0x21DD8A4` appends.
- `OrderedCardPool.GetNextCard` RVA `0x22A4EAC` reads index 0.
- `DiscardPileModifier.AddCard` RVA `0x2219B4C` appends the entity ID to `DiscardPile.CardIds`.

Therefore `ListPosition=3` can be defined as the literal zero-based index in either supported ordered zone. Lowest naturally means deck top / discard oldest; Highest naturally means deck bottom / discard newest.

Custom enum names use `DefaultDeserializer.GetValue(Type, object)` RVA `0x2220DF0` as the narrow conversion boundary. Translate only registered custom names to numeric strings, then call stock so stock owns final conversion/boxing.


## Phase 3 discard/query-domain findings — 2026-10-08

### Live membership and CardDatabase subset queries are different domains

`InDiscardPileQuery` works naturally for live targeting because `DiscardPile.CardIds` already stores real runtime entity IDs. Its membership rule is exact identity: a candidate matches only when `candidate.Id` appears in a discard pile.

`DrawCardFromSubset`, however, does not pass live match entities to `SubsetQuery`. Native inspection of `CardFromSubsetFinder.FindCard/FindCardsInSubset` showed the stock path creates criteria, obtains an `EntityQueryContext`, then calls `CardDatabase.FindEntities`. The candidates are CardDatabase backing entities. `PickRandomGuid` then returns a Guid, and `DrawCardFromSubsetSystem.CreateCard` creates a new card from that Guid.

This explains the earlier false positives: comparing a CardDatabase backing `Entity.Id` with live `DiscardPile.CardIds` can accidentally succeed when the two unrelated numeric ID spaces collide. No collision means no result.

Production resolution is separate query **types**, not a runtime mode flag:

```text
InDeckQuery              -> live EntityId membership
InDiscardPileQuery       -> live EntityId membership
InDeckGuidQuery          -> Card.Guid membership for CardDatabase candidates
InDiscardPileGuidQuery   -> Card.Guid membership for CardDatabase candidates
```

For discard Guid membership, resolve each live `DiscardPile.CardIds` entry through `EntityModel.GetEntityById`, read its `Card.Guid`, and compare that Guid with the CardDatabase candidate's `Card.Guid`.

### Rejected: serialized `MatchByGuid` mode field

A custom boolean field in query `$data` remained false in both the constructor and `Matches`. The current custom query construction path maps the serialized type to the BNM runtime type and creates the instance, but does not deserialize arbitrary custom fields onto that instance.

Therefore mode selection is encoded in the serialized query type itself (`InDiscardPileQuery` versus `InDiscardPileGuidQuery`, etc.). Do not restore the `MatchByGuid` design.

### DiscardPileCleaner is a known lifecycle risk, not a current blocker

Native inspection proved `DiscardPileCleaner.DestroyEntitiesInDiscardPile` iterates `DiscardPile.CardIds`, destroys each referenced card entity, and clears the list. Trace hooks observed ordinary `DiscardPileModifier.AddCard` / `RemoveCard` activity but did not observe the cleaner firing during the tested normal sequence.

Keep this in mind if discarded entities disappear later, especially around mode transitions or cleanup phases. Do not suppress the cleaner preemptively without a failing lifecycle test.

### Stock discard viewer is viable in battle

The safe UI route is the stock card inspection overlay rather than synthetic UGUI. `CardInspectionOverlayConfig` accepts a list of `InspectionItem`s, and `InspectionItem(int cardId)` accepts a card Guid. The game already opens this overlay from battle through `ShowCardInspectionOverlaySignal`, so screen creation and teardown remain owned by the game's `ScreenLoader`/prefab lifecycle.

The viewer pipeline is:

```text
hero/player tap
 -> resolve tapped player entity
 -> DiscardPileAssociation.DiscardPileId
 -> DiscardPile.CardIds
 -> EntityModel.GetEntityById
 -> Card.Guid
 -> InspectionItem list
 -> CardInspectionOverlayConfig
 -> ShowCardInspectionOverlaySignal
```

No Unity `GameObject` is created by the mod for this viewer.

### EntityId 0 is valid

Tap diagnostics proved the local player can have `PlayerId=0` / `TappedEntityId=0`, while the opponent used a positive ID. The first viewer version rejected `0` with `> 0` checks, which is why enemy viewing worked while local viewing failed.

For player/entity IDs in these paths, treat `-1` as invalid and allow zero. This is now an explicit engine invariant for future utilities.



## Phase 4 off-board presentation and query cloning — 2026-10-11

See `PHASE4_OFFBOARD_ANIMATION_HANDOFF.md` for the tested call ordering and RVA table. Highlights: `EffectQueue.Shift` occurs during `EffectCollectionProcessor.CompleteEffect`; distinguish exact listener completion from whole-queue draining; on-hand-view full-value reconciliation can pre-apply buffs whose stock animations have not yet run; record Deck/Discard deltas while executing stock effects; share hooks instead of fighting for BuffSystem/GrantAbility ownership; BNM inherited methods require concrete **and** interface dispatch validation (especially `DeepCopyFrom`, slot 6 in the inspected query build). Never globally map custom source names onto `System.Int32`.
