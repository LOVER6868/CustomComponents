# PickFromThree PvP handoff correction (2026-10-23)

Baseline: CustomComponents_Pick3_focused_repair_20261023.zip.

Changed existing source: `inc/pick3/PickFromThreeSubmission.hpp` only.

## Evidence

The supplied phone log reports `SUBMIT: RESULT success=1 outcome=2 rearmed=1` for both PvE and PvP. PvP's opponent receives and applies the correct card; only the submitting client has subsequent input/Unearth failures. The prior implementation re-armed an already-offered effect/group as soon as RemoteSelectTargetService raised OnSuccess. This can permit another FindValid call on the not-yet-retired replicated blocking effect to offer the same picker again.

## Change

PvE retains `!uncertain` re-arm and its original conditional GameStateUpdatedSignal notification. In PvP, a submitted effect/group is kept consumed (rearm=false), including remote failures/uncertain results, to avoid duplicate network submissions. New effect/group IDs remain eligible. Both modes still restore the scoped manual filter and clear the current snapshot. The result log now reports the actual re-arm decision, and PvP accepted transactions emit `PVP_LIFECYCLE: ACK_CONSUMED`.

## Scope and limitations

This is a targeted testable mitigation of stale picker re-entry, not proof the entire native SelectTargetCommand UI/phase is released. It does not alter CardDatabase network-transport policy, avatar rendering, or input hooks. If PvP input still sticks, collect logs for `PVP_LIFECYCLE`, `STEP411`, `STOCK_INPUT_BLOCKED`, `BOARD_TAP_SUPPRESSED`, and state transitions after RESULT; next step is matching that state against SelectTargetCommand's original success handler. Do not mark resolved without real device testing.

Host C++20 syntax check: PASS; not Android-built/device-tested.
