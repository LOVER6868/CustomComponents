# PickFromThree selection retention / Banana Bonsai diagnostics

- Card 706 uses `PickFromThreeCardDatabase` and a subsequent `DrawCardFromSubsetEffectDescriptor` with `PickedFromThreeQuery`.
- The previous selection store was keyed by `(effectEntityId, effectGroupId)` and rejected insertion at 64 entries. This is now one latest selection per effect, overwriting earlier groups.
- Selection cache is preserved and overwritten during the current match, and is not consumed by queries. It is cleared when the match ends, before the next match begins. Distinct effects remain bounded to 64 per match, evicting the oldest only if needed.
- Failed `Process` still erases the just-staged selection; otherwise a failed choice would be treated as valid by subsequent queries. That is an intentional transaction rollback, not a normal selection clear.
- The distinct retained Play objects limit remains 128 because their native lifetime is not yet known. It now emits an explicit error when hit; do not arbitrarily free GC handles until engine async ownership is proven.
- This patch was source-reviewed and ZIP integrity-checked, not compiled or exercised on a device. Investigate logs `PICKED_QUERY: STORE`, `PICKED_QUERY: STORE_FAILED`, `SUBMIT: RETAINED_PLAY_LIMIT`, and `SUBMIT: SERVICE_EXCEPTION` if the effect still fails.
