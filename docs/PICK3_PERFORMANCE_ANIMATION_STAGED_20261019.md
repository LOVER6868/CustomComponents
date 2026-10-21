# PickFromThree performance + native animation staged experiment (2026-10-19)

## Baseline and safety

This update is based on **CustomComponents-Pick3-ConsolidatedStatic-Full.zip** (the working static UI reported by the user). It **does not** change the pinned, non-destructive PNG Sprite/Texture cache; it retains game-owned stat sprites, trait deciders, class-color tinting, the real selection/submission flow, and the stable fallback portraits. Original game AssetBundles are **never loaded manually** (`[Pick3OwnedUnityFS] Enabled=false`, `ProbeMode=0`).

Native `CardViewFactory.CreateCardAsync` previously caused an Android native IL2CPP SIGSEGV (`0x38` signature). This remains **unresolved**. A managed exception handler cannot make a native SIGSEGV safe. All native factory experiments remain **OFF by default**, even though the alternative read-only preflight runs.

## Performance work

- The CPU-only embedded PNG verifier now includes `InspectionRimLeft` and `InspectionRimRight`, rather than silently skipping them. No Unity methods are invoked in the menu verification stage.
- After a sustained match is detected, the PNG-only prewarmer prepares **8 verified shared assets**, one per configured step: cost sun, health heart, description plate, tooltip plate, class rim left/right, grass, and reference rim. No native AssetBundle loading is performed. All preparations reuse the existing pinned cache.
- The expensive Unity `FindObjectOfType` check for in-match gating is now sampled once per 12 frontend frames rather than every tick; the stable-match delay is preserved approximately. No new Unity calls were added to menu startup.
- The existing `PERF: PRELOAD`, `PERF: CARD_UI`, `PERF: METADATA`, and `PERF: OPEN` diagnostics are retained. Per-kind `PREWARM: MATCH_PNG` elapsed times show the cost of work moved out of the popup.
- **Important limitation:** Unity Sprite creation runs on the Unity main thread. Moving grass and rim into match warm-up may still cause a brief *in-match* spike on individual steps. It reduces first-popup latency but does not magically turn synchronous native Unity work into a background task. UI Canvas creation still occurs when the popup opens; this update does not claim a full 80 ms first opening.

## Animation levels

`[Pick3AnimationExperiment] Stage` provides progressive testing. These are **experiment gates**, not proof that animated preview construction is safe on your Android build.

| Stage | Behavior | Risk |
|---|---|---|
| 0 | No animation work | Working static baseline |
| **1 (default)** | Read *one* selected card's already-loaded metadata and check class availability; **never** call `CreateCardAsync`, never allocate an RT/camera | Read-only preflight |
| 2 | One candidate: native `CardViewFactory.CreateCardAsync` request into an off-screen scene-owned parent, **no RenderTexture** | Known unresolved native crash risk |
| 3 | One candidate: same native request, optional camera/RenderTexture and UGUI RawImage only after rendered frames exist | Native crash and rendering risk |
| 4 | Up to 3 candidates: make requests in sequence, never simultaneously in one tick; require prior completion, and keep static fallback | Highest native/rendering risk |

Stages 2–4 are *doubly gated* by setting BOTH:

```ini
[PickFromThreePreview]
NativeAvatarCrashIsolation=false

[Pick3AnimationExperiment]
Stage=2
AllowUnverifiedFactoryCalls=true
FirstOption=1
SpacingFrames=120
RenderEveryNTicks=2
```

If either safety gate is absent, requested Stage 2–4 **downgrades to Stage 1** and logs `ANIMATION_EXPERIMENT: BLOCKED`. The legacy `NativeAvatarFactoryProbe`, `NativeAvatarLoadTest`, and `NativeAvatarRenderTest` switches no longer secretly override the new experiment gate. Stage 3 and Stage 4 additionally check `AVATAR_RENDER: INIT ready=1` before invoking the factory.

The stock `CardViewFactory` is only called **after** the static UI is open, and it cannot block *initial* popup construction. The optional RawImage starts disabled and is enabled only after the render target reports available frames, so a missing avatar keeps the static card face. The existing session tickets, cancellation, card asset unloader, and static fallback remain. No extra game bundles or avatars are loaded during prewarm.

### Native experiment caveats

- A failed or stuck asynchronous callback can stall later animation requests. For safety, the staged runner **stops rather than starting another concurrent factory request**. Look for `ANIMATION_EXPERIMENT: STALLED`.
- Stage 2 deliberately provides *no visible animation*: it is a dangerous factory-creation diagnostic only. Complete that device test **before** enabling Stage 3.
- The current renderer is experimental and uses per-option 512×768 RenderTextures. The default render rate is one paint every two EventSystem ticks, to reduce GPU cost; this does not guarantee a steady frame rate.
- Some tricks or environments may not have looping avatars. The existing in-hand sprite handling is a fallback. Animated card previews **are not guaranteed to work** just because these stages compile.
- Switching `NativeAvatarCrashIsolation=false` exposes a path already associated with a fatal Android SIGSEGV. Back up a known-good build first, and test locally; if it crashes, revert both switches and reinstall.

## Suggested device test progression

1. **Safe build (default):** `Stage=1`, `NativeAvatarCrashIsolation=true`, `AllowUnverifiedFactoryCalls=false`. Enter a match; leave it running long enough for 8 `PREWARM: MATCH_PNG` stages. Open PickFromThree three times. Confirm `ANIMATION_EXPERIMENT: PREFLIGHT ... nativeCalls=0`, static art is correct, no duplicate bundle collision, and compare first vs later `PERF: OPEN` values.
2. Finish and start another match **without restarting**. Verify `ART_CACHE: REUSE` and no cache invalidation loop, stock card inspection opens, PickFromThree remains responsive, and selection completes.
3. **Only if you accept the known crash risk**, in a separate diagnostic APK enable the two gates and use Stage 2. Note the last `AVATAR_LOAD: START_STEP` and `AVATAR_LOAD: REQUEST_RETURN` lines. If this build crashes, **do not** escalate to Stage 3/4.
4. If the single-card factory request succeeds and its callback/cleanup are verified, set `Stage=3`. Check `AVATAR_RENDER: STAGE`, `READY`, and `UI_BOUND`. The static portrait must remain when unavailable.
5. If one animated card renders stably through a match transition, set `Stage=4`. Watch for per-option `REQUEST`, `VISUAL_READY`, `RELEASE`, and no `STALLED` warnings. Check for overheating/GPU lag with three RenderTextures.

## Log checkpoints

```
PREWARM: CPU_VERIFY kind=8 valid=1
PREWARM: CPU_VERIFY kind=9 valid=1
PREWARM: MATCH_PNG stage=4 kind=8 present=1
PREWARM: MATCH_PNG stage=5 kind=9 present=1
PREWARM: MATCH_PNG stage=6 kind=5 present=1
PREWARM: MATCH_PNG stage=7 kind=6 present=1
ANIMATION_EXPERIMENT: CONFIG stage=1 nativeFactory=0
ANIMATION_EXPERIMENT: PREFLIGHT option=1 ... nativeCalls=0
PERF: OPEN totalMs=...
```

Successful C++ syntax-only compilation establishes *source integration*, **not** successful Android rendering or native factory safety. No Android execution was performed here.
