# PvZ Heroes — Trick and off-board trigger presentation research

**Research date:** 2026-10-09/10  
**Evidence:** user-provided `dump.cs`, `ZoneTriggeredAbilityPresentationSystem(20261010-030246).hpp`, `Pasted text(20261010-030904).txt`.  
**Scope:** IL2CPP symbols, declared signatures/RVAs and runtime observations. **This is not a decompiled native control-flow proof**. RVAs are specific to the supplied game binary/dump; validate build identity before reusing.

## Key finding

**Zone membership at presentation time is insufficient to classify an off-board triggered ability.** A normally played Trick may be in the discard pile by the time its visuals run. The existing header's `IsDeckOrDiscardSource()` checks `GetOffBoardZone(model, sourceId)` first and returns immediately on a match, **before** `TakePendingPresentation()`. This can suppress ordinary Trick animation stages and leave explicit custom-trigger registrations unconsumed.

## Record-processing paths visible in dump.cs

### A. Played one-time effect (Trick-relevant)

| Class / method | RVA | Dump evidence |
|---|---:|---|
| `PvZCards.Game.PlayOneTimeEffectRecordProcessor.ProcessRecord(PlayOneTimeEffectRecord)` | `0x1FA7C04` | Virtual record entry point |
| `...AddRevealCardAnimation(PlayOneTimeEffectRecord)` | `0x1FA7EC0` | Separate reveal-animation creation routine |
| `...AddHeroReactionAnimations(PlayOneTimeEffectRecord)` | `0x1FA7C88` | Hero reaction stage |
| `PvZCards.Engine.Records.PlayOneTimeEffectRecord.get_CardId()` | `0x228895C` | Card entity ID |
| `...get_HandId()` | `0x22889C0` | Hand ID |
| `...get_LocationId()` | `0x2288A24` | Location ID |
| `PvZCards.Game.LocalRevealOneTimeEffectAnimationFactory.CreateAnimation(EffectAnimationFactory)` | `0x1F8A39C` | Local reveal factory |
| `PvZCards.Game.OpponentRevealOneTimeEffectAnimationFactory.CreateAnimation(EffectAnimationFactory)` | `0x1F9C1A0` | Opponent reveal factory |
| `PvZCards.Game.LocalRevealOneTimeEffectAnimation.OnStarted()` | `0x1F8A480` | Local visual lifecycle |
| `...GetViewObjects()` | `0x1F8A8A4` | Local view resolution |
| `...HandleComplete()` | `0x1F8A97C` | Completion callback |
| `PvZCards.Game.PlayCardRecordProcessor.ProcessRecord(PlayCardToLanesRecord)` | `0x1FA0A90` | **Different** lane-play path, not synonymous with Trick use |

The `RevealOneTimeEffectAnimation` base class stores `CardId` (`+0x28`), `HandId` (`+0x2C`), `TargetId` (`+0x30`), plus `Card` (`+0x38`), `Hand` (`+0x40`), and `Target` (`+0x48`) references. The existence of reveal factories and these view fields suggests how Trick-specific presentation obtains its views; **the dump alone does not prove the exact instruction sequence** or that every Trick uses every listed stage.

**Working call-flow hypothesis:** `PlayOneTimeEffectRecord` → `PlayOneTimeEffectRecordProcessor.ProcessRecord` → reveal / hero reaction animations → local/opponent reveal implementation → queue. This needs read-only tracing to establish actual order.

### B. Triggered ability and grouped effect records

