# Phase 4 — Off-board Animation, Discard and Query Handoff

**Project:** Sprig / Bloom CustomComponents, PvZ Heroes Android IL2CPP  
**Status:** Phase 4 COMPLETE for the tested feature scope; Phase 5 next.  
**Evidence boundary:** device logs, test-card scenarios, image-based stat confirmation, `dump.cs` and inspected ARM64 RVAs. This is a handoff, not a claim every card/trigger/expiry has been validated.

## 1. What actually worked

- Persistent materialized deck entities and discard `CardIds` preserve *live `Entity.Id`* through supported zone moves; don't recreate a replacement card from a GUID when returning a modified card.
- Deck/discard gameplay modifications can change real attack, maximum health, sun cost and grantable abilities while no frontend card view exists.
- Deck/discard to hand and discard to lane were exercised. The Puff-Shroom test showed displayed attack 3, health 6 and cost 1 after a return; stock gameplay also operated with the returned card. A successful visual screenshot does not verify every possible stat animation edge.
- `Discarded` origin distinguishes **Hand=1** from **Deck=2**; cards destroyed during play are not to be marked `Discarded` merely for entering the pile.
- `ReturnedFromDiscardPileTrigger` and `DiscardOutsidePlayTrigger` integrate with the custom trigger dispatcher. `DiscardedFromQuery` successfully selected Deck (`Source=2`) and rejected Hand (`Source=1`) when the actual marker was Deck.
- Numeric `DiscardedFromQuery.Source` is the supported format. Named string values are intentionally deferred.

## 2. The two timelines must not be conflated

```text
GAMEPLAY (authoritative)                         PRESENTATION (async/queued)
Effect created -> ProcessEffect -> entity mutated   record/animation -> source/target views -> visual update
   | off-board: no CardView may exist                  | a CardView appears later in Hand/Lane
   + capture mutation at effect execution             + apply only off-board deltas, leave visible effects to stock
```

The same card can be in discard when a trigger is queued, in hand when the trigger changes it, and have the stock buff animation run later still. **Never classify a buff by the zone at animation time alone.** Use the target card's *real zone at the stock handler's entry*, and correlate source presentation only with authorized custom trigger/event context.

### Example: Puff-Shroom

The read-only tracker observed two discard-zone BuffSystem events, attack `1 -> 2 -> 3`, with no off-board health/cost deltas. At hand entry the old full reconciliation saw attack gap +2, health gap +5, cost gap +1. That prematurely applied effects which would receive normal stock animations. Phase 2B selective sync applied **attack +2, health +0, cost +0** and stock subsequently brought the visible health/cost to the displayed **3/6, cost 1**. Do not revert to `gameplayValue - frameValue` for every property.

### Selective adjustment contract

1. Track `(EntityModel*, cardEntityId)` and mutation epoch; do not key by Guid, view address alone or `SourceId`.
2. At each owned stock ProcessEffect handler, capture before/after effective stat while target is confirmed in Deck/Discard, including real `OrderedCardPool` membership (not just `DeckElement`).
3. Accumulate signed attack/max-health/sun-cost delta. Skip/flag invalid or unknown values; beware duration removal, pooling, repeated returns and lifetime of the tracker table.
4. On hand `ShowInHand` or lane `CreateCardAnimation.ShowCard`, adjust only pending off-board deltas, bounded by the actual frame-vs-model gap. Never replay the same epoch for the same view.
5. Let later hand/lane buffs, cost changes and their normal visuals run through stock. Avoid double applying a queued buff with both manual sync and `BuffCardEffectAnimation`.
6. Trait rebuilding was preserved during the tested Phase 2B; exact ability identity, counter values, expiration and visual sequencing are not fully captured by a component-type hash. Duration tests remain unverified.

## 3. Why animations hung and what not to do

**Invisible source lookups:** `TriggeredAbilityEffectAnimation`, `TimelineCommandEffectAnimation`, source particles and `GetViewObject` can expect a visual source card that is absent in Deck/Discard. Scope a null-timeline/source bypass to the exact explicitly triggered off-board activation. Thread-local scope and source-to-event-card bridge matter. Do **not** set arbitrary `SourceId` or make broad global `GetViewObject` return null.

**Trick false positives:** a normally played Trick may be in discard by the time its presentation runs; its current zone alone is insufficient to determine that its stock play animation should be suppressed. Follow the existing **BRIDGEONLY** authorization policy; preserve ordinary played Trick, reveal, stock draw, source particle, and target animations. `docs/PvZH_Trick_and_Offboard_Presentation_RVA_Research.md` contains the earlier diagnostic hypotheses, some deliberately superseded by later tests.

**Target actions:** an off-board source does not imply all targets are off-board. Stock hand/lane target animation should survive; only verified off-board-only target reactions are candidates for suppression. Apply target policy per target and effect, not blanket source policy.

**View creation:** a model mutation does not automatically refresh a card frame. The correct visible-entry lifecycle differs: hand `ShowInHand`; lane `CreateCardAnimation.ShowCard`. Manually calling gameplay handlers to produce visuals changes semantics. Stock `BuffCardEffectAnimation`, `ModifySunCostEventAction` and `GrantAbilityEventAction` may run after view entry.

**Don't fake UI:** the stock discard viewer uses stock inspection overlay signals and managed data/config objects. Creating arbitrary Unity GameObjects/Canvases is not needed and risks lifecycle issues.

## 4. Effect-queue timing and marker lifecycle

The inspected ARM64 code and trace showed `EffectQueue.Shift()` removes the effect **during** `EffectCollectionProcessor.CompleteEffect()`, not before entry. Exact-end matching therefore snapshots a shift serial before calling original completion, then checks the serial increment and exact completed `TriggeredAbilityEndEffect` afterward.

