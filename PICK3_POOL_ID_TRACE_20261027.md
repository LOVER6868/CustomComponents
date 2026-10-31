# PickFromThree stock target ID diagnostic

This version keeps the existing successful GUID 712/713 candidate-validation and drop-target correction. It adds read-only logging in `inc/pick3/PickFromThreeTargetBridge.hpp`, immediately after `TargetEntityFinder.FindValidTargetsFromEffect` returns for `UniqueGuidTargets` mode (4).

`POOL_ID_TRACE: BEGIN` prints count and model. Each `POOL_ID_TRACE: ITEM` prints the unmodified raw stock integer ID (including zero or negative), entity lookup result/exception, `Card` component presence/exception, the existing `cardGuid(entity)` result/exception, and whether the existing code's ID/GUID predicate would keep it.

Logging is limited to the first 32 mode-4 finder calls and first 12 IDs per call. The trace does not alter lists, target filtering, picker options, selection, revive, or commands. It does not guess discard-zone identity: this first diagnostic measures whether the five returned IDs actually map to card entities. If `entity` and `Card` are available, the next trace can examine the discard data safely.

Test: play GUID 712 with a discarded plant, complete lane selection, and send `POOL_ID_TRACE`, `POOL_BOUNDARY`, `WEIGHTED_POOL`, `WEIGHTED_OFFER`, and `STEP2` logs. The deferred trial must stay enabled as in the previous working build.

Native compilation and on-device execution not verified.
