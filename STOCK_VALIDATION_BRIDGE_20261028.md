# PickFromThree 712/713 stock validation recipient bridge

Baseline: StockValidationAudit FULL (2026-10-28).

## Scope

The previous validated primary discarded-card snapshot is used for popup offers. The normal stock finder returns lane entities during the later SelectTargetService.Process call and rejected an otherwise valid live-card selection.

This experiment applies only during the synchronous local SelectTargetService.Process for a `SelectTarget` play whose queue-front ChooseTarget effect/group, owner, source GUID (712 or 713), recovered mode 4, selected live card ID, and original validated primary snapshot agree. It does not change initial-play validation or the GUI offer list.

At that exact stock FindValidTargetsFromEffect call (only the already-offered effect), we invoke the original finder first, recheck the validated cards against the service EntityModel and Card component, and change the returned list's entries and count to those verified original validator IDs if the original returned List<int> has enough backing capacity. The selected ID must be present. Otherwise stock output is unchanged. The call-scoped bridge pointer is restored immediately afterward, even on normal scope exit.

## Logs

- `STOCK_VALIDATION_BRIDGE: PREPARE`: eligible service call + original list snapshot confirmation.
- `STOCK_VALIDATION_BRIDGE: APPLIED`: number of stock vs verified target IDs returned to stock validation.
- `STOCK_VALIDATION_BRIDGE: SKIP`: no safe list capacity, invalid model, or absent chosen card.
- Existing `VALIDATION_AUDIT`, `HANDOFF_TRACE`, `PICK3_TRIGGER`, `SUBMIT` logs remain enabled.

## Verification and limits

Source ZIP inspection and diff verification only. Not compiled against Android SDK/NDK and not device-tested. This is a gameplay experiment, not a read-only trace: it modifies the scoped finder result list to let stock validate the chosen real card. Because list mutation is in-place, it assumes the game's finder result is not shared beyond this synchronous process; confirm by testing in a controlled PvE match before considering PvP. No unrelated feature flags changed.
