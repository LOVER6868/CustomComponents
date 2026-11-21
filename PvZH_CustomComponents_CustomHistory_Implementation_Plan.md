# CustomComponents CustomHistory — Research and Implementation Plan

**Project:** Plants vs. Zombies Heroes Android / Sprig / Bloom / BNM (IL2CPP)  
**Target:** ARM64 Android build represented by the supplied `apk-unpacked.zip`  
**Status:** Research and engineering plan; **no gameplay code changes made for this document**  
**Plan version:** 1.0 — 2026-11-19  
**Primary goal:** Make custom gameplay actions produce **functional, accurately ordered history records** that are consumed by real frontend processors and drive their own presentation, independent of MatchRecorder.

> **Critical principle:** A replay observation is not a gameplay history record. A history record is not automatically a presentation animation. And being accepted by a processor is not the same as its animation finishing. CustomHistory must implement and expose all three concepts without conflating them.

---

## 1. Deliverable, priorities, and meaning of “stock-like”

CustomHistory becomes a permanent subsystem **inside `Bloom/CustomComponents`**. Other Bloom modules, including MatchRecorder, remain optional observers and are not required for gameplay or presentation. Existing genuine stock records continue to be generated and processed by the game. CustomHistory fills the gaps with typed custom records, exact ordering, and processors that can enqueue real `EffectAnimation` instances into the **same** stock animation queue.

### Must-have behavioral contract

1. **Gameplay is authoritative.** A record describing a mutation is committed only after the exact model operation is verified. If an operation partially fails, explicitly represent the failure; do not claim a successful transfer.
2. **One coherent order.** If custom action C occurs after stock record A and before stock record B, C's frontend work must be enqueued **between A and B**, not after the whole stock history batch or after the animation queue empties.
3. **No duplicate mutations.** Presentation processors are read-only with respect to the authoritative simulation. Stock and custom records may be correlated, but a stock-recorded move must not also be applied as an independent custom move.
4. **Actual queue ownership.** Visual effects are real stock `EffectAnimation` objects or proven queue-compatible custom subclasses, with stock `IsCompleted`, `CanYield`, `Cancel`, and completion behavior.
5. **Truthful completion.** Distinguish `RecordCommitted`, `PresentationEnqueued`, `PresentationStarted`, `PresentationCompleted`, `SkippedNoVisual`, `Canceled`, and `Failed`. No silent success on timeout.
6. **Model and match isolation.** Prediction/AI models, both PvP clients, old scenes, and reused pointers must not exchange record work. Avoid managed object references in simulation-side records.
7. **Operational independence.** The v5 `ReplayEventBridge` can remain a diagnostic journal, but nothing in CustomHistory relies on MatchRecorder or its export format.
8. **Stock-first migrations.** Retain valid stock records; introduce custom semantic records only when the game has no accurate equivalent or when there is additional nonduplicating provenance to express.
9. **Coverage is auditable.** Every mutating custom effect must have a documented stock-record path or custom-record commit, plus a presentation processor or explicit no-visual policy.
10. **Fail visibly and safely.** Missing method, failed hook, invalid view, missing animation owner, queue cancellation, record backlog and exhausted storage are not silently treated as completion.

### Non-goals for the first implementation

- Do not assign a new value such as `RecordType = 89` and assume stock binder, prediction, serialization, or animation code will recognize it.
- Do not rewrite the functioning stock history processor, the entire gameplay effect engine, or MatchRecorder in the first phase.
- Do not add an independent global animation queue and call it integrated history.
- Do not require every query-only component to generate a gameplay record; queries have no committed mutation to record.
- Do not claim PvP/network safety from an offline syntax test. It requires controlled on-device and two-client validation.

---

## 2. Exact source and binary provenance

The research was performed against these files already supplied to the project:

| Evidence | Exact source | Use |
|---|---|---|
| `S` | `Bloom(1).zip`, particularly `Bloom/CustomComponents/**` | **Current source baseline**. The 10 source/test files from `PvZH_CustomComponents_FunctionalRecords_v6_Experimental_PasteOver.zip` match the files present in the uploaded Bloom archive byte for byte. |
| `D` | `dump.cs` | Generated C# metadata: namespaces, fields, method signatures, record enums, type indices, and RVAs. |
| `A` | `apk-unpacked.zip → apk-unpacked/lib/arm64-v8a/libil2cpp.so` | Actual AArch64 implementation disassembled at the `dump.cs` RVAs. The ELF is stripped; symbols in the objdump output are not reliable managed-function names. |
| `M` | `apk-unpacked.zip → apk-unpacked/assets/bin/Data/Managed/Metadata/global-metadata.dat` | Metadata paired with the APK's native IL2CPP library. |
| `L` | `logs(20261119-182424).txt`, plus the four recent `.pvzhr` tests | Measured v6 behavior and observed frontend delays. |
| `V6` | `PvZH_CustomComponents_Functional_Record_Audit_v6.md` | Prior migration audit, cross-checked against current headers and inlines. |

**Binary facts:** `libil2cpp.so` is ELF64 AArch64 for Android, Build ID `f523655daa037ba2c70491f5347d56399160b088`, file size 63,289,984 bytes. `global-metadata.dat` is 15,263,644 bytes. The `0x...` addresses below are **RVAs relative to this specific library**, not absolute addresses after ASLR. **Do not hard-code them in production**: resolve with BNM and verify signatures and method ownership.

**Scope of proof:** `D` establishes a symbol exists; `A` establishes instructions present in this binary; `S` establishes what the current mod attempts; `L` establishes observed device behavior. None alone proves a proposed integration is PvP-safe.

---

## 3. What v6 currently does and why the timing is poor

### 3.1 Existing files and responsibilities

| Current file | Confirmed role |
|---|---|
| `inc/engine/CustomRecords.hpp` | Defines `Kind` (`DiscardOutsidePlay`, `DeckReorder`, `ReturnFromDiscard`, `ZoneTransfer`, `Selection`, `StateChange`, `CardCreated`, `Presentation`), `Record`, `Register`, `Commit`, `ProcessReady`, `MarkVisualComplete`, `Snapshot`, `ResetScene`. |
| `inc/engine/CustomRecordHistoryAnchor.hpp` | Reads `History.Records.Count` as an integer watermark. |
| `inc/presentation/StatQueueContext.hpp` | Hooks `HistoryProcessor.ProcessHistory`, calls original, **then** calls `PumpCustomRecords`; captures history/model/queue ownership from some stock processors. |
| `inc/zones/detail/HandToDiscard.inl` | Verifies a hand → discard-pile transfer, sets discarded-origin state, commits a native `DiscardOutsidePlay` record, fires `DiscardOutsidePlayTrigger`. |
| `inc/presentation/DiscardFromHandPresentationSystem.hpp` | Registers record handler but only creates a **ticket**; the hand-specific view/tween processing occurs later via queue-update callbacks. |
| `inc/presentation/DiscardCardAnimation.hpp` | Already creates real stock `TweenCardEffectAnimation` through `EffectAnimationFactory`; currently starts only after a conservative queue-idle gate. |
| `inc/zones/MoveCardInDeckSystem.hpp` | Creates a native `DeckReorder` record after physical pool reordering, while preserving the original card entity. |
| `inc/zones/ReturnDiscardToHandSystem.hpp` and `inc/zones/detail/DiscardToDeck.inl` | Emit provenance records; return-to-hand currently has a synthetic `DrawCardRecord` presentation bridge where enabled; return-to-deck emits a real `CardReturnedToPlayersDeckRecord`. |
| `inc/integration/ReplayEventBridge.hpp` | Separate bounded telemetry journal; **not functional presentation history**. |
| `tests/custom_records_v6_test.cpp` | Host-level checks for serials, cursor anchors, retries and acknowledgment, but cannot exercise real stock queue dispatch. |

### 3.2 Current v6 timeline

```text
SIMULATION THREAD                           FRONTEND / PRESENTATION
───────────────────────────                 ─────────────────────────────────
transfer hand → discard succeeds
commit native record at Count=N
                                            HistoryProcessor.ProcessHistory()
                                            ├─ processes stock records N...
                                            └─ returns
                                            PumpCustomRecords()
                                            └─ marks custom record Processed
                                               and creates discard ticket
                                            EffectAnimationQueue.Update()
                                            └─ ticket waits for:
                                               normal queue empty AND
                                               front queue empty AND
                                               stock history fully caught up
                                            create TweenCardEffectAnimation
                                            wait for tween completion
                                            unmap/destroy view
                                            MarkVisualComplete(serial)
```

This is the fundamental design defect: **a record that belongs before a later stock record is only dispatched after that later stock record has already had a chance to queue visuals.** The idle-queue gate then delays its animation even further.

### 3.3 Confirmed v6 limitations

- `stockHistoryAnchor` is **the count at commit time**, not a globally ordered stock/custom history entry. A numeric watermark can help reconstruct order but is not by itself a complete shared history.
- `ProcessReady` runs after the original `ProcessHistory`; it is **post-batch**, not in-band.
- `Delivery::Processed` means a handler accepted/enqueued work. `ProcessReady` continues processing subsequent custom records without requiring their earlier animations to have completed. The separate `VisualComplete` state has no built-in barrier semantics.
- The `ProcessReady` code treats **zero registered handlers as success**. That is fine for documented no-visual records but dangerous for records that require a processor.
- `Commit` uses one `activeModel` slot and clears the whole record deque when it sees a different model. That is not sufficient for concurrent prediction/live models or both PvP perspectives.
- When the bounded deque reaches 4,096 entries and the oldest record is unprocessed, `Commit` returns `0` **after gameplay may already have mutated**. Recovery must not pretend the action never happened.
- Some commits are guarded by `model == SceneViewContext::ModelIdentity()`. This suppresses prediction noise, but also risks dropping a valid live mutation if the view model is not yet bound or if the authoritative model executes through a different context. Model selection needs an explicit policy.
- The hand-discard failure branch has a legacy direct-queue fallback. It must be isolated as a clearly logged compatibility mode, not silently mixed with the new record path.
- The hand-discard code explicitly notes that a verified transfer can leave a non-atomic partial failure (for example, remove succeeds, pile add does not). The new record layer should expose that failure; it cannot repair an unsafe mutation by inventing a successful record.

