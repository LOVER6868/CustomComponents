# Pick3 UnityFS crash isolation — 2026-10-19

## Source of this patch

Crash occurred at **14:54:18** (UnityMain, SIGSEGV at `0x38`). In
`Pasted text(20261019-185444).txt`, the last completed Pick3 message was
`UNITYFS: API ready=1 loadFile=1 loadAsset=1 sprite=1 path=1`.
No `BUNDLE_READY` was reported. The failing build invoked
`UnityEngine.Application.get_persistentDataPath()` immediately after that
log; this is a **leading suspect**, not a proven symbolic stack match.
The exception log stack entered `libil2cpp.so` via `BNM::TryInvoke()`.

## Safe defaults

```ini
[Pick3OwnedUnityFS]
Enabled=false
ProbeMode=1
```

With `Enabled=false`, the direct loader never runs its Unity APIs or attempts
native AssetBundle loads. The existing verified original PNG cache supplies
grass and circular frame, sun and heart when available. The native-asset-only
brain/attack may be absent. Animated avatars remain crash-isolated.

## When ready to run controlled tests

**Make one change per test and collect Sprig logs and full tombstone.**

* `Enabled=true, ProbeMode=1`: Verify bundled bytes written to app-private
  storage; **no `LoadFromFile` or `LoadAsset`** call.
* `Enabled=true, ProbeMode=2`: Call `AssetBundle.LoadFromFile` for the **circle
  bundle only**. Do not resolve Sprite subassets.
* `Enabled=true, ProbeMode=3`: **New limited test**: `LoadAsset` only
  the circle's original `frame_circle_grass` Sprite (full-path and short
  name attempts), then assign it to the three existing grass Images. The
  verified original PNG is retained if Unity returns no Sprite. No other
  Sprite or Texture2D, no ScriptableObject, no avatar/Prefab calls.
* `Enabled=true, ProbeMode=4`: Former full experimental asset resolution;
  **do not test yet**. This enables Sprite.Create/Texture2D fallbacks and
  other source bundles, which are separate crash risks.

The loader uses `/data/user/0/com.ea.gp.pvzheroes/files` on this app/user
instead of invoking `Application.get_persistentDataPath()`. If that folder is
missing it logs `PATH_FAILED` and returns without a native Unity call.

Look for checkpoints: `CONFIG`, `API`, `PATH_BEGIN`, `PATH_READY`, `FILE_BEGIN`,
`FILE_READY`, `LOAD_BEGIN`, and `BUNDLE_READY`. The last checkpoint and the
native tombstone will identify the next unsafe operation more precisely.

Rebuilding/reinstalling is necessary after changing C++ code. The config
file must be bundled into the mod as usual. Keep native-avatar crash isolation
on; do not simultaneously experiment with animated card assets.

## Popup timing and Sprite lifetime

New checkpoints: `SPRITE_BEGIN`, `ASSET_BEGIN` / `ASSET_END`,
`SPRITE_READY` / `SPRITE_MISSING`, `GRASS_APPLY_BEGIN` / `GRASS_APPLY_READY`
and `Pick3/PERF: PRELOAD`, `CARD_UI`, `METADATA`, `OPEN`. Each measurement
uses native `CLOCK_MONOTONIC`, not game time; these timings measure work
within `Open()` and cannot account for every external card-effect stall.

The image/texture caches are process-lifetime and GC-rooted. Removed the
previous per-popup pointer reset that caused already-rooted original PNG
assets to be decoded and reconstructed repeatedly, potentially stalling
Unity and leaking redundant root handles. Scene-owned Image components are
still freshly created for each popup.

No newly built version has been phone-tested. If stage 3 crashes or freezes,
revert to `Enabled=false` (or last stable ProbeMode 2) and capture the last
`Pick3/UNITYFS` checkpoint plus tombstone. Do not test ProbeMode 4.
