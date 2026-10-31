# PickFromThree first-popup holder drive + physical deck transport milestone

Baseline: `Pick3_FirstTap_DeckBoundary_Audit_full_20261024.zip`.

## Gameplay change: first popup local holder drive

The 2026-10-24 21:15 log observed the stock holder enter state 6 for effect 52 / group 178 at 21:15:23.005 and cache the correct stock-live offer by 21:15:23.010. The custom UI's EventSystem.Update-driven `CLASSIFY_HOLDER` and `IDENTITY: ACTIVATE` did not run until 21:15:50.106, immediately after a failed stock target command. An unrelated card tap can cause this wakeup.

New behavior (opt-in, default enabled): `TargetingPresentationRefreshSystem::UpdateHook` already calls `Submission::GameInputHolderUpdated()` after stock Update. A **second registered observer** now calls the same `PickFromThreePreviewSystem::Tick()` from this already-running stock holder callback **only when**:

- the holder has a current 6/7 `ChooseTargetBlockingEffect`;
- a local PvE or locally-owned PvP picker is enabled;
- an offer for that exact effect/group and the live, rooted model is cached, or a popup already exists;
- Unity's `Time.frameCount` method is available (if not, fallback is disabled);
- UI tick has not already run in the same Unity frame.

The popup still calls the **existing** `PublishForActiveEffect()` (validates exact effect/group/model, filter mode, owner, and suspended-card validity) and stock selection service; there is no native finder retry, queue manipulation, new command submission, or other targeting policy change.

Two UI tick sources, EventSystem.Update and GameInputStateHolder.Update, are deduplicated on actual Unity frame count. A reentry guard protects the tick. The existing EventSystem.Update callback is retained for menus and match transitions. The existing Surprise-button recovery and Relic Hunter suspended-request handling are unchanged.

**Config rollback:**

```ini
[Pick3LifecycleRepair]
DriveFirstPickerFromHolder=true
GuardedStockButtonRecovery=true
SuspendInterruptedPick3=true
OneShotFirstPickRebind=false
```

Set `DriveFirstPickerFromHolder=false` if this experimental popup drive causes a regression. Trace markers: `FIRST_TAP: HOLDER_FALLBACK` at initialization, followed by `FIRST_TAP: HOLDER_DRIVE` during the first eligible active holder and `IDENTITY: ACTIVATE`. You should not need a failed stock target click before that activation. Android runtime not tested here.

## Deck issue: real PvP physical entries are virtual

New `DECK_TARGET_POOL` narrows the diagnostic to the deck *actually matched by stock candidates*, not the sum of both players' main decks, supers and temporary pools.

The prior log for effect 82 / group 632 reported `69` physical entries, `67` virtual and only `2` stock eligible **across all pools**. Its candidate-matched deck (entity `1`) actually had **29 entries, 2 live IDs and 27 virtual**. Other pools must not be included in the target deck's deficit.

No change converts a virtual entry (`entityId=-1`) to an ordinary `Play.Targets` entity ID. Doing so would be a protocol bug. The `DeckEntryWireIdentity.hpp` / `DeckEntryExecutionBridge.hpp` foundation already describes DEK1 physical entry identities; this update includes a tested independent Python decoding and validation reference under `tools/` for use by an eventual receiving simulator implementation. It is **NOT** a registered Flask endpoint or usable gameplay transport yet.

Checked public `mhdo298/pvzh-server/src/pvp.py`: its `/pvp/v1/pvpSendUpdate` route relays the POST JSON via Redis. It is not an authoritative C++/IL2CPP game simulation and cannot create the missing card entities on clients. Actual full deck support requires a client-side transport for owner/pool/index/GUID/fingerprint, receiving-side authenticated owner mapping, stock-equivalent entity materialization in that *receiver's* executing model, and matching action semantics on **both** multiplayer clients (plus compatibility/version checks). A server relay could transport a new envelope, but neither stock clients nor current mod receivers know how to act on it. This is the remaining block; **not fixed by this ZIP**.

The reference Python implementation `tools/deck_wire_receiver_reference.py` matches C++ DEK1's exact nine-word encoding and little-endian FNV-1a GUID-pool fingerprint. A C++/Python test vector agrees word-for-word. Tests reject wrong owners, altered pool order, malformed identities and ambiguous IDs, and explicitly return `requires_materialization` rather than inventing a card.

## Host-only validation

- Main C++20 source syntax: passed using local Sprig/BNM/Dobby headers and compatibility stubs.
- DeckEntryResolverRegression.cpp: passed.
- DeckEntryExecutionBridgeRegression.cpp: passed.
- DeckEntryWireIdentityRegression.cpp: passed.
- Python reference receiver: 7 tests passed.
- C++/Python DEK1 cross-language sample: identical nine-word packet.
- Android build/runtime: **not performed**. PvE/PvP gameplay tests are still required.

## Suggested field test

1. Back up working `CustomComponents`. Apply new full ZIP.
2. Confirm initialization logs `FIRST_TAP: HOLDER_FALLBACK enabled=1 clock=1`.
3. In a fresh PvP game, trigger the first custom PickFromThree **without touching another card**. Check for `FIRST_TAP: HOLDER_DRIVE` then `IDENTITY: ACTIVATE` shortly after `CLASSIFY_HOLDER`/`CACHED`. If none, capture the period from stock state 6 to any manual tap.
4. Re-test two Relic Hunter triggers, two Surprise effects, and PvE targeting.
5. For the separate deck problem, read `DECK_TARGET_POOL` and do not enable virtual selection yet.
