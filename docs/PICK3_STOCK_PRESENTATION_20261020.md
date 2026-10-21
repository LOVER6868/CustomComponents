# PickFromThree stock presentation / animation patch (test build)

## What changed

- Native avatar 256×256 RenderTextures now display in **square** RawImages; the earlier 165% vertical height stretched characters.
- All three stage-4 requests are started without the previous option callback barrier (`SpacingFrames=0`). This begins requests quickly, not synchronously; actual Unity asset loading remains asynchronous.
- Render scheduler now allows up to three due cameras per UI tick, instead of one camera total. Each option tracks its own paint timestamp. There is a shared 180-render/s rolling safety cap (60 per fighter; 45 per environment/trick), so real rates can be lower on slower devices.
- Stat numbers now use **non-autosizing, unwrapped labels** as in the stock inspection prefab. Name label shrink range is tighter; description text uses mild negative letter and line spacing when TMP setter methods exist.
- Disabled UnityFS bundle loading no longer prevents embedded brain/attack fallback sprites. Stock registry sprites are still preferred.
- A new rounded green *owned PNG* Choose button is bundled and displayed even with UnityFS disabled. This is a styled fallback, **not** the authentic Unity UI button prefab.
- The existing camera's heuristically selected sprite bounds remain a limitation. We do not claim perfect stock-equivalent camera geometry: the stock inspection prefab renders in a different world-space hierarchy.

## Stock asset measurements used for reference

- Stock inspection attached scale 0.333, fighter offset (0,-0.5,0), nonfighter offset (0,-0.58,0), parent scale 40. Not pasted directly into the RenderTexture cameras: their coordinate systems differ.
- Inspection cost/attack/health TextMeshPro fixed 18, no auto sizing. Card name bold / auto 70–90 and description 38–50 with tightened spacing in the stock panel. The smaller Pick3 layout uses scaled sizes.
- Stock sprite source geometry: sun 122×127 / 50 PPU, brain 256×256 / 100 PPU, attack 116×108 / 50 PPU, health 113×103 / 50 PPU; images preserve aspect in Pick3.
- Cafeteria Black - HQ Labels material was previously identified, and the module retains its resident Cafeteria font + material lookup instead of copying shader assets.

## Known issues / testing

This ZIP is source-only and has not been compiled into Android APK or tested on a device. Native factory calls remain an opt-in and may crash some cards. Because the renderer uses async callbacks and native engine methods, check logcat for `AVATAR_LOAD`, `AVATAR_RENDER` and `ART_CACHE` after rebuilding. Back up a known-good module.
