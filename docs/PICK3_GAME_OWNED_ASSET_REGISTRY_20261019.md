# PickFromThree: game-owned UI asset registry (stage 1)

Built against the prior **StockInspection TraceSafe** CustomComponents ZIP. This
is an **experimental runtime feature**, not a proven Android-stable release.

## Why

`AssetBundle.LoadFromFile()` on privately copied UnityFS files caused Unity to
reject the game's own later load of `autotagged/53bddfe24e1719146baf8fa4757904c4`.
The built-in card inspection overlay then intentionally exited the application.
This update NEVER calls `AssetBundle.LoadFromFile` to get stock art.

## Capture and use

Passively hooks **natural** calls to:
- `CollectionCardFrameView.Awake()` — stock plant/zombie cost sprites, icon set,
  and copied class palette.
- `CardFrameViewComponent.Awake()` — icon set/palette if initialized.
- `CardInspectionPanelView.Awake()` — icon set if initialized.
- `CardFrameIconSprites.GetSpriteByType(int)` — late capture of the icon set,
  since the tracer proved board-frame Awake can have null IconSprites.

On the Unity UI thread, reads only verified class fields. Retains only `Sprite`
managed GC handles; never retains scene `Image`, `Animator`, `View`, or prefab
component pointers. Before use it checks both managed type and native Unity
Object liveness using `UnityEngine.Object.op_Implicit`. Dead references are
released, and the picker falls back. All rooted references are released when
`PickFromThreePreviewSystem` detects a match ending.

Retained fields: 15 `CardFrameIconSprites` variants, the two cost Sprites, and
10 copied `DefaultFrameColors` RGBA values (not managed scene objects).

The picker currently **renders stock BasicAttackSprite, BasicHealthSprite and
plant/zombie cost sprites** whenever captured and alive; all other artwork
continues using existing intact fallback paths. The additional 13 trait-icon
variants are available in the registry, but dynamic selection based on card
abilities is **not yet implemented**. Class RGBA values are retained for future
stock class frame composition, but this update deliberately does not redraw
class rims: the correct prefab masking/material layering is unverified.

Grass, Choose button, animated card previews, rewards and inline TextMeshPro
sprite atlases are **unchanged**. Do not turn the manual UnityFS loader back on.

## Installation (avoid duplicate hooks)

The recommended update-only archive also has
`Pick3AssetTracer/config.ini` with `[module] enabled=false`. The standalone
tracer hooks the same original methods and must not run alongside this registry.
The module also auto-detects an enabled Pick3AssetTracer and deactivates itself
instead of knowingly double-hooking.

Keep `[Pick3OwnedUnityFS] Enabled=false ProbeMode=0` from the trace-safe build.

## New config

```ini
[Pick3GameOwnedAssets]
Enabled=true
UseInPicker=true
LogCaptures=true
```

To test in capture-only mode, set `UseInPicker=false`. To disable all new
hooks, set `Enabled=false` and rebuild/reinstall. Do not change your existing
native avatar crash isolation.

## Expected logs

- `STOCK_REGISTRY: READY hooks=4 useInPicker=1 bundlesLoaded=0 avatarCalls=0`
- `STOCK_REGISTRY: ICONS source=icon-resolver updated=...`
- `STOCK_REGISTRY: COSTS source=collection sun=1 brain=1`
- `STOCK_REGISTRY: ICON_USE type=0` / `ICON_USE type=1`
- `STOCK_REGISTRY: COST_USE faction=plant` / `faction=zombie`
- `STOCK_REGISTRY: ...FALLBACK...` when the source hasn't yet initialized
- `STOCK_REGISTRY: RESET reason=match-ended`

A `COST_FALLBACK` on the first match picker is expected if no collection or
inspection cost asset has been captured in that match. Native asset availability
is not guaranteed before the first stock view is initialized.

## Suggested one-run test

1. Install update-only ZIP and rebuild. Confirm the tracer is disabled.
2. Open collection first (both plant and zombie) and enter a match.
3. Trigger PickFromThree and inspect whether the icon/badges now match stock.
4. Open normal card inspection repeatedly (both factions).
5. Trigger a second picker; then finish/leave the match and enter another.
6. If a crash occurs, set `Enabled=false` in Pick3GameOwnedAssets and rebuild.

Host C++ syntax checking passed. Android runtime testing has NOT been performed.
