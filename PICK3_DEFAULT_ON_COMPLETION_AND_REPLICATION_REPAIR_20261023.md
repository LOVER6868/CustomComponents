# PickFromThree: default-on PvE/PvP completion and received-play reconstruction

## Based on

The 2026-10-23 `Pick3_full_source_connector_effect_phase_trace_20261023.zip` source, including the existing ADB-controlled network/effect tracer, local blocking-effect preflight, stock command lifecycle diagnostics and the UI picker.

The latest PvP trace confirms the client-authoritative `SelectTarget` Play reaches serialization, the connector send and successful HTTP post callback, and that the matching local effect/group leaves the engine's observed effect queue. It does not demonstrate other-client gameplay acceptance or that a lingering visual subturn is permanent.

## Gameplay changes — ON by default (not controlled by ADB)

### 1. Game-owned, deferred completion notification

The custom popup does not create a stock `SelectTargetCommand`, so it usually has no stock success handler or retained command. On service success, with no previously attached handler/owned command, `FinishTransaction` now registers a pending refresh for **both** local and remote selection paths. This DOES NOT immediately dispatch a signal, call `EndSubTurn`, release a command, or resend the Play.

`GameInputStateHolder`'s existing per-update observer resolves `GameStateUpdatedSignal` and `EffectCollection` through the current match's `CommandBinder` injector. The refresh is dispatched **at most once**, and only after:

- the matching effect entity **and group** are absent from all three lists (`Effects`, `ResponseEffects`, `GroupResponseEffects`) of the bound `EffectQueue`;
- another `ChooseTargetBlockingEffect` is not at the front of those lists;
- the local visual target subturn is still active (`SubTurn == 1`) for at least two observations;
- no stock `SelectTargetCommand.HandleServiceSuccess` executed in the meantime;
- a valid, match-injected `GameStateUpdatedSignal` exists.

If the game's normal subturn update already returns to non-targeting, the pending refresh cancels. If it cannot be verified within 600 observer calls, it expires without fabricating completion. A changed match binder drops the old dependencies and cancels the pending refresh.

### 2. Receive-side PickedFromThree cache reconstruction

The original custom picker calls `Picked::Store()` only on the selecting client. A different simulation does not see that C++ memory. `LocalSelectTargetService.Process(Play)` now has a wrapper that **forwards the original exactly once** and, before running it, reconstructs the selection when the play:

- has stock `Type=6` (`SelectTarget`);
- has a genuine `ChooseTargetBlockingEffect` at that service's effect-collection front;
- has the same `PlayerId` as that blocking effect's `PlayerId`;
- belongs to a live `PickFromThree` / `UniqueGuidTargets` filter (3/4), including an exact effect/group runtime Manual override;
- contains one to three entity IDs resolvable to positive card GUIDs using **the service's injected TargetEntityFinder/EntityFinder.Model**, not a different UI or card-database model.

The reconstruction writes only `Picked::Store(effect, group, {entityId, cardGuid})` in local C++ memory. It does **not** rewrite incoming Play data, skip validation, create an effect, fabricate network messages, or call the service twice. The game's existing `LocalSelectTargetService.Process` remains the authority for play validity.

GUID-only `CardDatabase` mode (5) is intentionally excluded: the stock play has no GUID field and the original source rejects PvP DB5 submission. The repair must not misinterpret a proxy recipient as the chosen database card.

## ADB — tracing ONLY

The prior diagnostic modes are retained without modification:

```powershell
# Off — gameplay repairs remain active
adb shell "echo 0 > /sdcard/Android/data/com.ea.gp.pvzheroes/files/pick3_net_trace.txt"
# Pick3 pipeline
adb shell "echo 1 > /sdcard/Android/data/com.ea.gp.pvzheroes/files/pick3_net_trace.txt"
# Pick3 plus stock selections (recommended test)
adb shell "echo 2 > /sdcard/Android/data/com.ea.gp.pvzheroes/files/pick3_net_trace.txt"

adb logcat -c
adb logcat -v time -s Sprig:V > pick3_trace_log.txt
```

New `DEFAULT_REPAIR` log messages (emitted on repair events independently of trace mode):

- `DEFAULT_REPAIR: incoming=1 ...` — the receive-side wrapper hooked successfully
- `DEFAULT_REPAIR: PICK_RECONSTRUCT effect=... group=...` — service-model entity/GUID cache reconstruction actually ran
- `DEFAULT_REPAIR: REFRESH_PENDING ...` — deferred local presentation repair queued
- `DEFAULT_REPAIR: GAME_STATE_REFRESH_SENT ...` — exact guarded signal was dispatched once
- `DEFAULT_REPAIR: REFRESH_CANCELED_STOCK_COMMAND` — stock command finished instead, so no synthetic duplicate
- `DEFAULT_REPAIR: REFRESH_EXPIRED ...` — safe conditions not reached; no signal dispatched

## Validation and limits

- Clang 17 C++20 **host syntax check passed** for `PickFromThreeSubmission.hpp` and transitive BNM headers with the supplied BNM/Dobby include trees. ZIP integrity checks passed.
- **Not Android-built or device-tested.** The new hooks and signal/queue binding may be unavailable or behave differently in a match. Check the startup repair flags and new event records after installing.
- Existing gameplay behavior unrelated to these two repair paths is preserved. The network sender/serializer, selected `Play` payload, stock validation and UI card sampling are unchanged.
- Multiplayer depends on both simulations understanding custom definitions and abilities. This cannot force an unmodified server or peer to understand them, and HTTP transport success alone is not authoritative gameplay acceptance.
- These are targeted repairs, not certification that every custom card is fixed. In particular, PvP CardDatabase GUID-only choices remain unsupported by the stock SelectTarget payload, and any outstanding Fisherman effect-activation issue must be tested separately.
