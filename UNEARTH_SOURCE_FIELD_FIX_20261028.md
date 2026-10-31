# Unearth SourceId lookup correction

Changes exactly one lookup in `inc/pick3/PickFromThreeSubmission.hpp`:

- Before: `BNM::Class("PvZCards.Engine.Effects","Effect")`
- After: `BNM::Class("PvZCards.Engine","Effect")`

`PlayCardEffect` derives from `Effect` and uses its inherited `SourceId`. The patch keeps all other hooks and guards unchanged.

The archived source has `kUnearthStripCastingPrimaryTrialEnabled=false` by default. To test the removal path, set it to `true`, rebuild, and check `UNEARCH_STRIP_PRIMARY: SETUP enabled=1 sourceField=1`. Observe the actual `SUMMARY` and `attempted` lines to confirm whether removal occurs. Test in PvE and retain a rollback APK.

Only ZIP integrity was checked; no Android build or runtime validation was performed.
