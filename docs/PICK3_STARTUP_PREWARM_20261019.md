# Pick3 prewarm — crash-safe replacement (2026-10-19)

## Why the original startup prewarmer was unsafe

Two Android sessions crashed with UnityMain SIGSEGV at null+0x38 after:

```
PREWARM: ART stage=0 ... (native grass loaded)
UNITYFS: ASSET_BEGIN bundle=circle asset=...smallinspectionframe.png
```

Neither session reached ASSET_END for the rim. A managed TryInvoke guard cannot
catch a native SIGSEGV. Loading the same asset successfully during a Pick3
popup does not prove it is safe during frontend/PvE screen transitions.

## Behavior in this revision

This module **never calls AssetBundle.LoadFromFile, AssetBundle.LoadAsset,
Texture2D.LoadImage, Sprite.Create, ColorScheme, or the native-avatar factory
from its menu-time prewarm phase**. At 180 frontend ticks it starts cheap
CPU-only verification of the embedded fallback PNG signatures, sizes, and IHDR
dimensions. One verification runs every 12 ticks.

After an actual match has exposed a live local hand for 180 consecutive ticks,
prewarm can create the four supported fallback PNG sprites, spaced 12 frames
apart: **sun, heart, description plate, and inline tooltip plate**. This uses
Texture2D.LoadImage and Sprite.Create only on UnityMain and only after the
match-stability gate; no UnityFS/AssetBundle API is used by either phase.

Native **grass, rim and Choose button** remain deferred to the normal
PickFromThree Open() path inside a match. The game still uses process-lifetime
GC-rooted caches. If a picker opens before match prewarming finishes, the
existing on-demand path works; no additional prewarm is attempted after full
static assets are ready.

PNG fallbacks for grass and rim are *not* proactively constructed, so they do
not prevent native grass/rim sprites from taking precedence. The missing
Brain/Attack PNGs are not replaced with guessed artwork. Trait icons, avatars,
card-specific artwork, and localization are still outside this change.

## Configuration

```ini
[Pick3OwnedUnityFS]
Enabled=true
ProbeMode=4
MaxAssetIndex=6
PrewarmStaticUI=true
PrewarmStartFrames=180
PrewarmStepFrames=12
PrewarmMatchFrames=180
PrewarmFallbackSprites=true
AllowTextureToSprite=false
LoadNativeColorScheme=false
EnableSplitRim=true
EnableButtonSlicing=false
```

For *zero early Unity asset creation* use `PrewarmFallbackSprites=false`; this
still performs the harmless CPU-only PNG verification on menu frames. For a
complete prewarm rollback use `PrewarmStaticUI=false` and rebuild. To isolate
experimental native art separately, set `ProbeMode=3` (last previously
verified grass-only mode) and rebuild.

## Expected logs

```
PREWARM: CONFIG enabled=1 cpuOnlyMenu=1 matchFallback=1 ... nativeEarly=0
PREWARM: CPU_BEGIN nativeEarly=0
PREWARM: CPU_VERIFY kind=... valid=...
PREWARM: CPU_COMPLETE nativeCalls=0 unityCalls=0
PREWARM: MATCH_BEGIN stableFrames=180 nativeEarly=0
PREWARM: MATCH_PNG stage=0 kind=1 ... nativeCalls=0
PREWARM: MATCH_PNG stage=1 kind=4 ... nativeCalls=0
PREWARM: MATCH_PNG stage=2 kind=0 ... nativeCalls=0
PREWARM: MATCH_PNG stage=3 kind=7 ... nativeCalls=0
PREWARM: COMPLETE fallbackSprites=4 nativeEarly=0
```

`MATCH_BEGIN` and `MATCH_PNG` must not appear during PvE frontend navigation.
A menu-only test should show CPU verification, then stop until the game enters
a real match. If a native UnityFS call still fails *when opening the picker*,
it is independent of early prewarming and needs a separate asset investigation.

## Why this is safer, not absolutely crash-proof

Any Unity API can still fail natively; an existing live-hand object plus stable
frames is a practical gate, not a formally guaranteed scene lifecycle state.
Do not invoke Unity APIs from worker threads. If the in-match PNG prewarmer
itself causes crashes, disable only `PrewarmFallbackSprites`, keep CPU checks,
and retest before enabling optional native art again.
