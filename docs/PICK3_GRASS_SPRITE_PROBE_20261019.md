# Pick3 native grass Sprite-only probe

Baseline: `CustomComponents-Pick3-UnityFS-CrashRecovery-Full.zip`.
ProbeMode 1 extracted all seven bundle files without loading Unity assets.
ProbeMode 2 successfully called `AssetBundle.LoadFromFile` for the circle
bundle, with multiple completed choices. ProbeMode 3 is the first unsafe
`LoadAsset(string, Type)` trial, isolated to one original grass Sprite and
reusing the same Image component creation path as the verified PNG fallback.

## How to test

Keep avatar crash isolation enabled. In config.ini:

```ini
[Pick3OwnedUnityFS]
Enabled=true
ProbeMode=3
```

Rebuild and run PickFromThree once. Expected checkpoints:
`CONFIG`, `BUNDLE_READY name=circle`, `SPRITE_BEGIN index=0`,
`ASSET_BEGIN` / `ASSET_END`, `SPRITE_READY` or `SPRITE_MISSING`,
`GRASS_APPLY_READY` (the latter only when grass was available from
either native Sprite or original PNG fallback), plus `PERF` breakdown.
On native miss, this **does not** attempt Texture2D / Sprite.Create; only
the original grass PNG is displayed. It never loads other native sprites
or colors and never initializes card avatars.

Do not enable mode 4 yet. If the process crashes, send logcat/tombstone
and switch to ProbeMode 2 or Enabled=false. Neither compilation nor
inspection of the bundle can guarantee managed ABI safety at runtime.

## Potential freeze addressed

The previous `Open()` cleared GC-rooted texture/sprite cache pointers and
caused repeated PNG decode / Sprite.Create calls, potentially on each card
play. This patch retains the roots and pointer cache across openings. The
new timing logs measure font lookup, preload, card UI build, and metadata.
The UnityFS circle bundle cache is still process-lifetime as before.
