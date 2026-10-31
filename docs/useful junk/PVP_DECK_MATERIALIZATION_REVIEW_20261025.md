# PvP Deck Materialization — Oct 25 investigation (safe additive foundation)

## Scope and evidence

Focus exclusively on virtual and stale physical deck entries used by the custom `PickFromThreeUnique` selection, not the already-working first-open UI. The full uploaded logs were UTF-16. This source archive contains `CustomComponents` but not the implementation of the separately logged `SuperpowerEditor` PvP construct patch.

In the supplied 2026-10-25 log, there are **424** `DECK_MODEL_BOUNDARY` observations: 380 `sameFactoryModel=0` and 44 `sameFactoryModel=1`. All print `remote=0`; the provided log does **not** establish two-client PvP convergence. Among **106** `DECK_TARGET_POOL` records, selected pools have `selectedPoolVirtual=0` in every case, although 95 observations report 2–4 stale positive IDs. Do not confuse unrelated aggregate pool `virtualCopies` with virtual entries in the selected deck.

Earlier Oct 24 sessions recorded large virtual physical pools and as few as 2–4 live eligible PvP targets. The existing DEK1 protocol foundation can identify copies but cannot transport a target through stock `Play.Targets` (entity IDs only).

## Safe changes made in this additive branch

- `inc/queries/DeckEntryWireIdentity.hpp`: added `DeckWire::Status::StaleEntity` **at the end of the enum** and opt-in `ValidateReceiverWithLiveCheck(snapshot, intent, owner, verifier)`.
- This helper first uses the unchanged stock-independent `ValidateReceiver` identity checks; only `ReadyEntity` requires a caller-supplied read-only check that the positive ID resolves to the correct **live Card.Guid** with the correct authenticated owner in the exact executing model.
- If the verification fails, it returns `StaleEntity` and entity ID `-1`. It does not rematerialize stale IDs or treat them like virtual entries.
- Added regression assertions for valid cross-model IDs, stale positive IDs, virtual copies and wrong-owner fail-closed behavior.
- **No existing hook, Play service, PickFromThree lifecycle, stock materializer, or network command was changed.** The new optional helper is NOT connected to production callbacks.

## Exact missing pieces to complete PvP virtual-copy execution

1. At the active selection boundary, capture one physical `OrderedCardPool` entry (owner, pool ordinal, index, GUID, GUID-only pool fingerprint), not just a `TargetEntityFinder` live entity ID. Ensure the operation refers to the actual player/deck and not a pooled/prediction model.
2. Implement a dedicated versioned multiplayer selection intent (DEK1 foundation already exists) tied to match, effect/group, selecting player, offer revision/nonce and a replay-resistant action lifecycle. **Never pack a pool index or GUID into stock `Play.Targets` or `Play.CardId`.** The server/relay and both modded peers must support routing/ordering this intent.
3. At each receiver's *executing simulation* after independently authenticating the owner, capture a fresh deck snapshot and run `DeckWire::ValidateReceiver`. Use `ValidateReceiverWithLiveCheck` to verify positive entity IDs, including GUID and correct owner, against the live `EntityModel`.
4. If `RequiresMaterialization` and the verified entry is actually `EntityId=-1`, invoke the game's stock per-entry creation path using the correctly injected `PrepopulateCardPoolSystem` / `CardEntityFactory` *bound to this exact model*; verify the correct pool element type/owner, proper components and triggered abilities. The dump contains `PrepopulateCardPoolSystem.CreateCardIfNotAlreadyCreated(CardPoolEntry, Type)` (RVA `0x21EBA20`) and `PooledCardRetriever.CreateNextCard(CardPoolEntry)` (RVA `0x220AE60`), but **neither has been confirmed safe to call out of context**. `PooledCardRetriever.GetNextCard()` changes the pool by drawing and is not an arbitrary-index materializer. Verify actual call context before using either.
5. Re-read the entry after creation and confirm GUID/owner/physical identity, positive unique live ID, and no unexpected deck shuffle. Fail closed on stale positive IDs, model mismatch, pool change, duplicates or no legal factory; do not retry an uncertain mutation.
6. Resolve each receiver's local entity ID and perform stock selection as an atomic, ordered custom action, with matching execution and acknowledgment on both clients. Avoid a second stock `SelectTarget` submission. Handle timeout/replay, rejection and state resync.
7. Test two peer clients, including duplicate GUIDs, multiple identical copies, temporary pools, cards drawn/removed between offer and commit, mulligan, model clones, prediction rollback, latency and repeated picks.

## Boundaries to preserve

- `PickFromThreePreviewSystem.hpp` and working first-picker service binding.
- `PickFromThreeTargetBridge.hpp`'s prohibition on remote/query-time bulk materialization.
- `InDeckQuery.Matches` as exact live membership; database GUID query separate.
- Existing immutable snapshot and wire validation code: do not silently turn stale positive IDs into virtual entries.
- Stock `Play.Targets` semantics and existing network paths.
- Current `RemoveExtraCardsInDeckSystem` suppression is global; changing its scope may be desirable eventually but is a **separate risky behavior change** and must not be combined with this receiver fix without independently validating PvE/PvP mulligan behavior.

## Separate log issue

The same Oct 25 log also includes `SuperpowerEditor PVP CONSTRUCT P3 PATCH: rebuild failed; leaving current pool` and `PATCH RESULT: applied=0` (six attempts). This concerns PvP hero/deck construction and is distinct from `CustomComponents` virtual ordered-pool entry execution. `SuperpowerEditor` source is not included in the provided `CustomComponents` archive, so no source-level repair for it is asserted here.

## Local verification

Four C++20 host tests passed (`DeckEntryResolverRegression`, `DeckEntryExecutionBridgeRegression`, `DeckEntryWireIdentityRegression`, `Pick3PhysicalCopiesRegression`); seven Python DEK1 receiver reference tests passed. Android APK compilation, two-client PvP execution and server acknowledgements are **not tested**. No gameplay functionality was enabled by this patch.
