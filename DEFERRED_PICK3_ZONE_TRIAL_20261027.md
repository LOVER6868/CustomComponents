# Deferred PickFromThree initial-zone trial (712/713)

**Default: OFF.** In `inc/pick3/PickFromThreeSubmission.hpp`, change
`kDeferredPick3TrialEnabled = false` to `true` to opt in. Rebuild the APK.

The trial has two coordinated, narrow changes:

1. In `GameStateUtils.GetPossiblePlayTargetsForBurst`, the existing scoped
   `IsOnPlayTargetedEffect` hook reports an untargeted initial trick for GUIDs
   712/713 only, enabling stock zone-target generation. The generated zone
   is recorded per card entity (bounded 32-slot table).
2. When `LocalPlayCardService.Process -> DetermineIfPlayIsValid ->
   OneTimeEffectValidator.IsTargetingValid` validates the same submitted Play,
   only its *immediate target comparison* can return true, and only when the
   submitted target exactly matches that card's recorded, engine-generated
   initial zone, and eligible effect targets currently exist.

This does NOT override OneTimeEffectValidator.Validate, CompositeValidator,
PlayCardService, PickFromThree selection, or the secondary lane validator.
All other plays run through the stock comparison. No automatic discarded-card
choice is made, and the card's primary/secondary filters are untouched.

**Known limitations / experimental:**
- A board-drop overlap can differ from the generated one-time-effect zone ID.
  In that case validation is deliberately not bypassed and `InvalidTarget`
  may persist. Do not change a drag target to a discarded-card ID to mask it.
- The engine may require the original effect target to be set before effect
  execution. A successful initial play is NOT proof the picker or secondary
  lane step will work. Verify logs and gameplay before considering the trial
  successful.
- The 32-slot cache is only for experimentation; collision/long-running match
  lifecycle need hardening for production, especially multiplayer.
- Service-side processing on a different thread may not share the thread-local
  trace context. In this case the stock validator is preserved.
- `validTargets` must expose get_Count; unreadable collections take stock path.

Look for `DEFERRED_TRIAL: SETUP enabled=1`, `ZONE_RECORDED`, and
`ZONE_COMPARE_ACCEPT` or `STOCK_COMPARE` on each attempted 712/713 play.
The rollback is toggling the flag back to `false` and rebuilding.

No native compilation or device test performed in this environment.
