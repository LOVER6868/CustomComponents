> **CRASH WARNING (2026-10-19):** The initial native-loader build crashed in
> UnityMain immediately after `UNITYFS: API ready`. Until the failing native
> call is isolated, `Enabled=false` is the safe default. See
> `PICK3_UNITYFS_CRASH_ISOLATION.md` before enabling it again.

# PickFromThree native UnityFS static assets (2026-10-19)

This is a **static-art loader integration test**, not an Android-verified release.
It preserves the previously working `NativeAvatarCrashIsolation=true` mode,
and does not call `CardViewFactory`, instantiate inspection prefabs, or perform
scene-wide sprite enumeration.

## Assets and provenance

`src/pick3-unityfs-originals/` holds seven original bundles from the user's
`files.zip` and `downloaded-dependencies.zip`, with SHA-256 checksums in
`manifest.json`. `src/Pick3BundledUnityFS.cpp` embeds those unmodified UnityFS
bytes so no extra APK-assets extraction hook is required. Regenerate it with:

```powershell
python tools/regenerate_pick3_unityfs_embeds.py
```

The bundles contain these original assets:

| Slot | Exact asset name requested |
| --- | --- |
| Circle | `assets/ingameart/sprites/meta/fullscreenview/frame_circle_grass.png` |
| Circle | `assets/ingameart/sprites/meta/miniinspectionview/smallinspectionframe.png` |
| Sun | `assets/ingameart/sprites/bases_frames/inhnd_sun_gb.png` |
| Brain | `assets/ingameart/sprites/boards/ui icons/ui_brain.png` |
| Attack | `assets/ingameart/sprites/bases_frames/attack.png` |
| Heart | `assets/ingameart/sprites/bases_frames/heart.png` |
| Button | `assets/ingameart/sprites/meta/buttons/button_tint.png` |
| Colors | `assets/ship/frameassets/defaultframecolors.asset` |

`Pick3LocalAssetBundles` uses the app-private path
`/data/user/0/com.ea.gp.pvzheroes/files/sprig/pick3-native-bundles/`
without invoking `UnityEngine.Application.get_persistentDataPath()` (the leading
suspect in the crash). This specific path is already used by Sprig and is
verified to exist at runtime before files are written.
That is private app storage, **not** the game's downloaded content cache.
It verifies the complete bytes before reusing a stored copy. Native assets are
requested by explicit path through `AssetBundle.LoadAsset(name, Type)` on
Unity's main thread. A missing Sprite subasset **does not** call Texture2D or Sprite.Create by
  default. `AllowTextureToSprite=true` enables this additional, previously
  untested native API branch only when explicitly requested.

Bundles and loaded objects remain rooted for the process lifetime. The manager
never calls `Unload(true)` while UI may reference an object. It tries each
static asset at most once, and does not scan game scenes on failure.

### Existing PNG compatibility fallback

The exact source-extracted grass, rim, sun and heart PNGs are retained for
compatibility if the native asset is missing on this Unity build. The
brain and attack PNG substitutes are **not** used as native-asset fallback.
The description plate and inline-tooltip background remain legacy UI artwork;
we have not confirmed a matching original serialized Sprite. Those parts are
**not** claimed to be native UnityFS in this release.

### Class-color and Choose-button status

In experimental ProbeMode 4, the original `smallinspectionframe` native Sprite
is rendered through two `UnityEngine.UI.Image.Type.Filled` half-rim layers,
with verified RGB values recovered from the real `DefaultFrameColors` asset.
One-class cards tint both halves alike; two-class cards split left and right;
zero/more-than-two classes remain white. This uses native sprites and built-in
UGUI fill functionality but is **not** a claim to reproduce the original
CardInspectionPanel colorizer, shader, prefab animations or materials.
The original `button_tint` is used where a `Sprite` subasset resolves, with a
muted green UI tint and selected-state tint. Its source prefab animation and
pressed transitions have **not** been reimplemented; nine-slice inspection is
separately opt-in (`EnableButtonSlicing=false` by default). A native
`ScriptableObjectColorScheme` object can also be loaded with
`LoadNativeColorScheme=true`, but the verified RGB values don't require that
additional risky managed call.

### Safe probe modes (2026-10-19 update)

`ProbeMode=1` checks the bundled files; `ProbeMode=2` loads only the
original circle UnityFS bundle; **`ProbeMode=3`** isolates the original grass
Sprite's typed `LoadAsset` call and attaches the result to the existing three
grass `Image` components. A Sprite miss falls back to the verified original
PNG; probe 3 explicitly does not call `Sprite.Create` or load other assets.
**`ProbeMode=4`** is the new opt-in multi-asset resolver, still **NOT tested
on Android**. Grass-only mode was verified on October 19, 2026. The full mode
has a hard cap `MaxAssetIndex=0..6`: 0 grass, 1 rim, 2 sun, 3 brain,
4 attack, 5 heart, 6 reward-choice button. During first testing, use index 1,
then increase after logs demonstrate stability; the `ASSET_BEGIN` checkpoint
helps identify a failing native call. `TryInvoke` cannot catch a native
SIGSEGV, so this is crash isolation, not a crash-proof guarantee. The direct loader remains disabled by
default after the prior crash. See `PICK3_GRASS_SPRITE_PROBE_20261019.md`.

### Configuration and diagnostics

```ini
[Pick3OwnedUnityFS]
Enabled=false
ProbeMode=3
MaxAssetIndex=6
AllowTextureToSprite=false
LoadNativeColorScheme=false
EnableSplitRim=true
EnableButtonSlicing=false

[PickFromThreePreview]
NativeAvatarCrashIsolation=true
NativeAvatarFactoryProbe=false
NativeAvatarLoadTest=false
NativeAvatarRenderTest=false
```

The first PickFromThree opening should produce logs such as:

```text
Pick3/UNITYFS: API ready=1
Pick3/UNITYFS: BUNDLE_READY name=circle
Pick3/UNITYFS: SPRITE_READY index=0 name=frame_circle_grass source=native-sprite
Pick3/UNITYFS: SPRITE_READY index=1 name=smallinspectionframe source=native-sprite
Pick3/UNITYFS: COLOR_SCHEME_READY=1
```

`SPRITE_MISSING` means the corresponding typed Unity load did not resolve; it
is **not** a reason to turn avatar rendering back on. If a crash occurs during
bundle loading, set `Pick3OwnedUnityFS.Enabled=false` and reproduce it with
full logcat before making another loader change.

## Patch / cleanup

For the *update-only* ZIP, extract the `CustomComponents/` files into the
parent directory containing your existing `CustomComponents` folder. This
updates only listed files. Then, from **inside that folder**, use:

```powershell
.\Clean-Pick3Junk.ps1 -WhatIf
.\Clean-Pick3Junk.ps1
```

The cleanup is an exact allowlist. It does not delete `.git`, game data,
other custom components, or any active asset embedded by the generator.

## Test order

1. Test a card with no portrait: the original grass/frame must remain visible.
2. Test cost icons for both factions, attack/heart, and the button texture.
3. Open/close the picker several times without native avatar loading.
4. Test a single-class, dual-class and classless card; the current uncolored
   native ring is intentional until the stock panel colorizer is transferred.
5. Capture logcat before changing any animation-loading flags.