### 3.4 Actual device baseline

From `logs(20261119-182424).txt`:

| Event | Time | Meaning |
|---|---|---|
| `HAND_DISCARD` serial 2, card 5, history anchor 247 | 13:00:52.506 | Simulation commit. |
| `HAND_DISCARD` serial 3, card 31, history anchor 247 | 13:00:52.516 | Second simulation commit at the same stock-history boundary. |
| Tickets for both | 13:00:52.604 | Record acceptance/ticket creation after history dispatch. |
| Boundary report | 13:00:52.614 | `normalEmpty=0`, `frontEmpty=1`, `canYield=0`, `queueCaughtUp=0`. |
| Unmap/destroy requested for cards 5 and 31 | 13:01:01.786 | Eventually completed after ~9 seconds from commit. |
| Card 191 committed at 13:01:17.807, cleaned at 13:01:20.279 | — | ~2.5 seconds total. |
| Cards 13 and 101 committed at ~13:01:37.22, cleaned at 13:02:00.72 | — | ~23.5 seconds total. |

**Measurement caution:** commit-to-cleanup includes queue waiting **and** the visual animation; the logs do not isolate the precise animation-start delay in every case. The history queue boundary state and the code nevertheless identify avoidable post-batch/idle gating. The goal is not to force every visual to appear instantly; it is to make the visual occupy the **correct causal position** among earlier and later effects.

---

## 4. `dump.cs`: verified stock architecture and methods

The following are **actual declared methods and fields**, not speculative API names. Indexes and RVAs apply to the uploaded dump/binary pair.

### 4.1 Simulation history and records

| Class / member | Dump evidence | RVA or field offset | Why needed |
|---|---|---|---|
| `PvZCards.Engine.History` | TypeDefIndex `15332` | — | Owns the stock record list. |
| `History.Records : List<Record>` | private field | `0x18` | Current stock ordering/count; use BNM field access, not raw offset in release code. |
| `History.Instances : InstanceManager` | injected backing field | `0x10` | Record instances are acquired using the game's instance machinery. |
| `History.AddRecord(Type recordType) → Record` | instance virtual method | `0x22829BC` | Creates and appends a stock record object. |
| `History.AddRecord(Record record) → void` | instance virtual method | `0x2282A74` | Appends an existing stock record object. |
| `History.GetRecords() → IList<Record>` | instance virtual method | `0x2282B20` | Enumerator used by frontend history processing. |
| `History.RemoveAllRecords()` | instance method | `0x2282B48` | Pool-aware cleanup path. |
| `History.RemoveAllRecordsWithoutReturningToPool()` | instance method | `0x2282CD4` | Different cleanup path; reason for treating lifetime seriously. |
| `PvZCards.Engine.Record` | TypeDefIndex `15340` | — | Base poolable managed record. |
| `Record.RecordType` | public `RecordType` | `0x10` | Concrete enum discriminator. |
| `Record.Data : Dictionary<RecordKey, object>` | private readonly | `0x18` | Stock payload; data is typed by `RecordKey`. |
| `Record.SetValue<T>` / `GetValue<T>` | generic APIs | varies | Stock data contract. |
| `Record.HandleReturnToPool()` | virtual | `0x2283198` | Lifetime reset boundary. |
| `Record.CompareTo(Record)` | virtual | `0x2283428` | Relevant to prediction/history equivalence. |
| `PvZCards.Engine.RecordType` | TypeDefIndex `15338` | values **0–88** | **No declared custom record type**. |
| `PvZCards.Engine.RecordKey` | TypeDefIndex `15339` | values **0–51** | Fixed known keys; don't hijack unused numbers. |
| `PvZCards.Engine.Records.DiscardFromDeckRecord` | TypeDefIndex `15406` | ctor `0x2287348` | Stock type `75`; has `CardId`, `DeckId`, `DiscardPileId`. |
| `PvZCards.Engine.Records.DiscardFromPlayRecord` | TypeDefIndex `15407` | `SetDataFromEffect` `0x2287598` | Stock type `45`; relevant to board removal, not hand discard. |
| `PvZCards.Engine.Records.CardReturnedToPlayersDeckRecord` | TypeDefIndex `15394` | ctor `0x22866B8` | Stock type `74`, with `CardId`, `DeckId`. |
| `PvZCards.Engine.Records.DrawCardRecord` | TypeDefIndex `15410` | ctor `0x22879C4` | Stock type `42`; has `HandId`, `DeckId`, `Card`. A discard-to-hand return is **not** a true draw from deck. |

**Important semantic distinction:** `RecordType.DiscardFromPlay = 45` and `RecordType.DiscardFromDeck = 75` are not general-purpose `DiscardOutsidePlay` records. The dump contains no corresponding stock class for hand-to-discard removal.

### 4.2 Frontend history dispatcher

| Class / method | Dump evidence | RVA / field offset | Role |
|---|---|---|---|
| `PvZCards.Game.HistoryProcessor` | TypeDefIndex `5651` | — | Frontend history consumer. |
| `HistoryProcessor.<Model>k__BackingField` | `HistoryProcessorModel` | `0x10` | Cursor/process gating. |
| `HistoryProcessor.<History>k__BackingField` | `History` | `0x18` | Stock list for this processor. |
| `HistoryProcessor.<SignalRecordExecutor>k__BackingField` | `RecordExecutor` | `0x20` | Actual record dispatch interface. |
| `HistoryProcessor.<PredictionHistoryModel>k__BackingField` | `GamePredictionHistoryModel` | `0x38` | Predicted-vs-authoritative handling. |
| `HistoryProcessor.RegisterForModelUpdates()` | method | `0x1F70938` | Binds to model resume updates. |
| `HistoryProcessor.ProcessHistory()` | method | `0x1F709E8` | Entry for stock history processing. |
| `HistoryProcessor.ProcessRecords(IList<Record>)` | private | `0x1F70A30` | **Iterates** stock records, checking prediction, processed state, and `CanRun`. |
| `HistoryProcessor.GetNextRecord(IList<Record>) → Record` | private | `0x1F70B58` | Gets the **next stock entry** using the current cursor, before prediction skip decisions. |
| `HistoryProcessor.TryProcessPredictedRecord(Record)` | private | `0x1F70C20` | Checks/marks predicted records. |
| `HistoryProcessor.ProcessRecord(Record)` | private | `0x1F70CF8` | Executes through `RecordExecutor`, then marks processed. |
| `HistoryProcessorModel` | TypeDefIndex `5653` | — | Mutable presentation progress state. |
| `HistoryProcessorModel.CurrentPositionInHistory` | public int | `0x18` | Next stock index. |
| `HistoryProcessorModel.CurrentTurnInHistory` | public int | `0x1C` | History-side turn. |
| `HistoryProcessorModel.ProcessedRecords` | `HashSet<Record>` | `0x20` | Idempotent stock processing. |
| `HistoryProcessorModel.CanRun` | private bool | `0x28` | Processing pause/resume gate. |
| `HistoryProcessorModel.IsRecordProcessed` | method | `0x1F70828` | Used to skip already-seen records. |
| `HistoryProcessorModel.MarkRecordAsProcessed` | method | `0x1F70DB0` | Mark after frontend dispatch. |
| `HistoryProcessorModel.Pause` / `Resume` / `CanProcess` | methods | `0x1F77418` / `0x1F77420` / `0x1F77474` | CustomHistory must respect processing gates. |
| `HistoryProcessorBinder.AddRecordProcessorBindings` | static | `0x1F71A34` | Registers stock processor bindings; adding a subclass is not self-registration. |
| `HistoryProcessorBinder.AddExecutorBindings` | static | `0x1F7101C` | Registers executable mappings. |
| `RecordExecutor.Execute(Record)` | interface | — | Exact processor dispatch dependency. |
| `RecordExecutableFactory.ExecutableMap` | `Dictionary<Type, Executable>` | `0x10` | Type-directed frontend executables. |
| `RecordProcessor.ProcessRecord` / `ProcessesType` | interface | — | Processor semantics; not a universal automatic registry. |

### 4.3 Presentation, animation, dependencies

