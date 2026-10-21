# PickFromThree native animated-portrait performance experiment

Edits in this archive:
- Render targets changed from 512x768 to 256x256 per option (~0.75 MiB for three RGBA8 color buffers vs ~4.5 MiB previously, excluding depth/mips).
- Native artwork now uses a sprite-alpha `UnityEngine.UI.Mask` created from the existing InspectionGrass sprite. If Mask creation fails, the static sprite is left intact; an unmasked native RawImage is not created.
- Render pacing: at most one avatar is updated per eligible tick, capped at a configurable 60 total CPU-submitted camera renders per second by default. This is about 20 fps per card with three active, subject to UI-tick frequency and GPU performance. The initial render is still immediate, and the white-quad guard is preserved.
- Logged `FIRST_RENDER` CPU submit durations and sampled `PERF` timing every 30 subsequent renders. These are CPU-call measurements, not GPU timing queries.
- Default UI dimensions now 100% card width, square. Inspect individual figures to tune view size and camera zoom.

No native factory, animation, lifecycle, or ability code has been intentionally changed. The existing per-option camera remains; collapsing to one shared camera requires a separate transform/state rework and is not necessary for these first optimizations.

## Test
Build in the original Sprig Windows repo with this folder replacing its CustomComponents module. Test one animated option (Stage=3); confirm `MASK_UNAVAILABLE` does NOT appear, `UI_BOUND ... masked=1` appears, and `FIRST_RENDER` has reasonable timings. Inspect animation and select/exit popup repeatedly. Test Stage=4 only after Stage=3 is stable.

## Limitations
The Mask depends on correct transparency in InspectionGrass; if its alpha is opaque, it may clip square instead of circular. The asset should be verified visually on-device. The camera is fixed from the initial renderer bounds; extreme root-motion animations may still need per-card offset tuning. No Android native build or device rendering was available here.
