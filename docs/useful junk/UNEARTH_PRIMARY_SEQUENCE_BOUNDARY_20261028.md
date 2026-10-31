# Unearth primary sequencing boundary / rollback (2026-10-28)

The previous PrimaryPhaseHandoff mutated the source List<int> during stock effect creation. Runtime logs show `PRIMARY_SUBSTITUTE` followed by `primary=165 secondary=165` and `secondaryLane=0`: the revive effect attempted to run with a plant as its lane. It **did not** postpone secondary selection. The substitution is removed.

This diagnostic build adds read-only hooks for PrimaryTargetEffectCreator.CreateEffectsForPrimaryTargets (2 parameters) and CreateEffectsBasedOnTargetMethod (4 parameters). Both invoke stock originals unchanged; no effect/queue edits. Setup log: `UNEARCH_PRIMARY_BRANCH: SETUP normal=... method=...`. On play, compare `NORMAL_ENTER`, `METHOD_ENTER` and `PRIMARY_PRESELECT_ENTER` to learn whether Burst bypasses primary choice entirely. The `ShouldAddChooseTargetEffects` hooks remain unresolved in previous builds (`decision3=0 decision2=0`), so do not infer their return values.

**Not a completed gameplay fix.** Future sequencing work requires verified primary chooser ownership/continuation; do not directly call CreateEffectsForPrimaryTargets from inside the preselected creator, as a previous build softlocked. Test offline/PvE first and preserve previous working APK.
