# Combined PickFromThree lifecycle experiment (2026-10-24)

Source baseline: `CustomComponents(20261024-224808).zip` from the user. No unrelated card/deck/trigger/ReturnToLane/native-avatar source changed. Full ZIP deliberately excludes the source folder's `.git` database; overlay these source files into the existing folder, do NOT delete the existing checkout or its `.git` directory.

## Direct changes

1. `inc/presentation/PickFromThreePreviewSystem.hpp`
   - Previous conservative first-frame check and `enterSeconds <= 0.16` remain.
   - Per-card reveal changed 340 ms + 75 ms stagger to 260 ms + 50 ms stagger. Three-card mandatory reveal limit changes from roughly **502 ms to 372 ms**, not including Unity object creation. UI input still waits until animation completes. Does not bypass the stock targeting holder or network protocol.
   - A once-per-match cold-start recovery is triggered only after **eight consecutive live targeting frames** with current holder state 6/7, *actual locally owned* PickFromThree filter, rooted live EntityModel, available stock service, no queued request and no pending offer for that live model/effect/group. It uses the stock `GameInputStateHolder` command rebind+state-change dispatch (a method already used by this mod for 6->6 handoffs) and **never calls FindValidTargetsFromCurrentEffect or FindValidTargetsFromEffect directly**. If rebind doesn't cause the normal finder to run, it logs the attempt and does not retry. This is an experiment, not guaranteed to fix the first-tap bug.
   - Runs a guarded button reconciliation when no popup or active targeting effect owns the UI.
2. `inc/pick3/PickFromThreeTargetBridge.hpp`: read-only helper verifying whether a **current model's exact effect/group** already has cached offers; read-only helper verifying the live PickFromThree component. No change to prediction cache, transaction selection, network serialization, or deck targeting behavior.
3. `inc/pick3/TargetButtonTrace.hpp`: after a stock `ChooseTargetStart`, look for a later real `ProcessChooseTargetPhaseEndRecord`. If the stock **resume record has not been scheduled** for this interruption, and the stock holder has stayed in ordinary input state **3/4** (never 5, 6 or 7) for ten frames, visual subturn is 0, stock phase-care predicate is true, game-owned button FSM is exactly **Interrupted**, and no custom picker is present, call the **stock** `EndTurnButtonViewMediator.ReturnToRegularPhase` trampoline *once*. This schedules stock button animation. It does not write FSM state, forge gameplay records, or send PvP plays. A normally queued resume record prevents this fallback. Original native hooks still call their stock originals once per invocation.
4. `main.cpp` and `config.ini`: independent reversible switches; both default true for this experimental build. Set either to false and rebuild to disable it:
   ```ini
   [Pick3LifecycleRepair]
   OneShotFirstPickRebind=true
   GuardedStockButtonRecovery=true
   ```

## Evidence from previous on-device capture

`PvZHeroes_PvP_Button_RecordTrace(3).txt` (2026-10-24): effect `17/675` reached holder targeting state 6 around **18:08:59.350** but logged `IDENTITY: ACTIVE_NO_VALID_CACHE` at **18:08:59.563**, preceding a manual target tap. This is the candidate for the bounded cold-start rebinding.

At **18:07:54.732-.756**, the second manual targeting lifecycle processed choose-target end records; visual subturn left targeting. At **18:07:57.116**, the holder was state 5, then at **18:07:57.316** state 3 with `subturn=0`, `machineState=3 (Interrupted)` and `phaseCare=1`. No stock resume record was scheduled during this eligible interval. The guarded stock button path targets state 3/4, not 5/6/7.

## New log markers

- `Pick3/LIFECYCLE_REPAIR` (startup flags)
- `Pick3/COLD_START: STOCK_BIND_REFRESH` and `COLD_START: RESULT`
- `BUTTON_RECOVERY: STOCK_MEDIATOR_RECONCILE`
- Existing `PHASE_RECORD`, `IDENTITY:`, `SUBMIT: RESULT`, `NO_POPUP`, `RESUME_TRACE`, `STATE_TRACE`

## Validation and limitations

- Clang C++20 host syntax check passed with extracted project BNM/Sprig headers and Android-log compatibility stub.
- Android NDK link, APK installation and actual PvE/PvP runtime are **NOT** tested here. This is a reversible experimental patch, NOT a proven gameplay fix.
- Stock input holder still owns the legal target-selection boundary. The patch speeds up the custom card reveal, but **does not fully decouple opening from opponent/shared animation queue readiness**; opening before the game is ready previously caused softlocks.
- Stock button recovery can add an animation if its resume record is generated much later; this is mitigated by one-shot, local stock-state, phase-care, and no-record guards, but remains an experimental change.
- The virtual deck/PvP transport problem was deliberately left unchanged.

## Recommended tests

First, test one PvE PickFromThree and a two-card PickFromThree. Then test PvP first popup before tapping anywhere, a second popup, and consecutive Surprise-phase manual effects. If any new regression occurs, disable both switches (or restore the uploaded source ZIP) and reproduce separately. Inspect relevant markers above in `adb logcat`.