| Class / member | Dump evidence | RVA / field | Relevance |
|---|---|---|---|
| `PvZCards.Game.EffectAnimationQueue` | TypeDefIndex `5350` | — | **Use this queue**, not a detached custom queue. |
| `EffectAnimationQueue.Animations` | `List<EffectAnimation>` | `0x10` | Normal ordered animation queue. |
| `EffectAnimationQueue.FrontAnimations` | `List<EffectAnimation>` | `0x18` | Independent front-priority path. |
| `EffectAnimationQueue.AddAnimation` | virtual | `0x1F138C4` | Normal insertion. |
| `EffectAnimationQueue.AddAnimationToFrontOfQueue` | virtual | `0x1F13970` | **Not** a safe generic way to fix ordering: front work can overtake stock animations. |
| `EffectAnimationQueue.Update` | method | `0x1F13A48` | Starts, updates and retires animations. |
| `EffectAnimationQueue.IsEmpty` / `CanYield` | methods | `0x1F13710` / `0x1F13760` | They are queue-status predicates, not record-order credentials. |
| `EffectAnimationQueue.StartFrontAnimations` | private | `0x1F13B68` | Front queue takes priority at update. |
| `EffectAnimationQueue.ShouldStartAnimation` | private | `0x1F13CA4` | Controls whether an animation starts. |
| `EffectAnimation` | TypeDefIndex `5345` | — | Stock animation base type. |
| `EffectAnimation.CompleteSignal` | field | `0x20` | Potential queue-compatible completion event. |
| `EffectAnimation.Start` / `Cancel` / `FinishAnimation` | methods | `0x1F0DD50` / `0x1F126D4` / `0x1F10AA0` | Define visual lifecycle. |
| `EffectAnimation.IsCompleted`, `CanYield`, `OnStarted` | virtual / abstract | — | Required behaviors for a custom animation subclass. |
| `EffectAnimationFactory.CreateEffectAnimation<T>()` | generic | generic specialization `0x238D2EC` shown in dump | Stock-injected animation creation. |
| `EffectAnimationGroup.AddAnimation` | method | `0x1F12990` | Group visuals when causally related. |
| `TweenCardEffectAnimation` | TypeDefIndex `6135` | ctor `0x1FD17B4`, `IsCompleted` `0x1FD141C` | **Already used by CustomComponents**. |
| `TweenCardEffectAnimation.MoveCardView`, `TargetPosition`, `TargetScale`, `TweenAnimationTime`, `AlwaysYield` | fields | `0x30`, `0x38`, `0x44`, `0x50`, `0x54` | Existing hand-discard lift/shrink implementation. |
| `DiscardFromPlayRecordProcessor.ProcessRecord(DiscardFromPlayRecord)` | method | `0x1F0B6A4` | Stock example: turns a record into targeted visual work. |
| `DrawCardRecordProcessor.ProcessDrawCardRecord(DrawCardRecord)` | method | `0x1F10BE8` | Stock example of card creation/load/draw presentation with follow-up processing. |
| `HistoryFilter.FilterAssociatedRecords`, `FindFirstRecordOfType`, `FindNextUnprocessedAttackRecordFromGroup` | methods | see dump around line 219920 | Causal grouping and preloading may inspect stock record types and history positions. |
| `HistoryCreatedCardFinder` | TypeDefIndex `5648` | — | Preloads imagery for recognized create/transform records. |
| `GamePredictorHistoryProcessor.ProcessHistory(Play)` | method | `0x1F5ABA0` | Predictive history execution is **separate** from frontend history. |
| `GamePredictionHistoryModel.MarkAsProcessed`, `IsProcessed`, `BlockFor`, `UnblockFor` | interface methods | — | Predictions may suppress/hold frontend execution. |

### 4.4 Concrete supported stock record types to preserve

For audits and correlation, at minimum recognize: `Buff=6`, `CardMove=23`, `Heal=25`, `ReturnToHandFromPlay=27`, `CreateCard=28`, `AbilityGranted=33`, `AbilityRemoved=34`, `DrawCard=42`, `DiscardFromPlay=45`, `DrawCardFromSuperpowerPool=53`, `ExtraAttack=65`, `CardReturnedToPlayersDeck=74`, `DiscardFromDeck=75`, `SetStat=77`, `TriggeredAbilityGranted=80`, `TriggeredAbilityRemoved=82`, and `CreateCardsInDeck=85`. The full enum range is **0–88**. The record audit must use the game’s actual concrete record objects and payloads, not enum names alone.

---

## 5. Native ARM64 assembly: verified control flow

The following observations are **disassembly-derived**, using `llvm-objdump -d --arch=aarch64` on the uploaded ELF. Comments summarize registers/call structure, not decompiled managed source. Machine offsets shown are RVAs for this binary only.

### 5.1 How stock records enter the list

**`History.AddRecord(Type)` — `0x22829BC`**

- Reads `History.Instances` from `this+0x10` at `0x22829EC`.
- Invokes a virtual instance-acquisition call with the requested `recordType` at `0x2282A04`.
- Invokes a virtual method on `History` with the newly created record at `0x2282A50`, consistent with `AddRecord(Record)`.
- Returns that record; failure/null paths are present. **Therefore `History.AddRecord(Type)` is an actual pooled-record creation-and-append path**, not merely an event notification.

**`History.AddRecord(Record)` — `0x2282A74`**

- Reads `this+0x18` (`History.Records`) at `0x2282AA4`.
- Performs `List<Record>.Add`, including capacity/growth and write barrier paths, from `0x2282AB4` onward.
- Does **not** create a new managed type or magically register a processor.

**`DiscardFromDeckRecord..ctor` — `0x2287348`**

- Calls the base `Record` constructor at `0x2287350`.
- `mov w8, #0x4b` (`75`) and `str w8, [x19,#0x10]` at `0x2287354–0x2287358`: **the concrete class constructor sets `RecordType=75`**. This confirms why picking an unused enum integer or constructing an arbitrary subclass is not enough.

**`Record..ctor` — `0x22837C4`**

- Creates/initializes its data dictionary and writes it to `this+0x18` at `0x2283828`.
- The body is not a guarantee that arbitrary user-created subclasses will be correctly pooled or serialized.

**`History.RemoveAllRecords` — `0x2282B48`**

- Enumerates held records and invokes an instance-manager return/pool path before clearing the backing list. **Custom IL2CPP subclasses must not be appended without a return-to-pool contract.**

**`DiscardFromDeckSystem.RecordCardDiscarded` — `0x21FF55C`**

- Reads the injected `History` at `this+0x28` and acquires a record via an instance-based interface, then calls `DiscardFromDeckRecord.SetDataFromEffect` at `0x2287490`. This is the exemplar for keeping the existing stock record path intact.

### 5.2 How stock frontend history advances

**`HistoryProcessor.ProcessHistory` — `0x1F709E8`**

- Checks the processor model `CanRun` field at model `+0x28` (`0x1F709F4–0x1F709F8`).
- Gets records from its injected `History` and branches to `ProcessRecords` at `0x1F70A20`.

**`HistoryProcessor.ProcessRecords` — `0x1F70A30`**

- Loads the cursor `CurrentPositionInHistory` from model `+0x18` at `0x1F70A74`.
- Compares cursor with `IList<Record>.Count` at `0x1F70AD4–0x1F70AD8`.
- Calls `GetNextRecord` at `0x1F70AE4` **before** deciding whether the record was predicted or processed.
- Calls `TryProcessPredictedRecord` at `0x1F70AF4`.
- If necessary, tests `HistoryProcessorModel.IsRecordProcessed` at `0x1F70B08`, then calls `HistoryProcessor.ProcessRecord` at `0x1F70B18`.
- Increments and stores the cursor at `0x1F70B34–0x1F70B3C`; repeats while the model allows processing.

**Important implication:** a `ProcessRecord`-only hook misses records suppressed because they were already predicted or already processed. An in-band custom-history boundary **before** the stock record is considered must account for **all** cursor advancements, including skipped stock presentations. `GetNextRecord` is one feasible prototype boundary because the compiled loop calls it for each stock index; this does not yet establish hook safety or multiplayer correctness.

**`HistoryProcessor.GetNextRecord` — `0x1F70B58`**

- Reads `CurrentPositionInHistory` at `0x1F70BA0`.
- Calls the list’s indexer to fetch the current stock record at `0x1F70BF8`.
- Logs/returns the selected object. A detour here can observe the precise **before-stock-index** boundary in a controlled prototype, but must be guarded against reentrancy and scene/model mismatch.

**`HistoryProcessor.ProcessRecord` — `0x1F70CF8`**

- Loads `SignalRecordExecutor` at `this+0x20` (`0x1F70D28`).
- Invokes its `Execute(record)` method through interface dispatch at `0x1F70D84–0x1F70D90`.
- Then calls `HistoryProcessorModel.MarkRecordAsProcessed(record)` at `0x1F70DA8`.

**`HistoryProcessor.TryProcessPredictedRecord` — `0x1F70C20`**

- Checks prediction status and, when appropriate, marks the record processed and uses the injected `PredictionHistoryModel` at `this+0x38`.
- This confirms why a new managed record class requires prediction-policy decisions, not just frontend dispatch registration.

**`HistoryProcessorModel.MarkRecordAsProcessed` — `0x1F70DB0`**

- Adds to `ProcessedRecords` (field `+0x20`). This is distinct from animation completion: stock history can mark a record processed even while a queued animation remains active.

### 5.3 Queue ordering and completion

**`EffectAnimationQueue.AddAnimation` — `0x1F138C4`** appends to the normal `Animations` list at queue `+0x10`.  
**`EffectAnimationQueue.AddAnimationToFrontOfQueue` — `0x1F13970`** also adds to `FrontAnimations` at `+0x18`. Front insertion changes priority and is **not equivalent** to inserting an event at its historical position.

**`EffectAnimationQueue.Update` — `0x1F13A48`** first calls `StartFrontAnimations` at `0x1F13A94`, then traverses normal animations, starts unstarted ones, calls virtual `Update` / `IsCompleted` / `CanYield`, and removes finished elements. The virtual calls and status checks are visible around `0x1F13AE4–0x1F13B44`. Therefore **queue-owned lifecycle** is the correct place to evaluate visual completion—not `HistoryProcessorModel.CurrentPositionInHistory` and not a global queue-empty flag.

**`DiscardFromPlayRecordProcessor.ProcessRecord` — `0x1F0B6A4`** obtains target ID, checks death/ability animation logic, then branches to visual work (`AddDestroyCardAnimation`). It is a useful record-to-animation model; do **not** reuse its record type for a hand/deck discard, since its semantics are death/from-play.

