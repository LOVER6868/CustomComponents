# Known issues and follow-up investigations

## 1. Physical-phone instability and native hook safety — OPEN / DEFERRED

As requested, preserve this investigation for the next engineering pass.

- The modified PvZ Heroes game reportedly crashes randomly on a physical phone,
  and can crash when PickFromThree is opened. No current-phone tombstone has
  yet established the specific faulting hook.
- Audit all installed Dobby/BNM native hooks for overlapping detours, short
  ARM64 target functions, duplicate installations, and ignored return codes.
- In the provided ARM64 game binary, `HeroBlockingSystem.ProcessBlockableDamage`
  and `HeroBlockMeterModifier.GetChargeAmount` do not overlap (entry points
  0x21F9000 and 0x220F1A4, respectively). A report about nearby Charge hooks
  remains unconfirmed for other binaries / actual installed hooks.
- `TargetIconViewComponent.Update` is a frequent-use presentation hook.
  Review its Unity-object access and disable it independently during triage.
- Audit managed exception handling, Unity object lifetimes, async callback
  cancellation, and post-match cleanup; crashes need not be detour collisions.
- Reproduce with and without `NativeAvatarLoadTest`, then with
  `MaskStockTargetUI=false`, then without CustomComponents if needed.
- Collect a fresh physical-phone tombstone and compare the `libil2cpp.so`
  Build ID before interpreting instruction addresses.
- Hook safety audit notes: `Pick3-Phone-Crash-Hook-Audit-Kit.zip` (2026-10-18).

**Do not mark resolved simply because the desktop emulator works.**

## 2. Native avatar visible-portrait experiment — IN PROGRESS

- The first-card async loader was verified in Android logs on 2026-10-18:
  REQUEST_RETURN, ENTER success=1, IDLE success=1, UNLOAD ok=1 and RELEASE.
- This experiment expands to three independently-created native CardViews, each
  captured through a private `Camera`/`RenderTexture` into a UGUI `RawImage`.
  Unity camera set_targetTexture, cullingMask, clearFlags, backgroundColor and
  Render are available as engine internal calls in the supplied Unity library
  but are **not present in this stripped managed dump**. Resolve them at runtime;
  never attempt rendering unless all required entries are available.
- Runtime appearance, camera framing, material alpha, sprite renderer bounds,
  actual native camera execution, and three-option cancellation **remain untested**.
  A static card face is kept if any native integration stage fails.
- Each render target uses the request generation as ownership, so a late
  previous-session callback cannot destroy a newer popup's texture.
- Camera rendering layer 31 is reserved for these offscreen avatars. Verify no
  mod/game object uses it in the target game build.
- Phone compatibility is not verified; physical-phone instability and native
  hooking audit remain issue #1 and must not be forgotten.

## 3. In-game visual asset fidelity — BACKLOG

- Sun/Brain cost icons, class frame/glass, and reward-style button artwork
  still need asset provenance verification against game-managed bundles.
- Native card-view Enter/Idle animation must eventually animate only the
  portrait, leaving cost, stats, text, and buttons stationary.

## 4. Native environment/card-view idle verification — OPEN

- Non-fighter Enter → Idle dispatch is now replaced with type-specific startup.
- `CardData.IsEnvironment` requests `AnimatorParameters.InHandTrigger` to
  select the card-view variant, without the placed-lane state. Confirm the
  animation on several environments and on different phone/emulator builds.
- If a prefab lacks a working in-hand state, the trigger can return without
  an exception; use video/screenshots to confirm actual motion.
- Trick and superpower animations should begin from Idle with no Enter.

## 2026-10-18 — Native trick/environment presentation follow-up

- Trick one-shot `CardAvatarViewComponent.PlayIdle()` states can appear briefly
  then disappear. This patch requests the game-owned card-view `InHandSprite`
  instead, with a normal static UGUI fallback if unavailable. Verify card
  examples including tricks, superpowers, and mixed-class results on Android.
- Native environment mesh sizes differ and can change *after* their InHand
  trigger. Current test reframes the isolated render camera at approximately
  0.12, 0.34, and 0.68 seconds using actual renderer bounds. Compare tiny and
  oversized environments and check for clipping or camera-frame jumps.
- Native render camera and 3 asynchronous CardView loads are experimental. The
  earlier physical-phone random crashes/hook-overlap investigation stays OPEN.
- Animated selection dismissal is presentation-only and must not alter PvP
  timer dispatch, multi-target `NumTargets`, or preflight retry behavior.

## 2026-10-18 — Inspection-state and native CardView scaling verification — OPEN

- `CardViewComponent.ShowForInspection()` now determines presentation for native
  fighters, environments and tricks. `CardAvatarViewComponent.HandlePlayedToBoard()`
  is triggered only once for fighters; the forced .85-second `PlayIdle()` call
  was removed. The stock AnimatorController must own state transitions.
- The game-owned `GetDesiredScale()` and `GetDesiredAvatarScale()` values are
  logged per offer as `Pick3/CARD_SCALE`. The per-card prefab transform is not
  overwritten; these are NOT guaranteed camera-fit measurements.
- The portrait renderer previously fit the entire animated renderer hierarchy,
  making cards such as Dark Matter Dragonfruit tiny when a large effect bounds
  was included. Current test uses the dominant 2D renderer and freezes camera
  framing. This is an experimental mitigation, not guaranteed stock framing.
  Inspect the card-view log scales and compare multiple fighters of different
  asset sizes before finalizing the camera algorithm.
- Trick and environment inspection animation support must be verified on an
  Android device. Missing native avatar/hand art retains the existing fallback.
- Physical phone crashes and native Dobby hook review remain unresolved.
