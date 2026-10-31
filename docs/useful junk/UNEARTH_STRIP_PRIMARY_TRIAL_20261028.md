# Unearth temporary PrimaryTargets removal — opt-in experiment

Base: `CustomComponents_UnearthSetupTargetsLookupFixed_20261028_FULL.zip`.

This experimental change is **OFF by default**. In `inc/pick3/PickFromThreeSubmission.hpp`, change `kUnearthStripCastingPrimaryTrialEnabled=false` to `true` to opt in and rebuild. This is a compile-time toggle.

After the stock `PlayOneTimeEffectSystem.SetupTargetsComponents` returns, the new code resolves `PlayCardEffect.SourceId` to the actual card entity and checks its `Card.Guid` is 712 or 713. It scans the model for effect entities containing `PlayTrigger`, both target filters, and `ReturnFromDiscardPileToLaneEffectDescriptor`. For entities with exactly one `PrimaryTargets.TargetEntityIds` equal to `LocationId`, and no `SecondaryTargets` component, it invokes the game's `Entity.RemoveComponent(typeof(PrimaryTargets))`. It leaves the command and casting location alone.

Log: `UNEARCH_STRIP_PRIMARY: SETUP ...` and if enabled, `UNEARCH_STRIP_PRIMARY: entity=...` plus `SUMMARY`. An empty or error report means no mutation was proven. Run only in an offline/PvE test and retain the last known-good APK.

**Important:** the game may reject or stall when the preselected primary is removed. This patch is a lifecycle experiment, not a verified fix. The source-to-effect ownership guard is structural (PlayTrigger and revive descriptor), not a full source-card-to-effect-group proof. The prior diagnostic before/after snapshots remain.
