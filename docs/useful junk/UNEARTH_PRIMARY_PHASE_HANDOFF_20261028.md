# Unearth primary phase handoff (experimental)

This patch ONLY modifies `inc/pick3/PickFromThreeSubmission.hpp`.

The game's serialized filters for 712/713 already put discarded-card PickFromThree in primary and a manual lane choice in secondary. The Burst adapter's temporary casting lane is necessary for initial play acceptance, but the same lane was observed in the preselected effect primary list after PickFromThree completion.

At the stock `PrimaryTargetEffectCreator.CreateEffectsWithPrimaryTargets` boundary, the patch checks for an active, verified SelectTargetService recipient with source card, validated chosen plant, exactly one primary target equal to the previously recorded casting lane, and 712/713 trick focus. It then temporarily substitutes that list entry with the chosen plant, calls the original game method, and restores the list entry immediately. The engine retains responsibility for building follow-up targeting effects.

No manual insertion into effect queues or secondary creator calls. No override to initial trick acceptance, no serialized filter edits. Does not alter other cards under its guards.

New diagnostics: `UNEARCH_PHASE_HANDOFF: PRIMARY_SUBSTITUTE` and `PRIMARY_RESTORE`.

This is an untested experimental patch. If the target context still reports primary=castingZone, or the lane choice doesn't appear, do not assume the continuation has been repaired. Test with one discarded plant first and check for command success and matched SUBSTITUTE/RESTORE. Runtime/native compilation not available here. Keep the last known working ZIP for rollback.
