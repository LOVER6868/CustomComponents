# PickFromThree white-square fix across match transitions (2026-10-19)

## Reproduction

First match: verified grass/rim and cream description plates draw normally. Later
matches in the same app process: rectangular white backgrounds obscure the
portrait and description surfaces, while text and game-owned basic attack/health
badges still appear. `STOCK_REGISTRY: ICON_USE type=0/1` in the logs proves the
stock icon registry is not a full explanation for the rectangle issue.

## Source bug and change

`LoadRewardSpriteImpl` cached managed `Sprite*` pointers across matches without
rechecking whether Unity's native Sprite **and** underlying Texture2D were still
alive. IL2CPP GC roots keep the managed wrappers reachable, but do not promise
Unity will retain their native images after the game unloads scene resources.
`RewardImage` also left a fully opaque UGUI Image behind if the real Sprite was
missing.

New behavior:

1. Check Unity `Object.op_Implicit` for the cached Sprite and owned Texture2D,
   and check `Sprite.get_texture` for the actual backing Texture before reuse.
2. If any test fails, destroy only our own still-live Sprite/Texture2D, release
   GC handles, and rebuild that PNG sprite inside the match on demand.
3. Apply `HideFlags.DontUnloadUnusedAsset` (32) to freshly generated fallback
   Sprite and Texture2D objects, **never** to stock/game-owned sprites. This
   protects against `Resources.UnloadUnusedAssets` where Unity supports the flag.
4. Draw missing grass/rim/cost/stat Image layers transparent instead of opaque
   rectangles. Cream description-plate fallback retains its normal color.
5. Log `ART_CACHE: CREATE`, `ART_CACHE: REUSE`, `ART_CACHE: INVALIDATE`, and any
   `ART_CACHE: PIN_FAILED` to test art lifetime across two or more matches.

This does not load AssetBundles, change the game-owned registry, or enable native
avatar rendering. `Pick3OwnedUnityFS.Enabled=false` remains mandatory for the
stock inspection asset-bundle collision workaround. The additional trait-icon
variants remain captured but not dynamically selected; that is separate work.

## Test

- Install over the **GameOwnedRegistry** version, rebuild and reinstall.
- Enter a first match, open PickFromThree and complete it, then end that match.
- Enter a **second** match without restarting the app, open PickFromThree again.
- Repeat on a third match if possible.
- Confirm grass/rim/cream plates remain visible. Compare `ART_CACHE: REUSE` with
  any `ART_CACHE: INVALIDATE` / `ART_CACHE: CREATE` on the second match.
- If blank rectangles persist while all cached art reports native backing alive,
  capture a device screenshot and Sprig log; the next step would be inspecting
  the actual `Image` component/material/texture during rendering rather than
  assuming the PNG cache is the only cause.

Host-only C++ syntax verified; Android runtime behavior remains untested.
