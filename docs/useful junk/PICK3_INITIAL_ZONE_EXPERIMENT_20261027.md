# Deferred Pick3 initial-zone experiment (GUID 712 / 713)

This is an **experimental gameplay change**, not a read-only diagnostic.

## Change

Only while `GameStateUtils.GetPossiblePlayTargetsForBurst` is executing for actual card GUID 712 or 713, the existing `EffectEntityUtils.IsOnPlayTargetedEffect` observer returns `false` instead of its stock `true` result. All other calls return the original result. The secondary manual-lane filter, actual card effect, validator, and submitted `Play` are untouched.

To disable at compile time, set `kDeferredPick3InitialZoneExperiment = false` in `inc/pick3/PickFromThreeSubmission.hpp`, then rebuild.

## Startup and runtime checks

Expect `TRICK_EFFECT_PATH: SETUP ... initialZoneExperiment=1` and when the target calculation occurs, `TRICK_INITIAL_ZONE: phase=OVERRIDE guid=712` or `guid=713`. Compare `TRICK_EFFECT_PATH: phase=BURST_AFTER` to 699's one-time-effect-zone ID. Then check whether `TRICK_ACCEPT` records success and the PickFromThree popup opens. If success, **test whether the secondary lane selector still starts** after selecting a discarded card. This first experiment may resolve visual targets without satisfying server/local validation; don't infer success from highlighted lanes alone.

## Scope/safety

Only GUIDs 712 and 713, only within Burst target-generation call stack. No global `HasTargeting` patch, no validation bypass, no changes to GUID 699. The experimental hook may not affect independent validation paths; restore stock by toggling the constant if anything behaves unexpectedly. Android compilation/runtime not verified.
