# PickFromThree — maintained guide

This is the single maintained PickFromThree presentation and diagnostics document.
Old dated investigations are intentionally removed instead of scattered across
the module. The other **non-PickFromThree** component references remain in
`docs/reference/`. The source baseline for this update is
`CustomComponents_FactoryDI_Staged_BuildFix_Full.zip`, which includes the tested
factory lookup, staged DI gate and successful three-card preview.

## What it does

- Renders three eligible card choices, with cost, name, attack/health (when
  applicable), localized ability description, and Choose buttons.
- Selects through the existing Pick3 gameplay/submission bridge. PvP remains
  disabled by default pending authoritative validation.
- Uses game-owned `CardViewFactory` + `CardAssetUnloader` to populate native
  `CardAvatarViewComponent` previews. Each option has a separate 512×512
  RenderTexture and disabled Unity Camera; only explicit camera Render() calls
  update the texture. Do not destroy or globally cache borrowed scene views.
- For unavailable artwork, static game-owned sprites remain the fallback.
- Displays only **one** localized targeting prompt (no duplicate English
  "Choose a card..." header). Its green Cafeteria-style face/dark shadow is
  styled after the original game's targeting line, without copying or
  modifying the underlying game's global targeting widget.
- Positions cost badges on the **right** of the portrait.

## Configuration

All release-facing settings live together in the root `config.ini`.

| Section | Purpose |
|---|---|
| `PickFromThree` | Gameplay selection, PVE/PVP policy |
| `PickFromThreePreview` | Popup enablement, timing, close button |
| `Pick3Overlay` | Battlefield darkening opacity (0–100%) |
| `Pick3Screen` | Fixed reference resolution |
| `Pick3TargetingText` | Localized targeting prompt style / absolute position |
| `Pick3RewardLayout`, `Pick3StatText` | Art/labels/spacing and typography |
| `Pick3Description` | Auto-fitted cream description panel |
| `Pick3Performance` | FPS targets, global render budget, load spacing |
| `Pick3NativeArtwork`, `Pick3CardViewCamera` | Per-card cropping, zoom and center |
| `Pick3InlineTooltip` | Rich tooltip sizing/timing |
| `Pick3GameOwnedAssets`, `Pick3NativeClassArt`, `Pick3OwnedUnityFS` | Stock-style fallback and crash-safe bundle settings |
| `Pick3Stage8Isolation` | Gameplay hook isolation |

### Battlefield darkening

`[Pick3Overlay] BattlefieldDarkeningPercent=42` changes the opacity of the
fullscreen dark veil over the battlefield, with 0 meaning no visual dimming,
42 retaining the original normal popup look, and 100 meaning fully opaque.
The selected choice, card portraits and targeting text render above the veil.
The veil still blocks touches when opacity is zero. Its dismissal fades from
the configured opacity. This is a presentation parameter read at mod startup.

### Absolute UI coordinates

`[Pick3Screen] ReferenceWidth=720, ReferenceHeight=1280` establishes authored
screen-space units with origin at the display center, X to the right and Y up.
Values are scaled by `min(actualWidth/720, actualHeight/1280)`. They do not
stretch independently by the device's aspect ratio. Side cards/top card use
card-relative percentages of this fixed reference canvas, and child labels
use card-local geometry; full-screen dimming naturally still fills the actual
viewport. This is a reliable reference coordinate system, **not** a guarantee
of collision-free placement on every extreme display cutout or aspect ratio.

To move the prompt, change `[Pick3TargetingText] PositionX` and `PositionY`
(reference units, not percentages). `FontSizePercent` is relative to its
reference font. The text is localized via `TryPickFromThreeTargeting`; the
shadow receives the same translated text.

### Native rendering and 60 FPS targets

`[Pick3Performance] EnableNativeArtwork=true` enables the three-card path
already validated with factory probe Mode 7 / DI Stage 5, without needing the
factory-stage ADB files. Set it to `false` to return to static portraits. A
nonzero factory probe selector remains an explicit diagnostic override.

`[Pick3Performance] FighterFps=60 EnvironmentFps=60 OtherFps=60`
sets **per-card ceilings**, not a promise of measured device FPS.
`GlobalRenderFpsCap=190` permits the 180 RenderTexture updates/s needed for
three 60-FPS previews and leaves slight scheduling headroom. One Unity
presentation tick can render all three cameras if they are due. Real FPS
depends on UnityMain update cadence, avatar animation sources, Samsung GPU
headroom, and 512x512 texture cost. FPS may fall below 60 if the event loop
runs below 60 Hz; more render calls do not accelerate animation time.

`LoadSpacingFrames=0` removes the old forced 120-frame (~4s) pause, allowing
one new native card request on each successive active presentation tick.
Callbacks remain asynchronous; separate slots and tickets prevent the three
choices from writing to the same texture.

### Camera framing for unusual cards