- Keep `Discarded` available through the direct `ReturnedFromDiscardPileTrigger` listener chain so queries and effects can read its provenance.
- Do not remove the marker merely when the first trigger is *fired*, or when the card becomes visible.
- A shared queue can contain unrelated effects; exact listener completion can permit cleanup with other effects pending. Tested simple Start -> Buff -> End chain reached `EXACT-END-SHIFT`, `EXACT-END-COMPLETE`, then `RETURN-CHAIN-DRAINED` and `Discarded: REMOVE remaining=0`.
- A complex listener (e.g. `ModifySunCostEffect` in the test) was ineligible for the simple-chain proof; conservative fallback remained. Do not claim cleanup is universally exact for complex or multi-listener chains.
- Preserve effect queue source/effect entity IDs, response queues and group lineage. Queue depth alone is not an ownership signal.

## 5. Hook ownership and crash avoidance

Initial audit tried to independently hook `BuffSystem.ProcessEffect` and `GrantAbilitySystem.ProcessEffect`, conflicting with existing owners and producing `GrantAbility initialization failed`. The working design routes read-only tracking through existing owner hooks:

| Existing hook owner | Stock processing | Rule |
|---|---|---|
| `DiscardedSystem` | `BuffSystem.ProcessEffect` | Observe around original handler, do not install another hook |
| `GrantAbilitySystem` | grant/remove ability processors | Share tracker callbacks, preserve custom ability init |
| `ZoneTriggeredAbilityPresentationSystem` | unowned cost/remove processors, visual entry and target/source boundaries | Observe before/after, never mutate gameplay via audit |

Always compare expected RVA against the actual installed build, check `BasicHook` success and original pointers, and validate hook install ordering in device logs. A C++ syntax pass is not proof an Android inline hook is compatible.

### Useful RVAs from the inspected build

| Method | RVA |
|---|---|
| `BuffSystem.ProcessEffect` | `0x21FD444` |
| `ModifySunCostSystem.ProcessEffect` | `0x21F10DC` |
| `GrantAbilitySystem.ProcessEffect` | `0x21F45EC` |
| `RemoveGrantAbilitySystem.ProcessEffect` | `0x21F1C58` |
| `RemoveBuffSystem.ProcessEffect` | `0x21FD258` |
| `RemoveModifySunCostSystem.ProcessEffect` | `0x21F1EA8` |
| `CompositeQueryDeserializer.Deserialize` | `0x222292C` |
| `ReferenceTypeDeserializer.Deserialize(object)` | `0x2221EE8` |
| `ReferenceTypeDeserializer.Deserialize(object,object)` | `0x2222200` |
| `ReferenceTypeDeserializer.SetObjectProperties` | `0x2222008` |
| `ReferenceTypeDeserializer.ConvertPropertiesToDictionary` | `0x2221F50` |
| `InHandQuery.DeepCopyFrom` | `0x228E930` |
| `CompositeQuery.DeepCopyFrom` | `0x228E8B4` |
| `EffectQueue.Shift` | `0x227FBDC` |

RVA values are **build-specific**; verify against the current APK's `libil2cpp.so` and `dump.cs` after any game update.

## 6. Query serializer and DeepCopyable lessons

`DiscardedFromQuery` inherits `InHandQuery` to reuse stock lifecycle. Creating a BNM custom `Matches` and a custom `DeepCopyFrom` by name is insufficient if the class still routes interface/concrete virtual slots to inherited implementations. The observed nested JSON query objects had correct deserialized `Source` (`2` / `1`), while runtime clones carried `0`; after patching both concrete and `DeepCopyable` interface `DeepCopyFrom` dispatch (slot **6** in the inspected build), the runtime objects retained values. The Deck query matched and the Hand query rejected on a Deck-discarded card; two return listeners were selected as expected.

The native query's `Source` must not be confused with the marker's `Source`. Query checks provenance, not current discard-zone membership. Compose with `InDiscardPileQuery` when current location matters.

**Safe verified JSON:** `"Source": 1` (Hand), `"Source": 2` (Deck).

**Unsafe experimental JSON:** `"Source": "Hand"`, `"Source": "Deck"`. Registering names against global `System.Int32` affects every integer field; later scoped preconversion experiments hung loading, and one failed run logged `GrantAbility initialization failed`. Do not ship the experimental preconversion hook; revert to the known-good numeric field bridge. String support should be revisited only after tracing and safely validating a strictly field-scoped pre-stock conversion.

## 7. Bugs deferred to later — unresolved

1. **Manual targeting UI:** legal targets and gameplay selection still work, including highlighting. Sometimes the targeting instruction text disappears and hand cards are not greyed out during deck/discard interactions. Investigate UI state initialization, refresh and teardown; don't change target eligibility without proof.
2. **Attack comparison timing:** a `DiscardFromPlayTrigger` buff and a separate `AttackComparisonQuery >= 2` return trigger can resolve inconsistently (often apparent return at 3, sometimes at actual 2). Evaluation vs buff ordering is a plausible explanation, not proven. `BuffTrigger` is a workaround; this is not fixed.
3. **Named DiscardedFromQuery.Source strings:** loading/initialization failure with string values. Keep numeric format. This is not fixed.

## 8. Phase 5 and release rules

**Phase 4 complete for tested cases; Phase 5 can start.** The Phase 5 feature list must be specified separately; do not infer all formerly planned Phase 5/6 items are absent or already done merely because this phase was relabeled.

Before future changes: preserve a known-good numeric-query build, test draw/hand, discard/hand, discard/lane, visual buffs, targeting UI and query clones after any hook edit. Leave verbose trace-hook removal and broad production logging cleanup for actual release preparation, as requested. Prefer one targeted change and one diagnostic per build over stacking speculative patches.