### 5.4 Proposed integration experiment justified by the assembly

Because the compiled stock processing loop explicitly calls `GetNextRecord` **before** predicted/processed checks, the least disruptive precise-order experiment is:

1. Hook `HistoryProcessor.GetNextRecord(IList<Record>)` with the exact instance signature and original call, resolving by BNM after IL2CPP is loaded. Verify it is not already owned by another module and that the hook's trampoline is valid.
2. On the owning **frontend** history processor, read the actual next stock index and return record. Before handing that entry to subsequent stock execution, drain custom records committed at **stock-count boundary ≤ that index**, in stable custom serial order.
3. Those custom processors enqueue **real** queue-owned visual objects immediately. No waiting for `IsEmpty()` and no front-of-queue insertion by default.
4. At the end of the processing batch, process eligible custom records at the **end boundary** (`index == final stock count`), even if there is no following stock record. The end-boundary code must respect pause/resume and prediction gates.
5. Keep `ProcessHistory` as an observation/tail-flush hook only; **remove its post-batch position as the primary dispatch point** once the in-band path is verified.

This is a **prototype hypothesis**, not an approved final hook. Instrument and validate it before migrating gameplay. One risk: calling into custom processors from `GetNextRecord` might re-enter model/list operations; the prototype must only **enqueue visuals**, never run new gameplay effects or modify `History.Records`.

---

## 6. Recommended architecture: CustomHistory, not a second replay journal

### 6.1 Data flow

```text
SIMULATION / GAME MODEL                         FRONTEND / VIEW
────────────────────────                         ───────────────
Stock effect ── stock Record ───────┐            Stock HistoryProcessor
                                    │            ├─ before-next-stock boundary
Custom effect ── commit mutation ──┼──→ CustomHistory merger / ordering
                                    │            ├─ custom registered processors
Custom mutation record ────────────┘            ├─ stock RecordExecutor
                                                 └─ SAME EffectAnimationQueue
                                                    ├─ record-owned tween / group
                                                    ├─ started / completed / canceled
                                                    └─ view cleanup + acknowledgment

Optional debug observer ← snapshots from CustomHistory (and/or old ReplayEventBridge)
Optional MatchRecorder ← observer API, not a dependency and never presentation owner
```

### 6.2 Suggested package/files (new code in the implementation phase)

```text
Bloom/CustomComponents/inc/history/
    CustomHistory.hpp             # public typed record API; immutable record values
    RecordTypes.hpp               # Kind, schema versions, origin, result, flags
    RecordSchema.hpp              # validated payloads, not anonymous int slots
    RecordRegistry.hpp            # producer declaration and processor registration
    RecordWriter.hpp              # commit at verified mutation; operation scope
    ModelHistoryStore.hpp         # per-epoch, per-model storage and thread boundaries
    StockHistoryAdapter.hpp       # exact stock History/HistoryProcessor access
    HistoryOrderMerger.hpp        # dispatch before-stock and at-tail anchors
    PresentationRegistry.hpp      # kind → queue-owned animation creator
    PresentationLifecycle.hpp     # enqueued/started/completed/canceled/no-visual
    Correlation.hpp               # stock ref, action ID, effect group / parent IDs
    SceneLifecycle.hpp            # safe reset, cancellation and epoch tracking
    Diagnostics.hpp               # invariants, counters, safety state
    Policy.hpp                    # live/predicted/PvE/PvP authority classification

Bloom/CustomComponents/inc/presentation/history/
    DiscardOutsidePlayProcessor.hpp
    ReturnFromDiscardProcessor.hpp
    PickThreePresentationProcessor.hpp
    ... one per family as warranted ...

Bloom/CustomComponents/tests/
    CustomHistoryOrderingRegression.cpp
    CustomHistoryLifecycleRegression.cpp
    CustomHistoryDualModelRegression.cpp
    CustomHistoryNoVisualRegression.cpp
    CustomHistoryBufferPressureRegression.cpp
    CustomHistoryPredictionRegression.cpp
    CustomHistoryDiscardRegression.cpp
```

The precise filenames may be adapted to the existing project style. Do not scatter record storage and animation lifecycle across each effect header. Each effect should invoke a small, stable writer API.

### 6.3 Typed schema and record payloads

A recommended **conceptual** schema (not copy-paste production code):

```cpp
namespace CustomComponents::History {
    using RecordId = std::uint64_t;
    using MatchEpoch = std::uint64_t;

    enum class Kind : std::uint16_t {
        DiscardOutsidePlay, DeckReorder, ReturnFromDiscard,
        ZoneTransfer, CardCreated, SelectionRequest,
        SelectionResolved, StatChanged, AbilityChanged,
        TagChanged, SubtypeChanged, CombatAction, PresentationRequest
    };

    enum class Outcome : std::uint8_t {
        Committed, Rejected, Canceled, PartialFailure
    };

    struct OrderKey {
        MatchEpoch epoch;
        std::uint64_t modelKey;
        std::uint64_t historyGeneration;
        std::uint32_t stockCountBeforeNextRecord;
        std::uint64_t localSerial; // stable order among same-anchor actions
    };

    struct RecordHeader {
        RecordId id;
        Kind kind;
        std::uint16_t schemaVersion;
        OrderKey order;
        std::uint64_t correlationId, parentActionId;
        int sourceEntityId, effectEntityId, effectGroupId, ownerPlayerId;
        Outcome outcome;
    };

    struct DiscardOutsidePlay {
        RecordHeader header;
        int cardEntityId, cardGuid;
        int sourceZoneId, destinationPileId;
        enum class Source : std::uint8_t { Hand, Deck } source;
        bool transferVerified;
        bool sourceWasFaceDown;
    };
}
```

**Design notes:**

- Use **tagged typed payloads** (`std::variant`, controlled tagged union, or equivalent) instead of one `amount/aux` pair whose interpretation changes by record kind.
- Store only scalars, bounded value vectors and strings copied into native ownership. Never retain an unmanaged `Il2CppObject*` across simulation/frontend lifetimes in the record itself.
- **Entity ID** identifies a physical runtime card; **GUID** identifies card definition. They are not interchangeable.
- A single action may produce several records; use a **correlation ID** and parent/action ID for causality and a stable event serial for ordering.
- Preserve before/after data needed for visual reconstruction (stat deltas, old/new positions) **at commit time** before later game effects mutate the model again.
- A `RecordId` must remain stable for the match and never be recycled; `MatchEpoch` prevents pointer-reuse confusion after a scene change.
- Add a semantic policy flag `VisualRequired`, `VisualOptional`, or `NoVisual` (not inferred from whether a processor happens to be registered).
- The engine must retain functional records until all required deliveries have reached a terminal state. Snapshots may be pruned separately after safe completion. An optional diagnostic ring buffer may drop old events, but **functional pending records must not silently disappear**.

### 6.4 Suggested publication API

```cpp
// Illustrative interface; implement against existing Engine::Match helpers.
auto action = CustomHistory::BeginAction(context, "DiscardFromHand");
if (!transferVerified) {
    action.Fail(CustomHistory::Reason::IncompleteTransfer);
    return;
}

auto recordId = action.Commit(DiscardOutsidePlayPayload{
    .entityId = cardId,
    .source = SourceZone::Hand,
    .fromZoneId = handId,
    .toZoneId = pileId,
    .verifiedTransfer = true
});
if (!recordId) {
    LOG_ERR("CustomHistory: required record commit failed after transfer ...");
    // Explicit fail-safe policy, not a hidden second animation path.
}
```

Keep record creation inside the authoritative mutation path; keep any frontend class lookup, GameObject operation, tween creation and mapper access strictly outside it.

### 6.5 Exactly-once semantics

There are **two orthogonal state machines**:

```text
GAMEPLAY: Proposed → Committed | Rejected | Canceled | PartialFailure
VISUAL:   NotRequired | AwaitingDispatch → Enqueued → Started
                                 → Completed | Canceled | Failed | SkippedNoView
```

`Enqueued` is not `Completed`. A no-visual record only succeeds with an **explicit** no-visual policy. A failed view lookup is **not** an implicit no-visual record. A canceled scene ends outstanding visuals in a defined terminal state while keeping the committed gameplay result historically truthful.

**Important subtlety:** stock `HistoryProcessorModel.MarkRecordAsProcessed` also does not wait until the associated animation completes; it means the presentation record was dispatched. CustomHistory should mirror that distinction, not block all future stock records until a tween ends. The stock animation queue, rather than the history cursor, controls when previously queued visuals yield or finish.

---

## 7. Precise ordering design and execution rules

### 7.1 Stock boundary anchoring

For a custom mutation committed when the stock history contains `N` records, store the ordering boundary `N`: it happened **after** stock records `0..N-1` and **before** any subsequently appended stock record at index `N`. When several custom records share `N`, order them by their own monotonic serial.

Example:

```text
stock[14]  (already appended)
custom A   (committed when history count = 15)
custom B   (committed when history count = 15)
stock[15]  (later appended)
custom C   (committed when history count = 16)
stock[16]

Frontend queue-enqueue order: stock[14], A, B, stock[15], C, stock[16]
```

**Required validation:** This works only if the mutation’s authoritative `History` instance is the **same history** the matching frontend processor consumes. `History.Count` must be obtained from that actual instance, not guessed from another injected model. Distinct model histories must be keyed separately.

### 7.2 Merger algorithm

Conceptual algorithm:

