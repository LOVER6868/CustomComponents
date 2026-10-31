# PickFromThree ownership cleanup test — 2026-10-24

This is an experimental, deliberately limited fix for the verified stale queued-offer lockup.

## Changed
- `inc/presentation/PickFromThreePreviewSystem.hpp`: a queued picker that is provably *Absent* from all three concrete local blocking-queue lists for 90 consecutive presentation ticks is retired instead of being held for 3600 ticks. Only the exact queued effect/group is cleared; the scoped overrides and owner latch are released using the existing `CompleteLocalSelection(..., false)` routine. A `Unknown`, `Queued`, `Front`, or `PlaceholderQueued` state cannot trigger cleanup. Completed-offer identity remains suppressed for the stale group.

## Explicitly unchanged
- The stock target finder, stock input binding, picker avatar generation, triggers, ReturnToLane and the button FSM are not modified. No synthetic tap, delayed target finder, forced phase resume or synthetic signal is added.
- Second-menu automatic opening and button recovery are *not yet proven fixed*. Existing `HANDOFF_TRACE`/`RESUME_TRACE` diagnostics remain to show why they fail, if they do.

## Device acceptance test
1. First picker opens on its own and offers three valid choices.
2. Selecting it can lead to another picker, which also opens without tapping the board.
3. Surprise/Done returns only on the proper stock phase and never prematurely.
4. There are no native crashes or repeated stale group popups.
5. Capture `CustomComponents/Pick3/RECOVERY`, `HANDOFF_TRACE`, `RESUME_TRACE`, `NO_POPUP` lines.

C++20 syntax-only validated against the supplied Android stubs and headers. This does not establish runtime correctness.