| Class / method | RVA | Dump evidence |
|---|---:|---|
| `PvZCards.Game.TriggeredAbilityStartRecordProcessor.ProcessRecord(TriggeredAbilityStartRecord)` | `0x1FCD0C8` | Record entry |
| `...ProcessRecordGroup(List<Record>, int cardEntityId, TriggeredAbilityStartRecord)` | `0x1FCD14C` | Source ID is explicitly passed |
| `...HasAnimatableRecords(IEnumerable<Record>)` | `0x1FCD200` | Group viability |
| `...AddAnimation(int cardEntityId, List<Record>, TriggeredAbilityStartRecord)` | `0x1FCD554` | Enqueue/construction stage candidate |
| `...AddAbilityAnimation(Record, int cardEntityId, List<Record>)` | `0x1FCD7E4` | Returns `TriggeredAbilityEffectAnimation` |
| `PvZCards.Game.AbilityRecordGroupFinder.FindAbilityRecords(int groupId, int sourceId, int startingIndex)` | `0x1ED520C` | Carries both group and source |
| `PvZCards.Game.TriggeredAbilityAnimationFactory.CreateAbilityAnimation(int abilityGroupId, int cardId, List<Record>)` | `0x1FCB630` | Explicit group + card pairing |
| `...CreateOnSelfDeathAbilityAnimation(...)` | `0x1FCB71C` | Special source/death scenario |
| `...AddCommandsForRecords(List<Record>, TimelineCommandEffectAnimation)` | `0x1FCB724` | Builds commands |
| `...CreateDrawCardsAction(SortedTriggeredEffectRecords)` | `0x1FCB97C` | Draw records become action |
| `SortedTriggeredEffectRecords.SortIntoLists(List<Record>)` | `0x1FCB8B8` | Draw/herald/effect record grouping |
| `PvZCards.Game.TriggeredAbilityEffectAnimation.OnStarted()` | `0x1FCC478` | Presentation starts |

`TriggeredAbilityEffectAnimation` fields include `AbilityGroupId` (`+0x98`), `IsTriggeringOnDeathAbility` (`+0x9C`), and `IsTriggeringSelfOnPlayAbility` (`+0x9D`). These are **better diagnostic discriminators than zone alone**, but `AbilityGroupId` is not guaranteed unique across simultaneous events without further verification.

**Working call-flow hypothesis:** trigger record → record-group finder / processor → ability factory with `(abilityGroupId, cardId, records)` → `TriggeredAbilityEffectAnimation` and commands → start. The method names/signatures are confirmed; runtime order needs tracing.

### C. Common timeline and draw visual implementation

| Method | RVA | Notes |
|---|---:|---|
| `TimelineCommandEffectAnimation.OnStarted()` | `0x1FC9C00` | Used by current off-board workaround |
| `...PlayNullTimeline()` | `0x1FC9CFC` | Null-source timeline path |
| `...PlayAnimatedTimeline(CardView)` | `0x1FC9D34` | CardView-backed path |
| `...HandleCommandLoadComplete()` | `0x1FCA260` | Asynchronous callback, already scoped in header |
| `...CheckForCompletion()` | `0x1FC9E88` | Completion lifecycle |
| `TriggeredAbilityAnimationFactory.CreateDrawCardsAction()` | `0x1FCB97C` | Draw action construction |
| `DrawAllCardsEventAction.CreateDrawAnimation()` | `0x1EDDB04` | Draw-specific visual stage |
| `DrawAllCardsEventAction.CreateCommonTargetReactionAnimation()` | `0x1EDDB2C` | **Tiny ARM64 branch thunk; do not inline-hook** |
| `DrawAllCardsEventAction.RetrieveFactoryAndCreateParticle()` | `0x1EDDB30` | Adjacent to preceding thunk; currently conditionally no-oped |
| `VisualEffectEventAction.RetrieveFactoryAndCreateParticle()` | `0x1EDB798` | Base method; conditionally no-oped |
| `VisualEffectEventAction.CreatePersistentVisuals()` | `0x1EDBEC8` | Conditionally no-oped |
| `DrawAllCardAnimationFactory.BuildAnimationFromRecordList()` | `0x1F0E52C` | Hand-draw animation factory |

**Safety:** Earlier builds combining inline hooks at `0x1EDDB2C` and `0x1EDDB30` crashed ordinary stock draw/conjure, possibly through overlapping ARM64 detours. HOOKSAFETY removed draw-common / create-draw / record-factory tracing hooks. Do **not** restore the tiny-thunk hook for diagnostics.

## What the latest on-device log demonstrates

