# Passive trick board-drop diagnostics

Build this module and compare (A) ReturnToHand PickFromThree trick (board placement accepted) with (B) ReturnToLane PickFromThree trick (placement rejected).

Collect `TRICK_PLACE_DIAG` log lines plus surrounding `INPUT_HANDOFF`, `PICK3_LIVE_*`, and `PICK3_TRIGGER` lines. They distinguish whether stock `StopCardDragCommand.Execute` runs, whether `GameStateUtils.GetValidPlayTargetsForCard` is queried, and whether `PlayCardCommand.Execute` is reached.

This experiment does not modify play targets, gesture data, selection queries, cost payment, board placement, or effect scheduling. The target-list pointer is logged without enumeration because the returned list is pooled. A query can be handled via virtual/interface dispatch and bypass the method hook; absence of a VALID_TARGETS line does NOT prove the check was never performed. Observations are capped for stop-drag and validation calls; PLAYCARD lines are uncapped.

If game startup reports another stock hook conflict, revert to the previous LiveEarlyQueue zip. C++/Android build and device testing have NOT been performed by ChatGPT.

## Correlated target-ID and resolver experiment
New markers: `TRICK_TARGET_IDS` at STOP_BEFORE/STOP_AFTER (read-only snapshot of DragModel.Targetids); `TRICK_OVERLAP` (stock GetTopDraggedEntityId result); `TRICK_DRAG_LIFE` (start and throttled update events). `SETUP` logs feature availability. The resolver's IEnumerable input is intentionally not treated as a List. Targetids is not modified. Compare rejected and successful drop attempts with similar release locations.
