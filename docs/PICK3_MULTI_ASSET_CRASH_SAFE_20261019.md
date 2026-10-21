# Pick3 original UnityFS static UI — multi-asset experiment

This update is based on the **verified** grass Sprite ProbeMode 3 branch.
A previous unguarded loader crashed at `SIGSEGV` address `0x38`, immediately
after `UNITYFS: API ready`; it was remedied by removing
`Application.get_persistentDataPath`, defaulting off, and adding checkpoints.
Never infer that a host syntax pass guarantees managed/native ABI safety.

## Included assets, no image recreation

The same seven original UnityFS bundles are retained unchanged: grass/rim,
sun, brain, attack, heart, reward-choice button, and frame color scheme.
Full mode requests original **Sprite** subassets; if one is absent, it logs
`SPRITE_MISSING`. The proven extracted grass/rim/sun/heart PNGs remain
fallbacks, not counterfeit new artwork. No imitations for brain or attack.
`AllowTextureToSprite` stays **false** because `Sprite.Create` is another
untested managed/native path. Non-game description/tooltip backgrounds
remain as in the prior baseline and are not claimed to be native.

## New native Unity UI

- Two halves of `smallinspectionframe`, rendered with UGUI Image `Filled`
  mode and original class RGB from `DefaultFrameColors`. This is an
  approximation of the native colorizer **geometry**, not exact game materials.
- Original `button_tint` Sprite where present, colored for normal/selected
  states. Stock pressed-state animations aren't reproduced.
- Both faction costs, attack and heart are resolved from their UnityFS slots.
- No native avatar/prefab construction; all prior crash isolation remains.
- Process-lifetime rooted cache remains, no per-popup Sprite cache clearing.

## Test strategy

1. Supplied `config.ini` keeps `Enabled=true; ProbeMode=3` to return to the user-verified grass-only
   behavior. No new native calls execute in ProbeMode 3.
2. Full multi-asset experiment (not Android-tested): `Enabled=true;
   ProbeMode=4; MaxAssetIndex=1; AllowTextureToSprite=false;
   LoadNativeColorScheme=false; EnableButtonSlicing=false`. Test ring, then
   grow `MaxAssetIndex` to 2,3,4,5,6 after success. Log last `ASSET_BEGIN`
   if a native crash occurs. Keep `EnableSplitRim=true` for tint testing;
   set false to isolate UGUI fill setup from native Sprite loading.
3. Do not activate native animated avatars. If crash occurs, switch back to
   `ProbeMode=3` or `Enabled=false` and retain logcat/tombstone.

This is one integrated source update, not a claim that all seven new asset
invocations have been validated at runtime. Source/build checks are separate.
