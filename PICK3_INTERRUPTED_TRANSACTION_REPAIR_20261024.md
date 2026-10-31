# Interrupted PickFromThree transaction repair — experimental (2026-10-24)

Base: `Pick3_Combined_PvP_Button_FirstTap_full_20261024.zip` (the button-working build, not an older rollback).

## Why this is needed

Capture `PvZHeroes_Pick3_Focused2.txt`, 19:23:39–19:23:49:
- Relic Zombie/Relic Hunter picker source GUID 710; effect **16**, group **392**.
- Finder predicted 16/392 and the popup activated.
- Game briefly left the targeting state before returning to state 6 for the **same effect/group**.
- The old local-only stale-popup watchdog called `Close()` which automatically executed `FinishTransaction(false)` on a *never-submitted* selection.
- `completedOffers` already contained 16/392 because activation itself reserves this key; failure cleanup kept it there. The same effect could never reopen, even though its stock blocking effect returned.
- Old OneShotFirstPickRebind logged `refreshed=1` but failed to recreate this request. It could also interfere with other stock command binding.

## Changes

1. `inc/pick3/PickFromThreeTargetBridge.hpp`: new `SuspendUnsubmittedOffer` preserves EXACT offered options, eligibility IDs/GUIDs, database recipient and owner from a displayed *but not submitted* transaction. It deduplicates speculative clones of that effect/group, restores the scoped manual filter override, and clears its `completedOffers` reservation **only after a pre-dispatch interruption**. Success and uncertain network outcomes are still kept consumed. Predictive cache entries expire after 180 seconds, suspended ones after 60 seconds to avoid indefinitely resurrecting old effects.
2. `inc/presentation/PickFromThreePreviewSystem.hpp`: the existing *PvE/local* stale-holder watchdog now suspends, rather than failing, an unsubmitted picker. A `!picked.Sealed()` gate ensures sealed or in-flight submissions are NEVER replayed. The UI closes cleanly without calling `FinishTransaction(false)` after successful suspension. An interruption in PvP does not enable a new watchdog that wasn't in the original build; remote handling is otherwise unchanged. Reopening requires the stock holder to identify exactly the same effect/group, and validates suspended card IDs/GUIDs in the active model.
3. `main.cpp`, `config.ini`: retire ineffective stock command rebind; `OneShotFirstPickRebind=false` remains a documented no-op. The successful `GuardedStockButtonRecovery=true` stays active and its code is byte-for-byte unchanged. New `SuspendInterruptedPick3=true` rollback switch.

## What isn't changed

- `TargetButtonTrace.hpp`, `PhaseRecordTrace.hpp`, `PickFromThreeSubmission.hpp` are unchanged.
- Native avatar rendering, PlayTrigger/ReturnToLane, deck pool and PvP transport are untouched.
- The *true* initial no-cache case (stock holder starts targeting without any prior finder evaluation) is not solved. This patch addresses cases where a valid prior offer was lost when the effect temporarily left state 6/7. It does not call target-finding from a timer or rebind game commands.

## Opt-out / rollback

```
[Pick3LifecycleRepair]
OneShotFirstPickRebind=false
GuardedStockButtonRecovery=true
SuspendInterruptedPick3=true
```

Set `SuspendInterruptedPick3=false` to restore the original failed-transaction behavior of the stale-holder watchdog **without disabling the button recovery**. Restore the previous combined full ZIP for full rollback.

## Testing (device verification mandatory)

1. PvE: let opponent play an environment with Relic Hunter/Relic Zombie triggering a three-card selection. Look for `HANDOFF_REPAIR: SUSPENDED effect=...`, followed (if the SAME effect reappears) by `IDENTITY: ACTIVATE ... resumed=1`; select one card and verify `PICK3_FINISH_TRANSACTION resultCode=0` occurs **once**.
2. PvE: ordinary single and two-card PickFromThree activations, followed by Surprise/Done. Ensure button resumes normally.
3. PvP: first picker, opponent-owned effect, consecutive effects and late scene transition. Verify no opponent menu appears locally and no network selection is dispatched twice.
4. Cold-start: `NO_POPUP: HOLDER_TARGET_WITHOUT_REQUEST` means there was NO cached offer at all. This requires separate stock lifecycle inspection; no native retry is attempted.
5. Inspect `HANDOFF_REPAIR: SUSPEND_REJECTED`, `IDENTITY: ACTIVE_NO_VALID_CACHE` and `HANDOFF_REPAIR: CACHE_EXPIRED` for cases we couldn't safely reconstruct.

Capture example in PowerShell:
```
adb logcat -c
adb logcat -b main,crash -v threadtime -s Sprig:V Unity:E AndroidRuntime:E DEBUG:E libc:E '*:S' > "$HOME\Downloads\PvZHeroes_Pick3_InterruptedRepair.txt"
```
Stop with Ctrl+C AFTER gameplay. 

## Verification performed here

Host Clang C++20 syntax-only compile of current `main.cpp` including project BNM/SPRIG headers; used stub Android log/JNI headers (no NDK linker and no Android runtime). Static checks confirm the stock button tracer, phase recorder and submission source are byte-identical to the user-tested combined ZIP. Two ZIP integrity tests. **This is not a confirmed on-device fix**.
