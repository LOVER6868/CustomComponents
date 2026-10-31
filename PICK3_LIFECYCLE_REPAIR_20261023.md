# PickFromThree lifecycle/source repair — 2026-10-23

Changed files:
- `inc/pick3/PickFromThreeSubmission.hpp`
- `inc/pick3/PickFromThreeTargetBridge.hpp`
- `inc/presentation/PickFromThreePreviewSystem.hpp`

## Functional changes
- Keep effect/group consumed after submission in PvE as well as PvP. Previously `Local() && !uncertain` immediately rearmed the same key and could republish it while stock re-queried the effect. A new effect/group is still eligible; a match reset clears the consumed key.
- Use resolved `Effect.SourceId` for preview source attribution instead of replacing it with the most recently played card in a 1500ms window. This is especially important for phase triggers.
- When a late remote-owner popup is discarded, clean up its overrides without rearming that same effect/group.

## What this does NOT fix or claim
- No manual invocation of game `FinishChooseTargetPhase`, command `Success`, or `Release`: doing so without matching command ownership risks duplicate processing.
- No unvalidated rewrite of the stock `TargetEntityFinder` candidate enumeration. The weighted sampler already computes weights correctly over the list it receives, as demonstrated by Fisherman's 29-entity/10-stack trace. Saucer Squash's 2-entity example is an upstream returned pool, and whether this is a stock truncation or the actual remaining eligible cards is not established.
- PvE/PvP stuck-input and highlighting symptoms are not proven resolved without rebuilding/testing on device.

This is a targeted source repair, not a fully verified game fix.
