# PickFromThree upstream pool and completion diagnostic

This is a READ-ONLY diagnostic of the existing selection path, not a behavioral fix.

- `POOL_BOUNDARY: BEFORE_STOCK`: filter mode / NumTargets and scoped override status immediately before `originalFindValid`.
- `POOL_BOUNDARY: AFTER_STOCK`: stock-returned target count vs. resolved live card candidates.
- `POOL_STACK`: copy count for each GUID in that stock-returned list. These are **not** guaranteed to be the full eligible deck query results.
- `RETIRE_BOUNDARY: BEFORE/AFTER`: service-completion status, popup capture, local subturn snapshot and fallback flags.

Compare `POOL_BOUNDARY` to `WEIGHTED_POOL` and `WEIGHTED_OFFER` for each effect/group. If BEFORE_STOCK reports NumTargets=1 and AFTER_STOCK returns ~3 while the deck has many eligible cards, then an upstream stock selection/query needs investigation. This build cannot directly prove the full pre-sampling eligible set, as no pre-stock enumeration API has been safely established yet.

The original command, timer, selection transport, UI and cleanup operations have not been changed.
