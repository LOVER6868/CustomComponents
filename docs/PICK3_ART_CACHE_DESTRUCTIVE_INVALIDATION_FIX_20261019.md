# PickFromThree: do not destroy a Sprite used by the open UI (2026-10-19)

## Evidence from the user's 19:15–19:17 log and video

The previous lifetime patch logged `ART_CACHE: CREATE kind=1 rooted=1 persistent=1` and then `ART_CACHE: INVALIDATE kind=1 reason=sprite-or-texture-unloaded` within milliseconds, *during the same popup construction*. Kinds 0/1/5/6/7 were repeatedly created and invalidated in tight succession, increasing the first opening to 563 ms. The video shows portrait/description areas switching between white rectangles and normal artwork. The native-backed `ICON_USE type=0/1` badges persisted.

The log does not establish that the game was genuinely unloading those assets in milliseconds. The liveness predicate (one or more of UnityEngine.Object.op_Implicit, Sprite.get_texture, and the backing-texture check) may be returning a false negative. The previous code treated any negative as license to call `UnityEngine.Object.Destroy()` on a Sprite/Texture that existing `UnityEngine.UI.Image` components still reference. Unity defers destruction, so an Image may display a white default rectangle rather than its expected artwork.

## Change

- Generated PNG Sprite/Texture pairs are rooted for the process lifetime and retain HideFlags.DontUnloadUnusedAsset, as before.
- A successful rooted cached pair is now **reused**, not destroyed and rebuilt on each access.
- Each art kind logs one non-destructive `ART_CACHE: CHECK` snapshot of the old Unity-native validation path. Negative readings do **not** delete a Sprite used by the popup.
- No UnityFS/AssetBundle loading or native avatar rendering was enabled. The game-owned icon registry and selection logic are unmodified.
- If a GC root is genuinely missing, log `ART_CACHE: ROOT_MISSING` rather than destroying images still in use. Such an event is not expected.

## Test

1. Rebuild the APK with this patch.
2. Open a PickFromThree in the first match; record all `ART_CACHE` messages and a screenshot.
3. Open several popups in the same match. Ensure `ART_CACHE: INVALIDATE` no longer repeats, and look for `ART_CACHE: REUSE` instead.
4. End the match, start another *without restarting the app*, and repeat. Check all three card portraits, grass/rims, description panels, and stat badges.
5. If any rectangles remain, send screenshot/video and log with the new `ART_CACHE: CHECK` diagnostics. We can then separate cache stability from card-face provider Sprite lifetime, masking, and draw-order issues. Do not conclude that native textures survive a scene transition solely from the absence of invalidations.

Verification: host C++ syntax checked; not yet compiled into Android APK or tested on device.
