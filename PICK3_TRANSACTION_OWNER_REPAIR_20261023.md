# Pick3 transaction owner repair (2026-10-23)

Scope: corrective changes to the ownership of a live UI selection, based on the latest CustomComponents full-source lifecycle baseline.

## Behavior
- A single owner (effect ID + group ID) persists across **Queued** and **Displayed** phases; `Consume()` changes to Displayed rather than making publication available again.
- New finder evaluations cannot replace the eligibility snapshot while that owner remains active. Stock finder results remain untouched.
- A set of consumed effect/group IDs blocks repeating a previously completed offer in the match. Reset at match reset (`TargetBridge::Clear()`).
- `CompleteLocalSelection` restores scoped target/query overrides and releases the owner only when IDs match. The existing rearm argument can allow an explicit retry, but normal successful and unsuccessful FinishTransaction calls leave the offer consumed.
- An unrelated stock targeting command cannot overwrite a currently captured model during the popup.

## Not claimed
- No stock command completion has been synthesized; the PvE/PvP input-lock remains unverified.
- No candidate enumeration or weighting algorithm changed; short stock target lists can still lead to fewer than 3 options.
- No game-side running tests have been performed.

## Device verification
Test Saucer Squash and Fisherman in PvE and PvP, especially overlapping activation and rapid repeated finder invocation; capture `POOL_BOUNDARY`, `HANDOFF`, `RETIRE_BOUNDARY`, and `INPUT_HANDOFF` lines on failure.