Native prefab positions and sprite pivots differ across fighters, Banana
Launcher, tricks and environments. The camera **does not alter the game's
prefab transform**; it fits the measured dominant body renderer and applies a
bounded 1.6× adaptive zoom only when this will not clip the art. Adjust:

- `AdaptiveFraming` / `AdaptiveZoomMaxPercent` for small rigs
- `FighterCameraYOffsetPercent` for fighters whose body pivot is odd
- `OtherCameraYOffsetPercent` for tricks and nonfighters
- `EnvironmentCameraYOffsetPercent` and `EnvironmentZoomPercent` for long rigs
- `FighterBaselineOrthoHundredths`, `EnvironmentBaselineOrthoHundredths`,
  `OtherBaselineOrthoHundredths` for different baseline frame sizes

A true pixel-for-pixel match for **every** card still needs on-device
comparison with stock inspection, since we do not inherit its entire view
hierarchy or camera controller. Avoid applying a one-off world-space fix to
all card types.

## Chosen-card animation (configurable)

In `[Pick3ChoiceMotion]`, the defaults are a **400 ms exponential glide**
(`GlideExponentHundredths=350` = 3.50), a **70 ms center hold**, then a
**180 ms fade** with the existing puff effect. The unchosen cards and chosen
button dismiss separately over **240 ms**, overlapping the first half of the
glide. The selected card reaches the center before fading. For a less front-
loaded glide, lower the exponent to 225–300 or increase `GlideMilliseconds`.
`LowerCardArcPercent=5` adds a shallow upward lift for cards starting below
the screen center; the top card slides downward instead. `PuffPercent=31`
controls the scale increase during fading.

`PressFeedbackMilliseconds` remains the gameplay submission delay, but is
**automatically extended** if needed to cover the total glide + center hold +
fade (+20 ms guard). This prevents the gameplay submission from closing the
popup before the selected card's travel is visible. Normal exit fade still
runs afterward.

## Targeting instructions

The prompt uses the source card's **exact** localization key:
`<PrefabName>_PickFromThreeTargeting` (not the generic `_Targeting`). It
resolves the source prefab using a safe, copied CardDataMapper snapshot first;
it does not depend on the optional native sprite provider being enabled.
`[Pick3TargetingText]` controls its position and font scale. Until the
localization mapper has that entry, a temporary generic prompt may appear;
check for `TARGETING_LOC` in Sprig logs to verify localization resolved.

## Runtime diagnostics (requires an installed build with the selectors)

Use PowerShell:

```powershell
$base = "/sdcard/Android/data/com.ea.gp.pvzheroes/files"
adb shell "echo 7 > $base/pick3_factory_probe.txt"
adb shell "echo 5 > $base/pick3_factory_di_stage.txt"
adb shell am force-stop com.ea.gp.pvzheroes
```

| `pick3_factory_probe.txt` | Behavior |
|---|---|
| 0 | Off / static fallback |
| 1–4 | API, DI, signature and readiness diagnostics |
| 5 | Native card creation without the full portrait test |
| 6 | One rendered preview |
| 7 | Up to three independent preview requests |

DI stage 0–4 stops before creating a card; stage 5 permits creation.
Mode 7 + stage 5 is the successfully tested **diagnostic** path. Normal
presentation now uses the same renderer when `EnableNativeArtwork=true`;
this automatic configuration still requires validation on device. The `ANIMATION_EXPERIMENT stage=4`
line is a different setting from `FACTORY_TEST: mode=7`. If a mode crashes,
restore `pick3_factory_probe.txt=4` and restart.

To collect results:

```powershell
adb logcat -d -v time |
    Select-String "FACTORY_TEST|FACTORY_DI|AVATAR_RENDER|AVATAR_LOAD|Fatal signal" |
    Select-Object -Last 180
```

## Asset files and cleanup

**Do not remove `assets/pick3-static/*.png`.** They are direct inputs to
`tools/regenerate_pick3_static_embeds.py` and to compiled fallback assets.
Obsolete *reference copies* in `docs/native-static-art/` are unnecessary.

From your `Bloom/CustomComponents` folder run:

```powershell
.\Clean-Pick3Junk.ps1 -WhatIf
.\Clean-Pick3Junk.ps1
```

The cleanup script removes dated duplicate PickFromThree notes, obsolete
root-level diagnostic text files, unused reference-art PNGs and obsolete
legacy sample portrait files, where present. It does **not** make backup
folders, walk outside the module, delete its `.git`, delete gameplay source,
or delete source PNGs used by static-embed generators.

## Known limitations

- Performance is configured for 60 Hz per card, but real 60 FPS on three
  animated card avatars is not device-verified.
- Some sprites contain visual effects/bounds that do not match stock inspect
  without further card-type or per-card camera tuning.
- Gameplay submission is not authorized for PvP until both clients and the
  server validate the chosen GUID.
- The staging and cleanup changes need **Windows/Android NDK compilation and
  on-device visual verification** before being treated as release-ready.
