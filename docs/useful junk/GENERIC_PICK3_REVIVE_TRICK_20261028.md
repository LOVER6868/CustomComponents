# Generic PickFromThree → ReturnFromDiscardPileToLane trick experiment

This build replaces GUID 712/713 gating of the **deferred initial-zone and drop adapter** with a conservative live-effect signature, and generalizes the experimental PrimaryTargets removal guard.

An eligible card must:
- Be a live card entity with the **Burst** marker (a trick, rather than a fighter).
- Have a **first PlayTrigger** (lowest entity ID belonging to that card in the current model) with **PrimaryTargetFilter** in a custom PickFromThree selection mode, **SecondaryTargetFilter**, and **ReturnFromDiscardPileToLaneEffectDescriptor**.
- For removal only: have exactly one PrimaryTargets entity ID matching PlayCardEffect.LocationId, no SecondaryTargets, and the same verified effect ownership.

Later PlayTriggers and other descriptors are not changed. No blanket changes to non-PickFromThree tricks, fighter effects, or the secondary target selection. This approach treats effect entity creation order (entity IDs) as PlayTrigger order, which must be verified using the game's logs; effect groups are not guaranteed to sort this way across all asset content.

The existing initial-zone, candidate, service, and drop adapters preserve their own additional original validation/ownership checks.

## Test configuration

The Strip Primary experiment remains **disabled in the ZIP**. Set `kUnearthStripCastingPrimaryTrialEnabled` to `true` in `inc/pick3/PickFromThreeSubmission.hpp` before testing. The existing `kDeferredPick3TrialEnabled` is preserved.

## Known limitation

The sole-offer automatic selection path is **not fixed**. The proven multi-offer manual path is the regression target. Do not infer one-option success from removal success. Also, a source lacking the Burst marker is intentionally skipped, even if its other components match.

## Verification

Source edited and ZIP integrity checked. No Android native build, live game test, or multi-effect first-trigger ordering test performed.