```text
on BeforeStockRecord(history, nextStockIndex):
    require matching match epoch, live frontend model and history generation
    require stock processor CanProcess and not recursively inside CustomHistory
    while pending custom records have boundary <= nextStockIndex:
        dispatch next record in (boundary, localSerial) order
        create/enqueue its queue-owned presentation now (or explicit NoVisual)
    return control to stock record executor

on AfterHistoryBatch(history, finalStockCount):
    drain records at boundary <= finalStockCount that have no next stock record
    do not bypass pause/prediction/model owner checks
```

**Do not** call the historical v6 `ProcessReady` after all stock processing as the authoritative path. For every custom action `C`, trace `stock record immediately preceding C`, `C`, `stock record immediately following C`, and normal/front animation queue insertion order. This proves actual position.

### 7.3 Hook feasibility ladder

1. **Read-only tracing**: instrument `ProcessHistory`, `GetNextRecord`, `ProcessRecord`, `RecordExecutor.Execute`, `EffectAnimationQueue.AddAnimation`, `Update`. Log only when enabled; keep exactly one owner of each native detour. Confirm that a chosen method is actually invoked in relevant scenes, including predicted/processed stock records.
2. **No-op in-band insertion**: register one no-visual test custom record and prove it dispatches between two adjacent stock indices with no new GameObjects, no changed game behavior, no MatchRecorder dependency.
3. **Queue-owned minimal animation**: enqueue a harmless stock `NullEffectAnimation` or controlled queue-compatible test animation at that boundary; verify no reorder and no duplicate completion.
4. **One real discard**: enqueue existing `TweenCardEffectAnimation` at dispatch, and let the queue decide start time. Protect the correct hand view identity and mark terminal completion on tween completion/cleanup.
5. **Failure/cancellation**: test view missing, opponent privacy, scene removal, paused history, stock queue front animation, and predicted-record skip. Ensure deterministic failure states.
6. **Only then** replace the old ticket/idle path for all hand discards.

**Hook candidate ranking:**

- `HistoryProcessor.GetNextRecord(IList<Record>)` (`0x1F70B58`): best **assembly-grounded candidate** for a before-stock boundary, because it executes even for predicted/already-processed stock entries. Requires careful reentrancy protection and runtime confirmation.
- `HistoryProcessor.ProcessRecords(IList<Record>)` (`0x1F70A30`): safe for batch entry/exit and cursor diagnostics, but a simple wrapper around the original cannot interleave actions inside its loop.
- `HistoryProcessor.ProcessRecord(Record)` (`0x1F70CF8`): useful for observing actual stock dispatch but **insufficient alone** because prediction/processed cases skip it.
- `HistoryProcessor.ProcessHistory()` (`0x1F709E8`): retain for batch/tail and context, not primary in-band dispatch.
- `EffectAnimationQueue.AddAnimation`: useful for queue ownership/order verification; avoid changing stock queue insertion semantics globally without proof.

**Do not patch machine-code RVAs directly for release;** use BNM method resolution, `Core::Hooks::Install`, checked signatures and valid originals. The game uses IL2CPP native signatures with the trailing `const MethodInfo*` on instance methods; match each resolved target and original pointer exactly.

### 7.4 Live model and prediction policy

Design a single authority classifier shared by all custom systems:

- `LiveAuthoritativeModel`: accepted for authoritative gameplay record commit.
- `LivePresentationReplica`: accepted as a frontend consumer of the corresponding committed record; may need peer-local record creation or a synchronized immutable payload.
- `PredictionModel` / `AIProbe`: no visual commits; must not clear another model's records.
- `DetachedOrSceneClosing`: reject new visual work, safely terminalize pending presentation.

**Open question requiring device traces:** how an authoritative CustomComponents effect propagates across PvP clients and which exact locally materialized `History` feeds each frontend. Do not assume a pointer inequality with `SceneViewContext` establishes a prediction model. Use existing game `GameFacade`, `PvpAuthorityDecider`, `GamePredictorHistoryProcessor` and local scene ownership evidence to classify contexts. Define behavior for opponent-authored actions, not just the local player.

---

## 8. DiscardOutsidePlay: first complete record and presentation processor

### 8.1 Desired semantic records

**`DiscardOutsidePlayRecord`** covers verified card transfers originating from **hand or deck**, preserving source-specific semantics. It does **not** mean `DiscardFromPlay` death, and it is distinct from a `DiscardOutsidePlayTrigger`.

Payload: `RecordId`, record kind/schema, card entity ID, definition GUID if available, owner/player, hand/deck ID, pile ID, origin `Hand|Deck`, source effect ID/group, verified before/after membership counts if available, ordering boundary, correlation/action ID, and presentation policy (including whether opponent card identity may be shown). If a deck discard already emits `DiscardFromDeckRecord` type 75, the custom record is **supplemental provenance only** or omitted under explicit stock-first rules; never schedule a duplicate discard animation.

### 8.2 Simulation publisher

Refactor the successful path in `inc/zones/detail/HandToDiscard.inl` into a small shared helper after verified transfer and discarded-origin marking:

1. Capture hand ID, pile ID, owner ID, card ID, effect/source IDs **before** other triggers can alter state.
2. Verify the transfer ended with exactly one physical card removed from the hand and present in the pile. Do not infer success from an effect callback.
3. Commit **one** `DiscardOutsidePlay` record to the canonical live model's CustomHistory with exact stock-history boundary.
4. Only then fire `DiscardOutsidePlayTrigger`. Trigger observations must not generate another discard mutation record.
5. Failed/partial transfers must return explicit failure status. Preserve current non-atomic safety rules; no speculative reinsertion or cloning.

For `DiscardFromDeckSystem.hpp`, continue calling the stock `DiscardFromDeckSystem.ProcessEffect` and compare pre/post physical deck entries. Preserve its stock type-75 record. The custom record should give card ID and causal source when genuinely missing from the stock metadata; do not blindly generate another visual.

### 8.3 Presentation processor

Replace `DiscardFromHandPresentationSystem::ProcessDiscardRecord`'s current **ticket-only** response with a record-specific visual processor:

1. Called **in-band**, between the correct stock history records, on the owning frontend model/queue.
2. Resolve and validate the exact `EntityViewMapper`, `HandViewComponent` (local or opponent), and **physical card view identity**. Hold managed references through the project's `Core::ManagedRuntime::Root` where required.
3. Use the existing `DiscardCardAnimation` machinery / `EffectAnimationFactory.CreateEffectAnimation<TweenCardEffectAnimation>` and enqueue via normal `EffectAnimationQueue.AddAnimation`. Do not wait for global queue emptiness; do not force a front-queue insert.
4. For opponent hand, retain hidden-identity policy and validated stock hand geometry. Never reveal a concealed card merely because its definition GUID is recorded internally.
5. If lift+shrink are both required, put both in a queue-owned `EffectAnimationGroup` or a proven record-owned sequence; ensure shrink cannot be overtaken by later card retirement.
6. Use the tween `IsCompleted`/completion signal and a record-specific cleanup callback. Ensure cleanup **unmaps/destroys only the expected exact view**; if the card returned to a new hand or its mapper entry now points elsewhere, refuse stale removal and log a reason.
7. Mark `PresentationCompleted` **after** the appropriate animation terminal and view cleanup; mark canceled or skipped explicitly for valid scene/visibility cases.
8. Stop using `normalEmpty && frontEmpty && CaughtUp()` as the normal **start** condition. Keep targeted view readiness/ownership checks without making them a global all-history barrier.

### 8.4 Prove correct timing, not just eventual completion

**Pass:** A hand discard committed between stock A and B is **enqueued** between the animations generated by A and B, regardless of whether the queue is currently busy. It starts when preceding stock effects yield or complete according to stock queue behavior. There is exactly one lift/shrink/cleanup per committed physical discard (or an explicit no-visual outcome). A second discard at the same stock boundary has deterministic order and no shared ticket collision.

**Fail:** It waits until all later stock animation work completes; a newly inserted later stock animation overtakes it; a repeated fallback schedules a second tween; it deletes a remapped card view; a record is acknowledged merely because its processor was called.

---

## 9. PickFromThree: functional selection, popup, and effects records

PickFromThree is not a single mutation. The source includes candidate selection/snapshots, actual submission and validation, discover/UI staging and subsequent gameplay effects. An observation that a callback fired is **not** sufficient evidence that the popup appeared, a choice was accepted, or a resulting card move committed.

Recommended correlated families:

| Kind | Owner | Commit/observation rule | Frontend role |
|---|---|---|---|
| `PickThreeRequest` | gameplay selection setup | Validated request with owner, source effect, group and request ID | Schedule popup at correct history boundary. |
| `PickThreeOptions` | candidate selection | Original ordered list of 1–3 actual card entity IDs/GUIDs | Render the correct options, not guessed from later callbacks. |
| `PickThreePopupPresented` | UI presenter | Only when frontend has actually shown options | Presentation diagnostic/processing state; not a gameplay mutation. |
| `PickThreeSelectionSubmitted` | input layer | Actual index/physical entity, request ID | Input provenance. |
| `PickThreeSelectionAccepted` / `Rejected` | stock validation/submission | Authoritative validation outcome, distinct from callback | Close or keep popup appropriately. |
| `PickThreeEffectCommitted` | underlying gameplay effect | References **actual** `DeckReorder`, `CardCreated`, `ZoneTransfer`, etc. record IDs | Never fake a zone change merely to close popup. |
| `PickThreePopupDismissed` / `Canceled` | UI lifecycle | Completion/cancel state from actual view | No stale screen, no duplicate cleanup. |

