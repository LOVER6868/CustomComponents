# PickFromThree presentation patch

- Three native requests are now started without waiting for prior option callbacks (asynchronous requests; the underlying factory still processes on Unity main thread).
- RawImage is no longer a child of a circular stencil mask, and is sized larger than the class rim.
- Config defaults 30 FPS fighters, 24 FPS tricks/environments, 90 total camera-render upper bound; renderer still uses one camera render per eligible tick. Actual FPS depends on game frame rate.
- Bounds fitting uses a robust representative renderer and a proximity-limited cluster rather than a single unverified dominant sprite.
- Choose button fallback now uses an opaque green rectangular UGUI Image, not the missing native button_tint sprite. Genuine native button graphic is still unavailable until the UnityFS source can be safely loaded.

## Caveats
The native avatar hierarchy can still modify animation bounds between frames, and this patch has not been device-tested. Starting all three requests together can increase a frame-time spike when prefabs instantiate. Keep a backup of the last working module.