1. The new `PARTICLE: STOCK FORWARD` log appears; replacing the broad AddParticle registration suppression alone **did not** restore Trick visuals.
2. At **23:06:50**, source **167** is identified as `zone=discard`, even though action inventory contains both `eventType=15` and `eventType=5`. Both source particles and both persistent-visual stages are replaced with `NullEffectAnimation`; source classification could be capturing a played Trick, but the log **does not independently identify the played card type**.
3. At **23:07:03.982**, an explicit presentation bridge is **queued** for `presentationSource=19`, `eventCard=19`, `zone=discard`.
4. At **23:07:08.102**, `TriggeredAbilityEffectAnimation.OnStarted` for source **19** takes `direct zone MATCH (bridge not consumed)` and receives draw-source/persistent no-ops. The custom bridge remains unconsumed in this path.
5. Queue progress is visible and the user reports no crash; this is a stable behavior checkpoint, not proof all visual stages are correct.

## Proposed bridge architecture (design, not implemented)

- **Authorization:** Only an explicit custom off-board trigger registration should authorize source-visual no-ops. Merely detecting `DeckElement` or `DiscardPileElement` is not authorization.
- **Correlation:** Capture `(EntityModel, sourceCardEntityId, abilityGroupId, record group / activation sequence)` when the custom trigger produces its effect, and join with the record-processor/factory animation (not simply the next animation with matching source). If the producer cannot supply group or record identifier, derive candidate association by tracing before mutating.
- **Consume once:** At matched `TriggeredAbilityEffectAnimation.OnStarted`, consume the explicit entry. Expire unmatched entries on a defined lifecycle/record boundary to prevent stale scopes.
- **Scope:** Only for authorized off-board triggers, use the existing source-specific `GetViewObject` null bypass, draw/base source particle no-ops and persistent no-op. All ordinary Trick animation paths forward untouched.
- **Presentation semantics:** Avoid artificially presenting the discarded card as if newly played. Preserve actual target and hand draw animations when independent of a source CardView.
- **Risk:** Some legitimate off-board triggers currently work because of direct-zone fallback; removing it before bridge correlation is demonstrated could reinstate the original null-view hang.

## Recommended diagnostic experiment (read-only)

**Phase 1: Verify Trick path.** Trace `PlayOneTimeEffectRecordProcessor.ProcessRecord` with `CardId`, `HandId`, `LocationId` using the dump-identified getters. Trace local/opponent reveal `OnStarted` and completion only if method bodies have adequate hookable size and no adjacent overlap. Record timestamps and source IDs on a stock Trick play.

**Phase 2: Verify triggered-group path.** Trace `TriggeredAbilityStartRecordProcessor.ProcessRecordGroup` `cardEntityId`, record pointer/list count; `TriggeredAbilityAnimationFactory.CreateAbilityAnimation` `(abilityGroupId, cardId, records)`; resulting animation's `AbilityGroupId` and `SourceCardId` at `OnStarted`. Keep existing presentation behavior while collecting this data.

**Phase 3: Verify bridge ownership.** Log the producer's registration reason/trigger identity and match against the actual factory creation / presentation. Test (a) stock Trick, (b) stock on-board triggered ability, (c) custom discard trigger, (d) deck trigger, (e) two triggers for the same source in one resolution.

**Phase 4: BRIDGEONLY experiment.** Only after phase 3 establishes a reliable correlation, remove the direct-zone early return and authorize null-source presentation through consumed bridge records. Verify stock Trick animations, off-board effects, queue completion, and stock draw/conjure. Keep PARTICLEFORWARD / HOOKSAFETY rollback builds.

## Open questions

- Does every played Trick emit a `PlayOneTimeEffectRecord`, and at what moment relative to discard zone transition?
- Are normal Trick visual stages produced by `LocalRevealOneTimeEffectAnimation` / `OpponentRevealOneTimeEffectAnimation` for the specific card and situation tested?
- How are record group IDs assigned, and do they match `TriggeredAbilityEffectAnimation.AbilityGroupId` in all cases?
- Can a Trick itself produce triggered-ability groups with the same source ID? (Likely, but must be observed.)
- Does the source-only `NullEffectAnimation` substitution suppress anything that should remain visible even after authorization is correct?
- Which record marks the reliable end of an activation for stale-bridge cleanup?

## Evidence boundaries

All method names, parameter lists, offsets, and RVAs in tables come from the provided `dump.cs`; the direct-zone and scope logic comes from the provided header; the runtime timestamps and outcomes are from the supplied log and user test report. Inferred call arrows and architectural recommendations are deliberately marked as hypotheses. No device-side test or native binary disassembly has been performed for this document.