Source integration points to audit and migrate: `inc/pick3/PickedFromThreeSelection.hpp` (`pick_three_callback` currently emitted); `inc/pick3/PickFromThreeTargetBridge.hpp`; `inc/pick3/submission/*` (validation and submit); `inc/presentation/PickFromThreeDiscoverPresentation.hpp` (`Publish`, `Show`, `Submit`, `FINISH`, `PRESENT`, `HOLDER_TICK`). **Do not** let a popup record override the stock selection authority or fabricate a successful decision.

Special cases to cover: opponent popup never shown locally; 1/2/3 valid candidates; auto-sole selection; deferred secondary target/lane selection; multiple simultaneous effect groups; cancel/disconnect; AI/prediction visits; popup visible while stock process/history cursor pauses.

---

## 10. Full CustomComponents migration ledger

**Statuses describe the present code, not finished v7 coverage.** `Keep stock` means validate exact stock record; `v6 native` means an initial sidecar exists but still needs in-band integration and/or improved schema; `Audit` means inspect implementation before selecting a record path; `New record` means the existing action needs a truthful custom record where no corresponding stock record is verified.

| Family / effect(s) | Source entry points | Current record evidence | v7 work / processor policy |
|---|---|---|---|
| Hand discard | `inc/zones/detail/HandToDiscard.inl`, `inc/presentation/DiscardFromHandPresentationSystem.hpp` | **v6 native** `DiscardOutsidePlay`, ticketed tween; no stock hand discard | **First migration**: in-band `DiscardOutsidePlay`, queue-owned animation, exact cleanup and completion. |
| Deck discard | `inc/zones/DiscardFromDeckSystem.hpp` | **Keep stock** `DiscardFromDeckRecord` type 75; custom trigger and replay provenance | Verify actual card IDs and stock record count for multiple/random targets; add only missing causal metadata. |
| Remove from discard pile | `inc/zones/ReturnDiscardToHandSystem.hpp` remove-only path | **v6 native** `ReturnFromDiscard` with unknown destination | Model record `DiscardPileRemoved` or precise source mutation; do not invent an unknown destination or animation. |
| Return discard → hand | `inc/zones/ReturnDiscardToHandSystem.hpp` | **v6 native** provenance and **synthetic DrawCardRecord** visual path | Replace synthetic draw after standalone custom return processor is proven; correct source-zone visuals, opponent privacy and card-view lifetime. |
| Return discard → deck | `inc/zones/detail/DiscardToDeck.inl` | **Keep stock** type-74 `CardReturnedToPlayersDeckRecord` and v6 provenance | Link custom provenance to exact stock record; no duplicate deck-return visual; validate discard origin. |
| Return discard → lane | `inc/zones/ZoneTransferSystem.hpp`, `inc/zones/detail/LaneTransferExecution.inl` | **Keep stock** lane creation/placement records | Correlate source zone and exact physical entity; ensure no duplicate on-death / on-play / returned trigger. |
| Play from deck → lane | Same lane transfer files | **Keep stock** lane record plus journal | Validate physical deck removal, new lane entry and one stock placement; no cloning on partial failure. |
| MoveCardInDeck | `inc/zones/MoveCardInDeckSystem.hpp` | **v6 native** `DeckReorder` with original/destination indices | Add authoritative history boundary, source effect, index semantics, no visual unless a UI deck animation is actually needed. |
| DrawCardFromSubsetInDeck | `inc/effects/DrawCardFromSubsetInDeckSystem.hpp`, `inc/effects/detail/SubsetDrawExecution.inl` | **Keep stock** draw record + subset-draw observer | Validate one real draw record per actual card; optional subset-choice provenance correlation. |
| CreateCardInDeckFromSubset | `inc/effects/CreateCardInDeckFromSubset.hpp` and helper | **Audit** choice GUID; generated entity identity may be missing | Record exact created entity ID/GUID, owner/deck and quantity after commit; reuse stock create/create-in-deck record if emitted. |
| CreateSuperpowerSubset | `inc/zones/CreateSuperpowerSubset.hpp` | **Audit** creation and pool membership | Exact created entity/GUID, pool and owner; avoid second created-card count. |
| PickFromThree/deferred selection | `inc/pick3/**`, `inc/presentation/PickFromThreeDiscoverPresentation.hpp` | Callback observations, popup managed outside functional history | Request/options/accepted/committed/dismissed lifecycle; queue-owned request/close presentation with correlation. |
| GrantAbility / RemoveGrantAbility | `inc/abilities/GrantAbilitySystem.hpp`, `inc/effects/RemoveGrantAbilityEffectDescriptor.hpp` | **Keep stock where present** `AbilityGrantedRecord` and `AbilityRemovedRecord` | Audit every direct/remove/expiry branch and record source, target, ability ID, duration, visual refresh. |
| Random keyword grant | `inc/abilities/RandomKeywordSystem.hpp` | **Audit** uses grant infrastructure | Correlate rolled keyword with actual stock ability record; no fabricated success for failed rolls. |
| Triggered-ability grants | `inc/abilities/TriggeredAbilityGrants.hpp` | **Audit** stock triggered-ability record types 80/82 | Verify attach/detach/duration and record identity; add missing semantic info. |
| OverhealToHealth | `inc/abilities/OverhealToHealth.hpp` | **Keep stock** buff through stock BuffEffect path per source audit | Ensure excess-heal amount and per-primary-target evaluation; do not emit duplicate buff record. |
| Withering and duration changes | `inc/abilities/Withering.hpp`, `WitheringSystem.hpp` | **Audit** mixed custom/stock mutations | Record exact stat/temporary state before/after; ensure visual expiration and hand/deck/board variants. |
| Charge / Thrash / combat modifiers | `inc/abilities/Charge.hpp`, `Thrash.hpp`, combat systems | **Audit** combat and stock effects | Use stock damage/attack records where present, custom combat schedule records for truly novel ordering. |
| ExtraCombat | `inc/combat/ExtraCombat.hpp` | **Audit** multiple combat-step triggers | Preserve stock attack/damage and phase records; add causal grouping and prevent repeated presentation. |
| SelfTeleport / SwapPosition | `inc/abilities/SelfTeleport.hpp`, `inc/effects/SwapPositionEffectDescriptor.hpp` | **Audit** direct move and placement | Correlate with stock `CardMove`/lane records; capture old/new lane, affected entities and failures. |
| Card tags | `inc/effects/CardMetadataEffects.hpp` | **New record** when direct change has no stock equivalent | `TagChanged(target, tag, add/remove, old/new)`; frontend inspection/icon refresh policy. |
| Card subtypes | Same | **New record** when direct change has no stock equivalent | `SubtypeChanged(target, subtype, add/remove, old/new)`; frontend card metadata refresh. |
| Level / LevelCostTint / selection | `inc/level/**` | **Audit** state changes, selection and visual modifiers | Differentiate level gameplay mutation from presentation tint; capture source and old/new levels. |
| Variable counters / multipliers | `inc/variables/**` | **Audit** many query-only components | Record persistent counter/state mutation; do **not** record mere query evaluation as a gameplay mutation. |
| MarkedForDiscard / PreDiscard | `inc/zones/MarkedForDiscard.hpp`, `PreDiscardSystem.hpp` | **Audit** pre-death/cancel state | Lifecycle/cancellation records, not a premature committed discard. |
| Custom trigger dispatch | `inc/triggers/**` | Existing `trigger_*` observations | Correlate trigger to parent mutation; do not double-record card moves. |
| View/animation refresh, stat reconciliation | `inc/presentation/**`, `inc/engine/FrontendStateRefresh.hpp` | Presentation hooks and queue idleness gates | Presentation-only records if useful, but never claim they changed gameplay; use actual queue completion. |
| Queries | `inc/queries/**` | No mutation by definition | No required mutation record; optional sampled diagnostics, not in functional history. |

### Enforced review rule for future effects

Every new effect pull request/change must answer: **What changes? Where is the authoritative commit? Does it already produce a stock record? Which exact card/entity IDs matter? What history boundary? Is presentation required? What is the completion/cancel policy? How does this behave in prediction/PvP?** The feature is incomplete if any required answer is missing.

---

## 11. Managed subclass vs integrated sidecar: research decision gate

### Option A — true `PvZCards.Engine.Record` subclass stored in `History`

**Possible in principle**, but not yet proven safe. `History.AddRecord(Type)` can acquire a `Record` subclass through `InstanceManager`; `Record` has a concrete constructor, type discriminator and dictionary payload. However:

- `RecordExecutableFactory` maps concrete **managed Type** to `Executable`; `HistoryProcessorBinder` binds known processors. New class registration alone doesn't add those mappings.
- Each stock record constructor sets a known enum (`DiscardFromDeckRecord..ctor` writes `75`); the declared enum ends at `88`. An arbitrary numeric extension can hit filters, switch/dispatch, network or analytics behavior not covered by the dump.
- Stock history cleanup returns records to the instance manager; a BNM-created subclass must pass constructor, inject/pool acquisition and return-to-pool tests.
- `Record.CompareTo`, prediction history and `GamePredictionHistoryModel` require equivalence/processed behavior. Some code uses concrete `Type`, some uses `RecordType`, some uses lists of selected record types (`HistoryCreatedCardFinder`).
- PvP synchronization/serialization may reject unknown classes or fields. The dump and local assembly **do not prove** a compatible serialization path.

**Go/no-go experiment:** in an isolated offline match, create a harmless new managed class derived from `Record` using supported BNM class management, verify pooled acquisition, correct dictionary, append, frontend executor registration, safe `CompareTo`, history filtering, cleanup, and prediction skip behavior. Then test a **separately gated** two-client PvP session. A missing capability at any step means **do not ship this route**.

### Option B — ordered CustomComponents-native sidecar with stock queue integration

