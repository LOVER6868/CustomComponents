# CustomComponents architecture

CustomComponents separates managed component construction, grantable-ability lifecycle, ability-specific gameplay, and the developing custom-effect layer.

## Core registries

- `CustomComponentRegistry` owns custom managed component identity, serialized type resolution and construction metadata.
- `AbilityRegistry` owns custom `GrantableAbilityType` identity and behavior. An `AbilityDescriptor` contains the numeric ID, serialized name, storage behavior/value accessors, and optional stock `SpecialAbilityType` used for presentation.

These registries are intentionally independent. Armor is a custom grantable ability backed by a stock component; Charge and Thrash are custom grantable abilities backed by custom managed components.

## Existing systems

- `ComponentSupport` resolves and constructs only components present in `CustomComponentRegistry`; every unregistered type remains stock.
- `NumericAbility` implements Add/Set modifiers, grouping and removal without knowing the concrete ability.
- `AbilityDefinitions` is the composition root for ability-specific registrations. Current registrations are Armor=19, Charge=20, Thrash=21.
- `EnumSerializationSupport` owns the single narrow `DefaultDeserializer.GetValue(Type, object)` hook used by custom enum names; registered names are translated to numeric strings and stock performs the final enum conversion/boxing.
- `GrantAbilitySerialization` registers custom `GrantableAbilityType` names with that shared enum layer and preserves `ModifyOperation` metadata.
- `GrantAbilitySystem` owns generic grant/remove/duration/history lifecycle. Ability-specific gameplay does not belong there.
- `GrantAbilityPresentation` maps registered abilities to stock visual ability types when a descriptor requests it.
- `ChargeSystem` owns Charge-specific block-meter semantics.
- `ThrashSystem` owns Thrash-specific overflow damage semantics.

## GrantableAbility state

Charge and Thrash use the stock `GrantableAbility.Counters` lifecycle directly. They do not define parallel `ModifiableInt`, `GrantedOnly`, or custom amount fields.

`GrantableAbilityRuntime` owns the shared AddCounter/RemoveCounter/expiry/IsActive bridge.

- Charge interprets the sum of counter `Value`s as its numeric amount.
- Thrash is a non-numeric flag and uses counter-backed active state only.
- Armor remains on the separate `NumericAbility` path because its gameplay value is a stock `ModifiableInt`.

## Thrash gameplay architecture

Thrash is now a verified example of why source identity and stock effect routing should be preserved.

A Thrash source that overkills a fighter sends only the valid overflow to the hero opposing the **source card**. The damaged fighter's owner does not define the destination. This intentionally supports future self-damage/self-sacrifice card designs: a card may damage or kill an allied fighter and still send Thrash overflow to the source card's opponent.

For Team-Up, all contributors share one overflow pool. Final allocation is deferred until the complete contribution group has resolved, then assigned **back-to-front**. Each positive allocation is emitted as its own routed `DamageEffect` with the actual Thrash card retained as `SourceId`. Zero allocations create no effect at all.

This preserves stock source-sensitive behavior. For example, two Thrash sources can create two separately processed Thrash shots; Bullseye on one source suppresses block-meter interaction only for that source's shot. The normal Team-Up combat hit is left untouched and retains stock one-charge behavior.

See `docs/Thrash.md` for exact semantics and tests.


## Persistent deck architecture

CustomComponents now treats the main deck as an entity-backed ordered zone instead of relying on vanilla Guid-only entries.

### Phase 0 — persistent entity-backed decks

At stock deck initialization, main-deck `CardPoolEntry` values begin as:

```text
(Guid, EntityId = -1)
```

`DeckMaterializer` runs after stock `DeckInitializer.InitializeDeck`, creates each missing card through the stock `CardEntityFactory.CreateCardEntityWithDefaultOwner(Guid)`, and writes the resulting `Entity.Id` back into the existing `CardPoolEntry`.

The existing entry objects and list order are preserved.

Stock `RemoveExtraCardsInDeckSystem` normally destroys those materialized deck entities at mulligan end and resets their `EntityId` values to `-1`. CustomComponents suppresses that cleanup system so the same entities survive the final mulligan shuffle and remain the canonical deck cards during gameplay.

This gives persistent card identity while preserving stock faction ownership and stock deck ordering.

### Phase 1 — real deck query and discard

`PvZCards.Engine.Queries.InDeckQuery` is a genuine BNM-created managed class:

```text
InDeckQuery : InHandQuery
```

The stock `InHandQuery` base is used only for its proven `EntityQuery`, `Poolable`, and `DeepCopyable` lifecycle. `InDeckQuery.Matches(Entity, EntityQueryContext)` implements actual deck membership by testing:

```text
candidate Entity.Id == CardPoolEntry.EntityId
```

across live `OrderedCardPool` components.

A build-specific BNM limitation required an explicit virtual/interface dispatch repair. `EntityFinder.RunQuery` invokes `EntityQuery.Matches` through the interface vtable, and the inherited `InHandQuery::Matches` entry remained in that slot even after the custom method was created. Initialization therefore rewrites both the inherited concrete slot and the `EntityQuery` interface slot to the custom `Matches` method.

`DiscardFromDeck` now receives the real deck card `Entity.Id` directly from targeting. The custom bridge only resolves the target owner's `DeckAssociation` and `DiscardPileAssociation`, validates that `TargetId` is present in that deck, populates `DeckId` / `DiscardPileId`, and delegates to stock `DiscardFromDeckSystem.ProcessEffect`.

