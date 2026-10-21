# PickFromThree stock-parity audit and stable framing (2026-10-20)

## Implemented here
- Stock CardViewFactory creation, inspection avatar/motion, and game CardAssetUnloader are retained. No custom fake 3D avatar is substituted.
- The game-authored avatar hierarchy, scale, sprite renderers and animation components are untouched.
- The isolated portrait camera uses a **stock-inspired stable inspection baseline** instead of renderer-bound-derived zoom. Separate baselines for fighter, environment, and other; opt-out restores legacy bounds framing.
- The representative renderer remains useful for camera center and clipping safety. Safety expansion uses the visible representative bounds, but cannot promise a particle never extends beyond a finite RenderTexture.
- Existing unmasked 250% RawImage viewport and 512x512 transparent RT stay. The class plate is decorative, not a clip mask.
- Existing stock-derived UI text/icons/traits, selection submission, match-scoped PickedFromThree reset and latest-wide-viewport audio trace script are preserved.

## Configuration
[Pick3CardViewCamera] UseStableInspectionFraming=true;
FighterBaselineOrthoHundredths=700; EnvironmentBaselineOrthoHundredths=900;
OtherBaselineOrthoHundredths=750; SafetyOverscanPercent=120.

## Not claimed stock-exact
- The game inspection view's `AttachedCardScale=0.333` and anchor transforms are scene-space semantics. Our separate offscreen cameras are not the stock inspection camera, so copying those numbers into the ortho size would be incorrect. The baselines are an approximation subject to device/screenshot tuning, **not extracted stock camera constants**.
- The independent popup and Choose buttons are custom UI; they are not instantiated stock inspection view prefabs.
- Native stock audio events are not yet empirically verified. `tools/trace_inspection_audio.js` is an optional separate, read-only Frida trace; simply including it in the ZIP does not execute it.
- This is not a promise of zero clipping for every animation in every frame.
- PvP gameplay submission remains disabled unless otherwise configured and validated.

## Testing
Source and archive checks only; no Android build/runtime test performed. Compare 3 fighters with very different renderer counts; 2 environments and 1 trick; confirm initial display, held animation, cleanup, and no rendering regression.
