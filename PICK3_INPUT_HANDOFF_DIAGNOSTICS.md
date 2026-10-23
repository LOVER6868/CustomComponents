# PickFromThree input-handoff diagnostics — October 23, 2026

Baseline: CustomComponents_Pick3_PvP_handoff_20261023.zip.

Only functional source diff: inc/pick3/PickFromThreeSubmission.hpp. No input interception, submission, cleanup, card rendering, or gameplay semantics changed.

New always-visible INFO messages:
- `HANDOFF: BEFORE_CLEANUP`: remote/local service, success, queued state, input capture, visual target phase, existing stock OnSuccess handler, selection completion status.
- `HANDOFF: AFTER_CLEANUP`: check whether queued state/input capture remains and whether the effect was rearmed.
- `INPUT_HANDOFF`: next 24 PlayCardCommand, SelectTargetCommand, and HandleBoardTappedCommand execute calls; records blocked vs forwarded, active completion status, and phase snapshot. It is a bounded event budget, not a timer. If no INPUT_HANDOFF events appear when tapping the hand, that itself is valuable: the touch/highlight problem is upstream of these stock command hooks.

Please reproduce: in PvP choose one live-entity option (not CardDatabase), then try selecting/highlighting and playing a normal card, then Unearth again. Capture logs containing HANDOFF, INPUT_HANDOFF, SUBMIT, STOCK_INPUT_BLOCKED. For the phone crash obtain tombstone/native stack immediately after submitting.

Verification: clang++ C++20 -fsyntax-only against supplied BNM, Sprig and Dobby includes succeeded (host Android shim). Not an Android build and not device tested.
