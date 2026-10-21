# PickFromThree stability and resource-loading pass (2026-10-19)

## Baseline

`CustomComponents-Pick3-UIPolish-Multiclass-Full.zip` (the last UI-polish build).

## Why this is a stability build

The Android log `PvZ_Exception_Log(20261019-013438).txt` shows a native
SIGSEGV at address 0x38 on the Unity main thread after a successful keyword
tooltip and a later `INSPECTION_CIRCLE` refresh. Temporal correlation is not
proof of the exact crashing call. The previous per-90-frame RefreshDeferredGameArt
repeatedly enumerated Unity objects and accessed borrowed raw Sprite and
ColorScheme references while rendering. These are unsafe lifetime assumptions.

### Default changes

- Removed `RefreshDeferredGameArt`'s repeated runtime discovery from active
  picker ticks. No `FindObjectsOfTypeAll` passes are scheduled mid-tooltip.
- Stopped invoking the stock ColorScheme instance retained as an unrooted
  reference. Colors remain stable fallback tints until the original source is
  loaded in a verified way.
- The tooltip now consumes subsequent taps during its exit animation, with
  an additional short cooldown; keyword text is not repeatedly converted by
  enumerating resident mediator components on tap.
- The native CardView callback now initiates a bounded renderer-readiness
  phase. `ShowForInspection()` still fires only ONCE. Rechecks do not replay
  enter/idle animations. Fighter/environment failures remain blank; nonfighter
  static in-hand art is accepted only after waiting for animated renderers.
- Render status is logged as `inspection-avatar`, `game-inhand-static`, or
  `VISUAL_UNAVAILABLE`. An inspection avatar need not itself contain an
  animated rig, so that first category does not promise animation.
- Removed card-placeholder flashes during early callbacks; never display
  unrelated frame art while an avatar is still resolving.
- Embedded Choose-button face now uses an opaque authored green interior,
  leaving rounded silhouette edges transparent.
- Grass scales to 94% by default (was 104%, which crossed the rim).
- A bounded, process-lifetime GC-rooted cache holds only the fixed set of
  local UI sprites/textures (rather than decode/destroy on every picker).

### Native inspection prefab (experimental, disabled by default)

`[Pick3NativeClassArt] RequestNativePrefabExperimental=false`

There is a new **single, typed** async loader through the game's existing
`IdInterpreterLoader.LoadResourceAsync<GameObject>`, pointed at the exact
`Assets/Ship/Prefabs/UI/CardInspectionPanel` resource name from the APK
manifest. It does not instantiate a panel, scan all scene sprites, or start
its scripts. It preserves callback GC handles until a callback completes.
If the resource is unavailable, the fallback ring remains visible.

The prefab's availability and correct runtime path are **not proven** by the
manifest alone. The game can install without the downloaded asset bundle.
The native renderer currently accepts only a square class sprite; a tall
inspection-panel texture is logged and skipped rather than cropped blindly.
Two-class cards continue using the deterministic split fallback; classless
and 3+-class cards use white. This mode is for deliberate asset testing, not
an assertion of complete native-art compatibility.

Only enable this experiment **after** repeated tooltip and picker stability
is confirmed with the default settings, and use a disposable PvE match.
Inspect `INSPECTION_PREFAB` logs for request/callback, exact class count,
and whether an option applied a native square sprite.

### Acceptance checklist

1. Open/close at least 10 PickFromThree popups without crash.
2. Tap the same Freeze/Conjure/Double Strike keyword repeatedly, waiting
   briefly between taps; verify proper open/close and no game crash.
3. Confirm plants use sun, zombies use brains.
4. Test a fighter, environment, trick, and a delayed animated custom card.
   Check `VISUAL_READY` mode; no frame flash before rendering.
5. Test `meow_meow_meow`, `zombats`, a classless card and a card with 3+ classes.
6. Verify button faces have no transparency holes and grass remains inside rim.

The source passes a host C++ syntax check against supplied Bloom/BNM includes.
**There has been no Android runtime test** and crash resolution is unconfirmed.
