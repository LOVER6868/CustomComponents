# Unearth SetupTargetsComponents audit

Based on user-provided `CustomComponents(20261028-215928).zip`.

New read-only hook: `PvZCards.Core.Systems.PlayOneTimeEffectCardSystem.SetupTargetsComponents(PlayCardEffect)`.

The hook logs `PlayCardEffect.LocationId` and snapshots live effect entities with `ReturnFromDiscardPileToLaneEffectDescriptor` before and after the stock method. Each matched entity logs the presence of primary/secondary filters and the first ID in its existing PrimaryTargets/SecondaryTargets components when accessible. `firstPrimary=-1` or `firstSecondary=-1` means absent, unavailable, or empty, not necessarily a verified missing component.

Look for `UNEARCH_SETUP_TARGETS: SETUP hook=1 modelField=1 locationField=1`, then `BEFORE` and `AFTER` lines during GUID 712 trick play. A zero `reviveEntities` count may indicate the effect entities are not yet part of the EntityModel at this boundary. Do not infer a missing descriptor if so.

No gameplay modification, target-list rewrite, chooser insertion, or feature flag changes. Native compile/device test not performed.
