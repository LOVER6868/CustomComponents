# Deferred initial drop target experiment (712/713)

Enable `kDeferredPick3TrialEnabled` in `inc/pick3/PickFromThreeSubmission.hpp` (default false).

After candidate generation validates exactly one play with the stock effect-zone target, the StopCardDragCommand hook requires the DragModel to advertise exactly that zone as its sole legal target. Only during that matching drag, PlayCardCommand.Execute corrects EntityGestureData.TargetId (offset 0x14) to the legal zone before calling stock. The permission is consumed on first matching Execute, and does not run in SelectTargetCommand or a subsequent manual lane selection. No validator or underlying effect target filter is globally bypassed.

Logs: `DEFERRED_DROP: phase=ARM`, `SKIP`, `CORRECT`, `ALREADY_ZONE`. Confirm `DEFERRED_TRIAL: phase=ZONE_COMPARE_ACCEPT scope=service` and command acceptance, then check primary PickFromThree and secondary lane selection separately.

CAUTION: This is an uncompiled, untested experiment; later processing may still reject the corrected play or need a separate picker/resume change.
