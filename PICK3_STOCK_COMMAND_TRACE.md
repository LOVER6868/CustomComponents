# PickFromThree — read-only stock command lifecycle trace

Based on the previously supplied `CustomComponents_Pick3_input_handoff_diagnostics_20261023.zip`. Only `inc/pick3/PickFromThreeSubmission.hpp` was modified for this test.

## Instrumentation

- Stock `SelectTargetCommand.Execute()` now logs command identity and injected service/signal/retention tracker at entry and return, whether blocked or forwarded.
- `SelectTargetCommand.HandleServiceSuccess()` logs entry and return.
- `SelectTargetCommand.HandleServiceFailure(ServiceFailure)` logs entry and return.
- `HeroesCommand.Release()` logs entry and return **only** when the object is a `SelectTargetCommand`; all calls are forwarded unchanged.
- `STOCK_COMMAND: HOOKS` reports which optional lifecycle probes installed. Successful setup is necessary to interpret absent callbacks.
- Logs are bounded to 160 candidate events per match. The probe does not store managed command references, call `Release()` manually, or modify PvE/PvP signaling.

## Test

Test a regular stock target-selection card once (baseline), then a PickFromThree selection in PvE and in PvP. After each one, attempt ordinary hand-card highlighting and card play. Capture lines containing `STOCK_COMMAND`, `HANDOFF`, `INPUT_HANDOFF`, and `SUBMIT`, including the few seconds following each submission. Separately, for the phone crash capture Android tombstone or `adb logcat -b crash -d` output.

Interpretation: if stock baseline emits `SUCCESS_ENTER` and `RELEASE_ENTER` but custom PvP emits neither, the direct service path bypassed the stock command. If neither emits them, check `HOOKS` before drawing conclusions. The stock command pointer is an identification aid only, not a safe pointer for later invoking methods.

## Validation

Clang++ C++20 `-fsyntax-only` passed against supplied BNM/Sprig/Dobby includes using a host Android-log shim. Not built or runtime-tested on Android. Hooks have runtime risk; keep a backup of your last working build.
