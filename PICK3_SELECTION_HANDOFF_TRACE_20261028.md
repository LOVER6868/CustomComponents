# Pick3 selection handoff diagnostic (2026-10-28)

Base: ValidatedPrimarySource_ENABLED_FULL_20261028. Read-only log additions only.

No gameplay flags altered; kDeferredPick3TrialEnabled remains true.

Modified:
- `inc/pick3/PickFromThreeSubmission.hpp`: begin/dispatch checkpoints and actual stock SelectTargetService.Process entry/exit with completion status.
- `inc/gameplay/SelectionCompletion.hpp`: existing handler snapshot, delegate attach result, and success/failure callback ticket/return status.

Log prefix: `CustomComponents/Pick3/HANDOFF_TRACE`.

Run GUID 712 with two eligible graveyard cards, select one in popup, choose a lane, capture logs until completion. Compare `SERVICE_CALLBACK` against `STOCK_PROCESS_ENTER/EXIT`, `DISPATCH_RETURN`, and `SUBMIT: RESULT`. This does not attempt to repair resumption.

Not natively compiled or device-tested.