There is no longer a Guid-to-entity translation path.

### Phase 2 — generic ordered-zone targeting

`TargetScopeSortValue` is extended by serialization convention with:

```text
None         = 0
Attack       = 1
Health       = 2
ListPosition = 3   // custom
```

Native inspection of `TriggeredEffectTargetScoper.CalculateSortedValueFromEntityId` (RVA `0x2212194`) proved that stock handles Attack=1 and Health=2 and returns 0 for unknown values. `TargetScopeSupport` therefore intercepts only value 3 and delegates every stock value unchanged.

`ListPosition` is the literal zero-based index in the owning ordered zone. Stock helpers prove `OrderedCardPool.Entries[0]` is top/next draw (`AddCardToTop` inserts at 0; `GetNextCard` reads 0), while `DiscardPileModifier.AddCard` appends to `DiscardPile.CardIds`. Therefore one ordering rule works for both zones:

```text
Deck:    Lowest = top / next draw, Highest = bottom
Discard: Lowest = oldest,          Highest = newest / top
```

Stock `StatComparisonEvaluator.CompareValueForSort` continues to own Lowest/Highest behavior; no custom comparator or target-filter pipeline is required. Query selection and ordering remain composable.

The serializer uses the shared `EnumSerializationSupport` hook so readable JSON can use `"TargetScopeSortValue": "ListPosition"`; the hook translates that name to numeric `3` and then delegates final conversion/boxing to stock.

## Developing custom-effect architecture

Engine investigation shows a reusable stock pattern:

```text
card-data EffectDescriptor
        ↓ ToEffect(InstanceManager, effectEntity)
runtime Effect
        ↓
EffectRouter
        ↓ type-based registered EffectSystem/handler(s)
```

Simple stock descriptors such as Damage, Heal, and ChargeBlockMeter acquire their runtime effect with `InstanceManager.Acquire(typeof(RuntimeEffect), true)`, copy their payload, and return it. Runtime effects are deliberately small data/messages; gameplay behavior normally lives in registered effect systems/handlers.

`InstanceManager` lazily creates pools by managed `System.Type`, so a future BNM-created custom runtime effect should not need a hard-coded stock registration table. The planned first proof is a harmless custom descriptor/runtime effect whose handler only logs a copied integer payload. See `docs/Effects.md`.

## Adding another grantable ability

1. Decide numeric vs flag semantics and whether it can reuse stock state.
2. Register its managed component only if a custom managed type is actually needed.
3. Register one `AbilityDescriptor` in `AbilityDefinitions` with the next custom ID.
4. Reuse `GrantableAbility.Counters` for GrantableAbility-derived state unless the engine proves another field is necessary.
5. Put gameplay semantics in an ability-specific system, not the generic GrantAbility pipeline.
6. Set `specialAbilityType` only when the ability should reuse a stock presentation.
7. Document the final architecture and important failed investigation boundaries.


## Phase 4 completion — off-board gameplay vs presentation (2026-10-11)

Phase 4 is marked complete **for the tested deck/discard/return workflows**, not certified for every card, effect, temporary duration, or zone transition. This section supersedes earlier planned-status entries; the earlier research remains historical.

The central rule is that the authoritative `EntityModel` owns gameplay state; Unity card views and effect animations are delayed representations. Cards kept as live entities in `OrderedCardPool` or `DiscardPile.CardIds` can receive real stat/trait changes without having a valid `CardView` at the time of mutation. Do not synthesize a fake card view or change gameplay `SourceId` to force visuals.

`ZoneTriggeredAbilityPresentationSystem` uses narrow source-card presentation guards plus hand/lane entry reconciliation. `OffboardDeltaEntry` accumulates effective attack/health/sun-cost changes observed **at the moment stock effect processors execute** on a Deck/Discard target. At card-entry presentation (`ShowInHand` / lane `ShowCard`) only recorded off-board adjustments are eligible for manual frame correction. Changes applied after entry remain the responsibility of stock animations. Rebuilding every current stat from the model at entry previously pre-applied return-trigger buffs and caused visual duplication/out-of-order animation.

Do not stack BNM hooks on a single stock method from multiple headers. `DiscardedSystem` owns `BuffSystem.ProcessEffect`; `GrantAbilitySystem` owns grant/removal hooks. Their handlers provide shared read-only observations to the zone presentation tracker. Other unowned cost/remove hooks may be independently observed. Preserve the existing exact-end-effect completion check and conservative fallback for complex listener chains; `EffectQueue.Shift` happens inside `EffectCollectionProcessor.CompleteEffect`, not before its entry.

`Discarded.Source` is provenance (Hand=1, Deck=2); destroyed-from-play cards should not automatically receive the discard-origin marker. `DiscardedFromQuery` must preserve the custom Source field across runtime cloning by patching both inherited concrete and `DeepCopyable` interface `DeepCopyFrom` dispatch (slot 6 in the inspected build). Numeric Source was confirmed end-to-end; string Source remains deferred because experimental pre-deserialization hooks caused menu-load stalls and an observed `GrantAbility initialization failed` in a failed build. Never register `Hand`/`Deck` globally as aliases for `System.Int32`.

See `docs/PHASE4_OFFBOARD_ANIMATION_HANDOFF.md` for lifecycle diagrams, tested coverage, exact RVA references, known failures, rollback advice and the Phase 5 handoff.