**Recommended first implementation.** Keep typed native records and the validated gameplay simulation path, but **dispatch within the exact frontend stock-record loop** and enqueue proper stock `EffectAnimation` objects. This meets the immediate requirements of functional history and correct presentation ordering without risking unknown managed subclasses in server/replication paths.

It is not literally stored as a stock managed `Record` inside `History.Records`; document that distinction. It can still be **first-class within CustomComponents** with complete semantics, processors, matching order, and independent lifecycle.

### Decision

**Build Option B first**, prove discard timing and multi-record ordering. Investigate Option A as an **independent experimental branch**; promote it only if the above constraints are verified. Do not block correcting the visible discard delay on a speculative new managed `RecordType`.

---

## 12. Implementation phases and acceptance gates

### Phase 0 — freeze, tracing, and baseline

**Changes:** No behavior changes. Add optional, budgeted tracing with precise records, history indices, frontend queue inserts, and completion; capture method ownership and original trampolines.

**Required artifacts:** source hashes/baseline, hook conflict map, paired `dump.cs`/binary build ID, logs from known two-discard/return/Pick3 scenarios, stock and custom event ordering traces.

**Pass:** Each observed simulation write is attributable to one model/history, each history processor to one view scene, with clear stock cursor before/after and queue identity. No gameplay change.

### Phase 1 — CustomHistory core and model isolation

**Changes:** Extract v6 `CustomRecords` into typed schemas/storage/registry/lifecycle, add epoch and history-instance identities, separate diagnostic snapshots from required delivery, add explicit no-visual semantics and errors. Keep v6 compatibility adapter under a flag.

**Pass:** Host tests for same-boundary serials, multi-model concurrency, callbacks and cancellation, absent processor, full buffer, no mutation-on-replay, disposed/reused model pointer, duplicate commits, and observer absence. No MatchRecorder linked.

### Phase 2 — in-band stock-history merger (no gameplay visuals yet)

**Changes:** Read-only `GetNextRecord`/cursor experiment, then controlled in-band dispatch for no-visual test records, validated `CanRun`/prediction and end-tail handling. Keep existing stock `RecordExecutor` unmodified.

**Pass:** In a known sequence `stock A → custom C → stock B`, logs show custom dispatch before B, including when A is predicted/marked processed. No missing or duplicate stock record execution; paused history doesn't allow custom to run ahead.

**Rollback:** Disable only `CustomHistoryInBandDispatch` via config; stock processing remains intact.

### Phase 3 — record-owned hand discard and queue-native animation

**Changes:** Create `DiscardOutsidePlay` schema/processor; enqueue existing stock `TweenCardEffectAnimation` in-band; coordinate view lifecycle through record ID and queue completion. Remove successful-discard idle-queue scheduling, retain isolated legacy fallback only under a diagnostic switch. Allow grouped simultaneous discards.

**Pass:** Local/opponent discards work, exact enqueue order with preceding/following stock effects, no duplicate lift/shrink, no remapped view destruction, clean scene cancellation, card returns after discard, hidden opponent cards remain private. No 9–23-second **post-history idle wait** unless preceding stock animation itself legitimately occupies that interval.

### Phase 4 — full functional effect migration

**Changes:** Work through Section 10 ledger family by family. Begin with `DeckReorder`, discard returns, deck/card creation and Pick3 because those are known missing semantics. Then ability/state/metadata/combat/presentation records. Preserve existing genuine stock records.

**Pass:** Every mutating branch has a verified record path (or explicit documented reason no record is needed), and every visual-required custom record has one processor. No dual visual for a stock-recorded operation. Maintain a machine-readable coverage file and regression tests.

### Phase 5 — prediction, multiplayer, and resilience

**Changes:** Dual-model model/epoch policy, optional synchronization of custom record payloads where necessary, reconciling local-presented vs server-confirmed actions, backlog and scene lifecycle handling.

**Pass:** PvE, offline AI, both PvP perspectives, prediction rollback/replay, disconnect/reconnect, concession during animation, turn transition, multiple rapid discards, and scene reload produce no desync, accidental card reveals, lost required records, stale pointer cleanup or repeated visual.

### Phase 6 — optional observers and external integrations

Only after functional record history is stable, expose:

```cpp
using CustomHistoryObserver = void (*)(const ImmutableRecordView&);
bool CustomHistory::Subscribe(CustomHistoryObserver observer);
void CustomHistory::Unsubscribe(CustomHistoryObserver observer);
std::vector<ImmutableRecordView> CustomHistory::SnapshotAfter(RecordId after);
```

The callback is **non-owning, read-only**, and cannot block visual dispatch or mutate gameplay. MatchRecorder can subscribe when installed; the CustomComponents module has **no link-time or runtime dependency** on it.

---

## 13. Required test matrix and what to log

| Test | Expected simulation/history evidence | Expected frontend evidence |
|---|---|---|
| Discard one card from hand | Exactly one verified transfer + one committed `DiscardOutsidePlay` | One in-band enqueue, correct hand view, complete/cancel exactly once. |
| Discard two in the same effect | Two different physical IDs and serials at same stock boundary | Correct deterministic order/group; no ticket collision. |
| Discard while stock queue busy | Correct boundary relative to adjacent stock records | Enqueued immediately in history order; no whole-queue-idle scheduling. |
| Discard opponent hand | Correct source hand/owner IDs | Hidden information protected; correct opponent view and geometry. |
| Discard then return to hand/deck | Two real mutations, correctly correlated | Old view not destroyed if remapped; return is not mislabeled stock draw. |
| Deck discard with stock type 75 | Exactly one stock `DiscardFromDeckRecord` for each actual discard | No extra custom discard animation. |
| Deck reorder + subsequent draw | Same physical entity moves index then draws | No false create/draw; trace causal IDs. |
| Pick3: 3 options, one choice | Original option ordering and accepted submission; actual effect commit | Popup shown once, chosen option accurate, dismissed once. |
| Pick3: cancel or rejected choice | No invented game mutation | Canceled/rejected UI terminal state. |
| Ability grant/remove/expire | Stock or exact custom stat/ability record | Correct icon/stat updates without double effect. |
| Predictive and authoritative histories | Per-model records with correct classifier | No prediction-only visuals or cross-scene records. |
| Paused/resumed frontend | No illegal custom record advancement | Same interleaving after resume. |
| Scene unload/concede mid-tween | Gameplay commit preserved | Queue cancellation, GC roots release, no stale view deletion. |
| Missing custom processor | Commit never silently acknowledged as presented | Explicit failure or documented no-visual success. |
| Exhausted record storage | No silent loss of required committed work | Explicit backpressure/fail-safe diagnostic; avoid uncontrolled memory growth. |

### Minimal structured diagnostic schema

Record a compact, rate-limited line at each boundary:

```text
CustomHistory/COMMIT    epoch=... model=... history=... serial=... kind=... card=... atStock=... action=...
CustomHistory/BEFORE    epoch=... stockIndex=... stockType=... customPending=... canRun=... predicted=...
CustomHistory/DISPATCH serial=... beforeStockIndex=... processor=... queue=... accepted=...
CustomHistory/ENQUEUE  serial=... anim=... normalIndex=... frontIndex=... viewer=...
CustomHistory/START    serial=... anim=... timestamp=...
CustomHistory/COMPLETE serial=... anim=... cleanup=... outcome=...
CustomHistory/FAIL     serial=... stage=... reason=... fallback=...
CustomHistory/SCENE    oldEpoch=... newEpoch=... canceled=... reclaimed=...
```

Test instrumentation must distinguish **commit→dispatch**, **dispatch→enqueue**, **enqueue→actual start**, and **start→completion**. Correctness is predominantly ordering, not a universal fixed-second latency target. Timestamp the producer at commit, not when MatchRecorder later flushes its journal.

---

## 14. Configuration and rollout controls

Use independently switchable flags so a risky hook does not make CustomComponents unusable:

```ini
[CustomHistory]
Enabled=true
Trace=false
InBandDispatch=false       ; off during Phase 0 / until Phase 2 passes
FunctionalDiscard=false    ; off until Phase 3 passes
StrictMissingProcessor=true
AllowLegacyDiscardFallback=false
ExperimentalManagedRecords=false
```

Exact config syntax must be reconciled with the existing `config.ini` and initialization code. Flags must not make gameplay correctness depend on a diagnostic observer. Disabling visual integration should not silently skip a successful game mutation; failure behavior must be clearly defined and logged. Give each experimental hook a checked install result, original trampoline, and deterministic shutdown/reset policy.

---

## 15. Risks, unknowns, and specific verification tasks

