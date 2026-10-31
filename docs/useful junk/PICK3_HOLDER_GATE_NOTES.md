# Pick3 Holder-Gated Containment (2026-10-24)

This is NOT a confirmed full fix. It targets two regressions identified in `PvZHeroes_Handoff_Capture(1).txt`:

* At 16:14:28.354 a speculative picker offer was generated for effect 50/group 375. The popup prepared at 16:14:28.556, while the game input holder had not entered targeting state 6 for group 375. The popup closed as stale at 16:14:29.635. The legitimate `BindCommandsForState(6)` for group 375 happened only at 16:14:36.746, with no pending picker request.
* `STALE_QUEUE_ABSENT` in the previous patch discarded queued offers after 90 frames even though later stock transitions could activate those effect/group IDs.

Changes:
* Queue/front appearance alone can no longer open the popup: current holder targeting state must be 6/7 AND holder effect/group must match AND the real local queue front must match.
* Removed frame-count-driven speculative queue cleanup, which was deleting some still-future offers.
* Decoupled 'a preview is pending' from 'queued choice may suppress stock input': the latter now requires the holder in targeting state 6/7 with identical effect/group. Before that, normal stock target/board input is not suppressed by the speculative queued offer.
* Existing visible-popup stale check now respects the input holder's confirmed ownership instead of treating a transient prediction queue mismatch as proof the popup retired.

Unchanged: renderer, card effects, PlayTrigger, ReturnToLane, button FSM, native avatars.

Limitations: The finder can produce offers for multiple prediction-group IDs. This patch still retains only one queued request at a time and therefore does not guarantee selection of the correct later-activated group. Stock Surprise/Done button phase resume was not modified. A real Android test is required.

Logs to compare: `ACTIVATION_GATE: WAIT` / `ACTIVATION_GATE: ACCEPT`, `NO_POPUP`, `STABILITY`, `SUBMIT: RESULT`, `LIFECYCLE: BIND_ENTER`, `RESUME_TRACE`.
