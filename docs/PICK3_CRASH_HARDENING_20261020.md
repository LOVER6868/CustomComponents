# PickFromThree crash-path hardening (2026-10-20)

Baseline: `CustomComponents_CombatHotPath_Optimized_Full.zip`.

## Changes

- Match-start and menu prewarming disabled **in C++** (not just config). The per-frame dispatcher now skips this entire phase after one inexpensive boolean check.
- Removed `UnityEngine.Object.set_hideFlags` calls from the fallback PNG `Texture2D`/`Sprite` construction path. This corresponds to the reported native IL2CPP call site; `BNM::TryInvoke` does not protect against SIGSEGV.
- Cached artwork reuse no longer calls Unity's `op_Implicit`/`Sprite.get_texture` as a diagnostic. GCHandle availability is checked without a managed method call.
- A failed PNG decode destroys its unshared Texture2D instead of discarding the managed wrapper with no cleanup.
- Native-avatar render performance log sampling reduced from every 180 paints to every 1800 paints.
- Public config switches default to no prewarming.

## What remains unchanged

- PickFromThree gameplay selection and nested NotQuery rules, match clearing, submission behavior.
- Animated native card artwork when the picker itself is open; camera geometry and stock-inspired framing unchanged.
- Charge and Thrash optimizations from the baseline.

## Risks / validation

The Unity native lifetime of generated Sprite/Texture objects is still not proven on all devices, even when their managed GC handles remain live. Runtime graphics calls can fail independently of C++ exception handling. The resource loader, async avatar ownership, and scene-transition cleanup deserve additional on-device validation. This is a targeted first step, not a guarantee against crashes.

Build with your normal Android toolchain, then reproduce PvE match startup and repeat picker open/close. Collect `adb logcat -b main,system,crash -v threadtime` and verify that `PREWARM: MATCH_BEGIN` never appears. If it does, the installed binary is not this version.
