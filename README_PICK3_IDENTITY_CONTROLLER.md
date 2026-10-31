# PickFromThree effect-identity controller (experimental, 2026-10-24)

Base: Pick3_Crash_SafeRollback_full_20261024.zip. C++20 syntax-checked; **NOT Android tested**.

Modified files (four):
- `inc/pick3/PickFromThreeTargetBridge.hpp`
- `inc/pick3/PickFromThreeSubmission.hpp`
- `inc/presentation/PickFromThreePreviewSystem.hpp`
- `inc/presentation/TargetingPresentationRefreshSystem.hpp`

## What changes
1. Stock target-finder evaluations cache candidate offers by effect/group and backing EntityModel, rather than immediately publishing `nextRequest` or overwriting active `latestEligible` snapshots.
2. After stock GameInputStateHolder.Update, its observed current ChooseTargetBlockingEffect identity decides which cached offer can own the UI. A same-state 6-to-6 effect change is distinguished by effect/group. Predictions cannot open UI early.
3. Prediction clones are only reused when offered card entity IDs and GUIDs resolve in the actual injected EntityModel. **NEVER pass PvZCards.Core.Utilities.GameModel to EntityModel.GetEntityById.** This distinction caused an earlier native crash.
4. The active holder identity, not EffectQueue's divergent private-list snapshot, gates popup display. Native LocalSelectTargetService.Process remains final authority over selection success.
5. Removing UI's 3600-frame speculative request deletion avoids clearing requests before targeting. Active popup cleanup also checks the holder's current effect identity.
6. No direct timer-driven FindValidTargetsFromEffect calls, no forced EndTurnButton FSM calls, no changes to native avatars, PlayTrigger, or ReturnToLane.

## New log markers
- `Pick3/IDENTITY: CACHED` – offer recorded, not presented.
- `Pick3/IDENTITY: ACTIVATE` – current stock holder agrees; request published. `verifiedClone=1` means a prediction copy passed a live entity/GUID check.
- `Pick3/IDENTITY: ACTIVE_NO_VALID_CACHE` – stock holder requires this picker but no validated cached option exists. **Known remaining edge case**: this build logs the miss, rather than injecting a risky finder call.
- `Pick3/IDENTITY: ACTIVATE_FILTER_UNAVAILABLE` – no rooted active-game filter; popup stays closed.
- `Pick3/IDENTITY: STOCK_HOLDER_SUBMISSION` – holder agrees with request though inspected queue lists disagree; real stock service still validates.

## Tests on device
1. Single PickFromThree: no popup before targeting state 6/7; popup should open after active group matches. Select and confirm stock selection completes.
2. Two consecutive PickFromThree activations: inspect `IDENTITY: ACTIVATE` for each different group without a board tap. Both should complete.
3. NumTargets=2 card: popup must allow selecting two cards and accept only when the stock service confirms.
4. Surprise/Done button: stock phase restore still decides; verify after final choice. No artificial resume invoked.
5. Inspect `ACTIVE_NO_VALID_CACHE` or stock service failures: this means this build is NOT a full fix and needs a validated current-finder activation path.

### Capture
```
adb logcat -c; adb logcat -b all -v threadtime > "$HOME\Downloads\PvZHeroes_IdentityController.txt"
```
Run in PowerShell; stop with Ctrl+C after reproducing.

## Validation boundary
This code has **passed Clang C++20 syntax checking only**, not NDK link, APK rebuild, device runtime, or PvP verification. Never treat the button or two-card issue as proven fixed until observed on a device. If the experience worsens, use the provided crash-safe rollback ZIP.
