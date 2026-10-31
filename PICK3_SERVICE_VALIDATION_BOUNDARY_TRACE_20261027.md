# Read-only service validation trace for PickFromThree tricks

This revision instruments `PvZCards.Core.Services.LocalPlayCardService.Process(Play)` and `PvZCards.Core.Utilities.PlayCardHelper.DetermineIfPlayIsValid(Play)` to compare the **actual submitted `Play*`** against the validator calls made for that pointer. It does not infer card identity from the last drag event. It also logs the full submitted Play's CardId, GUID (when model identity resolves), PlayerId, Type, OrderInTargetLocation, and up to eight target IDs.

`TRICK_SERVICE` records:
- `SETUP`: per-method and per-field availability.
- `PROCESS_ENTER`/`PROCESS_EXIT`: actual submitted Play pointer, card GUID, and targets.
- `DETERMINE_ENTER`/`DETERMINE_CONTEXT`/`DETERMINE_EXIT`: validation argument, return, and fail-reason count.
- `VALIDATOR`: concrete Composite/OneTimeEffect/Null/InHand validator result **only when called with the same Play pointer during Determine**.
- `TARGET_COMPARE`: an IsTargetingValid call within Determine; accompanying `TRICK_TARGET_VALID` collection snapshots distinguish `serviceValidTargets` / `serviceSelectedTargets` from candidate-generation paths.

The previous diagnostics and disabled initial-zone experiment remain unchanged. The service captures only focused GUIDs 699, 712, 713, 27; `Determine` capture is capped at 100 focused calls to avoid unbounded log volume. The concrete validator coverage is still partial, and error collection logging reports counts, not error codes. An absent `VALIDATOR` call can therefore mean the concrete implementation was not hooked.

Rebuild from the ZIP, verify `TRICK_SERVICE: SETUP determine=1 ...`, then drag GUID 712 with at least one eligible discarded card and compare `PROCESS_ENTER`, `DETERMINE_ENTER`, `VALIDATOR`, and `DETERMINE_EXIT`. Also run GUID 699 as a control. No target list, play, error list, or validation return is modified by the new trace.

## Validation status
Archive packaging checked. Native compilation / on-device runtime have not been performed here.
