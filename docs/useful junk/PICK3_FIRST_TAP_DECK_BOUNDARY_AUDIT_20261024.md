# First PickFromThree and PvP deck-model boundary audit

Based on `Pick3_InterruptedOwnershipRepair_full_20261024.zip` (Relic Hunter and Surprise button working), with only two existing headers modified:
- `inc/pick3/PickFromThreeTargetBridge.hpp`
- `inc/presentation/PickFromThreePreviewSystem.hpp`

**This is a diagnostic build, not a claimed fix for the first-tap or multiplayer virtual-card transport issues.** The last working button recovery, all selection submission, avatar rendering, and interrupted-popup suspension code are unchanged.

## Why this diagnostic is necessary

2026-10-24 20:51:19: first observed custom UniqueGuidTargets PickFromThree `17/202` got `BEFORE_STOCK` -> `AFTER_STOCK` (4 resolved) -> `IDENTITY: CACHED` -> `IDENTITY: ACTIVATE` and later successful stock submission. This alone cannot establish whether a user tap triggered the first finder call; the log has no correlated physical tap event.

2026-10-24 20:54:18, 20:56:30 and 20:57:10: holder entered state 6 for `106/864`, `106/1253`, `106/1392` respectively, and logged `ACTIVE_NO_VALID_CACHE`. But the old controller tried to publish EVERY state-6 effect, including stock `TargetSelectionType=Manual`. Without reading the actual live filter mode, a no-cache log is NOT evidence that a custom PickFromThree has failed. This patch logs `CLASSIFY_HOLDER` and suppresses the misleading cache miss for a verified stock filter.

2026-10-24 20:51:19, effect `17/202`, the finder model contained 77 ordered card entries, 7 live IDs and 70 virtual entries. A later queried match model at 21:00:12 had 68 entries and 61 live IDs. These are not interchangeable model snapshots. The missing entries cannot be sent as stock `SelectTarget` EntityIds. The patch logs separate `DECK_MODEL_BOUNDARY` counts by ordered-pool-bearing entity and exact initializer-model identity.

## New log markers

* `Pick3/CLASSIFY_HOLDER: STOCK_TARGET`: state 6 belongs to stock targeting, not a custom picker; do not create popup.
* `Pick3/CLASSIFY_HOLDER: CUSTOM_OR_UNKNOWN`: includes `mode` (3=PickFromThree, 4=UniqueGuid, 5=CardDatabase, negative=unresolved) and `hasMatchingCache` for the exact holder effect/group/model.
* `Pick3/IDENTITY: ACTIVE_NO_VALID_CACHE`: should now only appear for custom/unresolved modes.
* `Pick3/DECK_MODEL_BOUNDARY`: includes finder `model`, initializer `initializerModel`, `sameFactoryModel`, remote service status, ordered-deck entity ID, real/virtual/stale entries, stock matched count. This is purely observational.

## What is intentionally NOT changed

* No `FindValidTargetsFromCurrentEffect` synthetic invocation, fake card tap or command binder refresh: earlier attempts caused native crashes or failed to create cache.
* No virtual entities allocated in prediction clones.
* No changes to stock multiplayer `Play.Targets`, `CardId` or serializer and no invented pool-entry IDs.
* No changes to stock button recovery, target completion, Relic Hunter suspension, global PlayTrigger or ReturnToLane.

## Next safe implementation boundary

To fix no-tap activation, trace real first user tap against the game’s injected TargetEntityFinder and state-6 holder; only after verifying *which* valid custom filter failed to produce a stock call should we implement the same stock target-discovery pathway with correct dependency injection and pooled-result lifetime. Never repeat the old timed arbitrary finder call.

To fix virtual-deck selection in PvP, a receiver-authenticated physical deck entry intent (owner, pool, ordinal/index, GUID, fingerprint) must be supported by both clients and server/executing simulation; validation must materialize/resolve it to a real entity via the stock deck system. Existing `DeckEntryWireIdentity.hpp` and `DeckEntryExecutionBridge.hpp` are tested foundations only and MUST NOT be packed into stock `SelectTarget` target slots. Where exact-model local materialization works, report it separately from cloned/remote models.

## Validation

- Clang C++20 syntax-only compile of `CustomComponents/main.cpp` passed with project dependencies and Android log/signals compatibility headers.
- Native host regression passed: `DeckEntryResolverRegression`, `DeckEntryExecutionBridgeRegression`, `DeckEntryWireIdentityRegression`, `Pick3PhysicalCopiesRegression`.
- Not rebuilt into Android APK; no runtime playtesting in this environment.
