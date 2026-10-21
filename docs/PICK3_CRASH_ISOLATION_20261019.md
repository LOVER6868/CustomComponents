# PickFromThree native-avatar crash isolation (2026-10-19)

## What the uploaded log confirms

Latest crash: 2026-10-18 22:16:50, UnityMain SIGSEGV/null dereference at 0x38.
IL2CPP offset 0x17c7ee4, via BNM::TryInvoke and libsprig. The final
PickFromThree lines were `PROBE_START guid=462`, `FACTORY_READY option=1
guid=462`, followed by Unity SpriteAtlasManager warnings. No `AVATAR_RENDER:
STAGE` / `AVATAR_LOAD: REQUEST` occurred in this session. An earlier
crash in the same log predates the stabilization build and is not evidence
of the latest code failing at that earlier UI refresh point.

The exact failing managed call is *not yet confirmed*. A C++ syntax check
cannot find a runtime Unity/IL2CPP null dereference. The previous
anti-scan and tooltip guards are not sufficient for this pre-render crash.

## Default isolation mode

The new `NativeAvatarCrashIsolation=true` master gate is on by default,
even if old per-device configuration leaves individual switches enabled.
The following PickFromThreePreview settings are disabled by default:

- `NativeAvatarFactoryProbe=false`
- `NativeAvatarLoadTest=false`
- `NativeAvatarRenderTest=false`

NativeSpriteCacheTest and NativeSpriteCacheRequestArt remain configured as
before, so genuine static card-face art can still appear. The character
animations will **not** appear in this isolation build. The popup,
localization, selection, tooltips, button design, and class-circle visuals
are otherwise unchanged. This is a diagnostic safety tradeoff, not a
completed fix for animated previews.

## Test sequence

1. Rebuild/install, enter PvE, trigger PickFromThree, close and reopen
   several times, and tap keyword tooltips repeatedly.
2. If this still crashes, disable `NativeSpriteCacheTest` and
   `NativeSpriteCacheRequestArt` too and retest; this isolates the native
   sprite cache. (Unavailable card pictures will stay blank.)
3. If stable, set `NativeAvatarCrashIsolation=false` and enable
   `NativeAvatarFactoryProbe=true` **only**. Keep
   NativeAvatarLoadTest/NativeAvatarRenderTest false, then retest.
4. If that is stable, enable `NativeAvatarLoadTest=true` and
   `NativeAvatarRenderTest=true` together for a controlled PvE test. New
   `AVATAR_LOAD: START_STEP` messages identify whether the failing call is
   metadata lookup, factory DI, unloader DI, position, or rendering stage.
5. Share the full new exception log from the first failure. Do not assume
   a missing SpriteAtlasManager listener is the direct cause.

Do not enable `RequestNativePrefabExperimental` during isolation.

This revision does not change targeting, gameplay submission, native
animation behavior itself, or localization lookup.
