# PickFromThree stock selection validation audit — 2026-10-28

Baseline: CustomComponents_Pick3_SelectionHandoffTrace_20261028_FULL.zip.

**Goal:** one comprehensive follow-up test of the rejected `SelectTargetService.Process` for GUID 712/713, compared to the working CardDatabase case (e.g. GUID 716). No gameplay hooks, flags, selection protocol, or target lists changed. `kDeferredPick3TrialEnabled = true` remains enabled.

## Code review conclusions

- `Submit` builds a `Play` of `type=6`, with `CardId=-1` for ordinary live-entity Pick3 and `CardId=selected GUID` for DB5. This is intentional and must not be changed just because `-1` appears.
- `LocalSelectProcess` handles `type=6` and inspects the concrete front of the blocking effect queue. If owner and restored filter mode match, the selected entity is re-stored into `Picked`. Stock `originalLocalSelectProcess` still validates the DTO.
- The bridge normalizes the live target filter to stock Manual mode for an active popup, so distinguish **raw** versus **recovered** selection mode and match effect/group/owner.
- The UI validated the entity against its snapshot before dispatch, but the stock simulation model may differ from the UI model; this test traces both.
- `SERVICE_CALLBACK accepted=1` means the callback is accepted by our tracker, **not** that stock gameplay accepted the selection.

## New log prefix

`CustomComponents/Pick3/VALIDATION_AUDIT`, phases:

- `PREPARE_REQUEST` / `OFFER`: exactly what the picker submitted, owner, mode, IDs and GUI-validated snapshot.
- `QUEUE_BEFORE`: actual stock front effect/group/owner, request effect/group, Play player/CardId/targets. Does not call `Peek()` or modify queue.
- `FILTER_CONTEXT`: raw vs recovered filter mode, actual source effect, owner match, injected service model vs picker input model.
- `TARGET_IN_SERVICE`: same selected entity ID looked up in the *stock service's* model, GUID and exception status, plus whether still in UI snapshot.
- `QUEUE_AFTER`: stock queue front after service processing and callback completion/reason.

Existing `HANDOFF_TRACE` phases log Process enter/exit, delegate attachment and completion callback. Existing `DEFAULT_REPAIR`, `PICK3_LIVE_CAPTURE`, `PICK_RECONSTRUCT`, `PICK3_LIVE_EARLY`, `SUBMIT: RESULT` and `TRICK_ACCEPT` should also be captured.

## One-test collection

Rebuild and play GUID 712 with two eligible discarded plants, select one choice, observe lane stage. If possible, play a working GUID 716 PickFromThree card in the same session for a control. Capture the full `Sprig` log (not just errors); preserve both selection events and the startup flags.

## Safety / unverified

Read-only logging only. No validator bypass, forced callback, replay, new hooks or filter mutation introduced. Native compilation / Android runtime not performed in this environment.
