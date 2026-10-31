# First-picker holder wake V2 (experimental, 2026-10-24)

## Why V1 did not resolve the issue

The capture `PvZHeroes_logswewant(2).txt` repeated the first-picker problem:
- 21:15:23.006 — active local targeting, effect 52/group 178 in state 6.
- 21:15:23.010 — stock-live options cached for that exact effect/group.
- 21:15:50.089–.091 — stock SelectTarget command executes/fails.
- 21:15:50.106 — popup identity activation, following the failed command.
There were no `FIRST_TAP: HOLDER_DRIVE` logs in the capture. Initialization occurred before the capture started, so we cannot prove whether its observer registered, or which early-return gate prevented V1 from running.

## What V2 does

- Makes the existing holder-update observer register if either Unity frameCount OR realtimeSinceStartup exists (instead of frameCount only). If neither exists it stays disabled safely.
- The callback triggers a wake-up whenever the real active stock `ChooseTargetBlockingEffect` is in state 6/7 and an unconsumed picker offer exists somewhere in the cache, or a request is already queued/active. The callback does NOT require that its DI model or owner has resolved yet; that was a circular precondition since `Tick()` is responsible for resolving the model and publishing precisely matched offers.
- Leaves the exact-match/presentation gates inside the original `Tick` and `TargetBridge::PublishForActiveEffect`: effect/group, model pointer, local owner, selection mode, stock target validity and subscription still have to match.
- Uses Unity frameCount to deduplicate frontend and holder callbacks, with a bounded realtime fallback when frameCount cannot be invoked.
- Adds readable markers for setup and wake decisions. No extra finder, command, phase resume, card materialization, or wire submission.

## Config / rollback

`[Pick3LifecycleRepair] DriveFirstPickerFromHolder=true`

Set to `false` to stop this V2 experimental callback. Leave `GuardedStockButtonRecovery=true`, `SuspendInterruptedPick3=true`, `OneShotFirstPickRebind=false` as before.

## What to watch in the next log

- `FIRST_TAP_V2: HOLDER_FALLBACK enabled=1 ...` should appear at initialization. If absent, verify rebuilt APK contains this ZIP and capture starts before launching the game.
- `FIRST_TAP_V2: HOLDER_WAKE effect=...` before any tap means the holder observer is operating. Its `binderModel` and `exactCache` fields identify dependency/identity readiness.
- `FIRST_TAP_V2: HOLDER_NO_OFFER` means the holder is active with no queued or cached offer, not that it is definitely PickFromThree.
- `IDENTITY: ACTIVATE` should immediately follow once `Tick()` successfully publishes the exact request. A wake without activation narrows the failure to the later `Tick()` gates.

## Out of scope

The DEK1 protocol files and read-only deck model audit are preserved unchanged. PvP virtual deck entries are still NOT valid stock entity targets, and DEK1 receiver validation does not itself provide an executing receiver. Deck selection transport remains unresolved and deliberately disabled.

## Validation

Host C++20 syntax check: passed with available BNM, Sprig, Dobby headers and Android host shim.
Existing deck regression tests: passed. Python reference receiver tests (7): passed.
No real Android build/test run. Re-test first picker, Relic Hunter and the Surprise button.