| Risk / open question | Why it matters | Required experiment |
|---|---|---|
| Does `GetNextRecord` detour run reliably on both clients and all relevant modes? | Exact-order approach assumes a stable before-stock-index boundary. | Read-only trace across PvE, PvP and prediction; check hook conflicts and trampoline. |
| Is each custom publisher using the correct **same** `History` instance the frontend consumes? | Wrong `Count` produces wrong ordering. | Log `(model, history, epoch, current count)` at commit and compare with bound history on frontend. |
| Do stock callbacks generate additional records during processing? | Reentrant history mutation can invalidate naive count/index logic. | Instrument stock count before/after `GetNextRecord`, recursive processing, and custom dispatch; design reentrancy guard. |
| Can a custom tween be enqueued while a stock animation is already in flight? | Exact queue ordering must be validated, not assumed. | Insert safe test animation in normal queue at in-band boundary; compare queue lists and start order. |
| Is `EffectAnimation.CompleteSignal` practical for the current tween/BNM setup? | Completion must be lifecycle-driven rather than guessed. | Probe stock tween completion signal and cancellation; retain `IsCompleted` check as verified alternate. |
| Can opponent hand view exist when model has already moved the card? | Visual removal must not destroy a newly reused mapping. | Card view identity/root and `EntityViewMapper` tests before, during, after discard/return. |
| How does PvP replicate/predict custom-only mutations? | Functional history must match visible authoritative results on both clients. | Compare dual-client model/history identities and commit records; only add networking if evidence shows need. |
| Can arbitrary managed `Record` subclasses be pooled/dispatched/serialized? | Determines whether Option A is realistic. | Isolated managed-subclass experiment; no new enum in production without full proof. |
| Does a 4,096-entry native store ever overflow in long/retrigger matches? | Required presentation cannot silently vanish. | Synthetic burst and real match stress test; separately bound retained completed records. |
| Does a scene reset happen without an obvious local-hand Awake? | v6 single-model clearing and owner roots may become stale. | Trace game scene/level transitions, match ends and new view mapper binding. |
| Do stock record group and preload utilities need to see custom action metadata? | Popup/creation visuals might need asset preload and grouping. | Inspect/trace `HistoryFilter`, `AbilityRecordGroupFinder`, `HistoryCreatedCardFinder` for each new visual type. |

**Not proven by current offline research:** new managed-record class registration, a network encoding for unknown record types, generic array/list behavior under all AOT contexts, a guaranteed one-to-one event propagation between two PvP clients, and that every custom effect already emits stock records. These must remain explicit gates, not assumptions.

---

## 16. Concrete first coding sequence (smallest safe milestone)

1. Add `history/ModelHistoryStore.hpp` with per-match epoch + per-model/history record storage. Adapt existing v6 `CustomRecords.hpp`; keep it callable during transition.
2. Add `history/RecordSchema.hpp`, `history/RecordRegistry.hpp`, lifecycle state and validated required-processor policy. Unit-test no-visual and missing-processor outcomes.
3. Add **read-only** trace of `HistoryProcessor.GetNextRecord`, `CurrentPositionInHistory`, `History.Records.Count`, owning queue and model. Confirm runtime binding and no hook collision.
4. Add a **no-visual in-band test record** and a tail flush, preserving all stock record processing. Host-test `(A,C,B)` and cursor-skipped cases; device-test behavior disabled/enabled.
5. Add an in-band **queue-owned test animation**. Verify normal queue order and completion/cancellation.
6. Refactor `DiscardFromHandPresentationSystem::ProcessDiscardRecord` to create a real animation on in-band dispatch; retain existing `DiscardCardAnimation` for tween creation and view safety, remove global idle gate as primary scheduling.
7. Record/observe five stress scenarios: one discard, two simultaneous, busy queue, opponent hidden hand, discard immediately followed by return. Compare new structured timing log with the baseline above.
8. Roll out `DiscardOutsidePlay` for hand only. After passing, migrate deck and return semantics while maintaining native stock record ownership.
9. Proceed through the migration ledger and only then expose the optional observer API to MatchRecorder.

### Phase 1 acceptance checklist

- [ ] No new dependency on MatchRecorder.
- [ ] No unguarded additional stock hooks; original callbacks and calling signatures validated.
- [ ] Custom record from a verified hand transfer appears at the exact in-band history boundary.
- [ ] Custom presentation enqueues through the stock `EffectAnimationQueue`, not a detached ticket queue.
- [ ] No global `IsEmpty && CaughtUp` requirement for *starting* a discard.
- [ ] No duplicate or missing stock history records after the hook.
- [ ] Every required presentation has a terminal state and correlation ID.
- [ ] Proper scene cancellation; model/prediction separation.
- [ ] No card view removal after a card has been remapped/reused.
- [ ] Host tests and Android on-device tests pass; PvP is separately verified before declaring it supported.

---

## 17. Investigation index — exact pointers for the implementing engineer

### Current CustomComponents

- `Bloom/CustomComponents/inc/engine/CustomRecords.hpp` — current data schema, `Commit`, `ProcessReady`, `MarkVisualComplete`.
- `Bloom/CustomComponents/inc/engine/CustomRecordHistoryAnchor.hpp` — stock count anchor.
- `Bloom/CustomComponents/inc/presentation/StatQueueContext.hpp` — existing `ProcessHistory` hook and queue owner capture; start here for in-band migration.
- `Bloom/CustomComponents/inc/zones/detail/HandToDiscard.inl` — verified transfer + record commit; migrate after hook proof.
- `Bloom/CustomComponents/inc/presentation/DiscardFromHandPresentationSystem.hpp` — idle gate, ticket queues, view lifecycle, record handler.
- `Bloom/CustomComponents/inc/presentation/DiscardCardAnimation.hpp` — stock tween creation and completion; reuse this, not replace blindly.
- `Bloom/CustomComponents/inc/core/AnimationQueuePostUpdate.hpp` — shared queue hook; **avoid creating duplicate hooks**.
- `Bloom/CustomComponents/inc/core/HookRegistry.hpp` — hook conflict detection and safety.
- `Bloom/CustomComponents/inc/core/ManagedRuntime.hpp` — rooted managed reference/lifetime conventions.
- `Bloom/CustomComponents/inc/core/SceneViewContext.hpp` — frontend scene identity; insufficient as sole authority classifier.
- `Bloom/CustomComponents/inc/zones/DiscardFromDeckSystem.hpp` — stock discard delegation.
- `Bloom/CustomComponents/inc/zones/detail/DiscardToDeck.inl` — stock type-74 record + provenance.
- `Bloom/CustomComponents/inc/zones/ReturnDiscardToHandSystem.hpp` — synthetic DrawCardRecord and return semantics.
- `Bloom/CustomComponents/inc/zones/detail/LaneTransferExecution.inl` — stock lane records and physical-card transfer rules.
- `Bloom/CustomComponents/inc/zones/MoveCardInDeckSystem.hpp` — deck reorder record.
- `Bloom/CustomComponents/inc/pick3/PickedFromThreeSelection.hpp`, `inc/pick3/submission/**`, `inc/presentation/PickFromThreeDiscoverPresentation.hpp` — popup and selection lifecycle.
- `Bloom/CustomComponents/tests/custom_records_v6_test.cpp` — baseline test to expand, not a runtime safety proof.

### `dump.cs` pointers

- `History`: around line `755520`; `RecordType` / `RecordKey` / `Record`: around `755710–755965`.
- `DiscardFromDeckSystem`: around `660749`; stock record classes around `757037–757450`.
- `HistoryProcessor` / `HistoryProcessorModel` / binder: around `220013–220195`.
- `HistoryFilter`: around `219920`; record-executable factory around `234372`.
- `EffectAnimation` / `EffectAnimationQueue`: around `209173–209465`.
- `DiscardFromPlayRecordProcessor`: around `207792`; `DrawCardRecordProcessor`: around `208827`.
- `GamePredictionHistoryModel` / predictor: around `214998–215335`.
- `TweenCardEffectAnimation`: around `242564`.

### Assembly addresses to recheck before patching

```text
0x22829BC  History.AddRecord(Type)
0x2282A74  History.AddRecord(Record)
0x2282B48  History.RemoveAllRecords()
0x22837C4  Record..ctor()
0x2287348  DiscardFromDeckRecord..ctor()
0x21FF55C  DiscardFromDeckSystem.RecordCardDiscarded()
0x1F709E8  HistoryProcessor.ProcessHistory()
0x1F70A30  HistoryProcessor.ProcessRecords(IList<Record>)
0x1F70B58  HistoryProcessor.GetNextRecord(IList<Record>)
0x1F70C20  HistoryProcessor.TryProcessPredictedRecord(Record)
0x1F70CF8  HistoryProcessor.ProcessRecord(Record)
0x1F70DB0  HistoryProcessorModel.MarkRecordAsProcessed(Record)
0x1F138C4  EffectAnimationQueue.AddAnimation(EffectAnimation)
0x1F13970  EffectAnimationQueue.AddAnimationToFrontOfQueue(EffectAnimation)
0x1F13A48  EffectAnimationQueue.Update()
0x1F0B6A4  DiscardFromPlayRecordProcessor.ProcessRecord(...)
0x1FD141C  TweenCardEffectAnimation.IsCompleted()
```

**Rebuild/version warning:** Validate every resolved method against the actual deployed `libil2cpp.so`; these RVAs are evidence for this particular dumped binary, **not portable hook offsets**.

---

## 18. Final engineering decision

The shortest reliable route to **stock-like custom history with correct discard timing** is:

1. **Keep real stock records** and stock `RecordExecutor` untouched where they already describe the effect correctly.
2. **Upgrade CustomRecords into CustomHistory**, with typed committed records, per-model epochs, explicit presentation delivery and no silent record loss.
3. **Merge its frontend dispatch into the stock history loop** at a verified before-next-record boundary, including predicted/already-processed cases and the final tail.
4. **Enqueue real stock `EffectAnimation` instances at that boundary** and acknowledge completion through their actual lifecycle, not whole-queue idleness.
5. **Make hand-to-discard the first complete implementation**, then migrate every remaining mutating CustomComponents path against the ledger.
6. Only after this is stable, add a **read-only optional subscriber** for MatchRecorder.

A fully managed `Record` subclass inserted into stock `History` is an interesting **separate research track**, not an assumption. Until pooling, `RecordExecutableFactory`, binder registration, prediction, cleanup and PvP serialization are proven, the **in-band native CustomHistory + stock queue** architecture is the recommended production path.

**Implementation status at document delivery:** researched and planned only. No runtime hook, source file, or Android APK was changed by this deliverable.
