# PickFromThree — normalized sizing and PvP preparation

## Normalization

- CardView native transform scales are not changed by this patch.
- `Pick3NativeArtwork.ViewSizePercent=100` and `EnvironmentViewSizePercent=100`: portrait RawImage uses exactly the card width and stays square. Artwork remains unmasked.
- `Pick3CardViewCamera.ZoomPercent=100`, `EnvironmentZoomPercent=100`, offsets 0: eliminate extra user-config camera scale and translation. Camera bounds fitting and stock-prefab authoring still affect apparent character size, so normalization is **not** identical to the original stock inspection camera.
- Preserved existing framing padding (122%), clamps, and dominant-renderer selection: these are layout algorithms rather than neutral multipliers. They remain candidates for later native attachment-based replacement.
- Green Choose button asset now has higher face alpha/brightness; existing Image tint remains white.
- Existing per-card animation settings are unchanged; this patch does not assert a device FPS improvement.

## PvP preparation / safety boundary

- `SubmitInPVP=false` remains the explicit default; the C++ config fallback is now also false.
- Existing PvE selection flow, candidate sampling, UI, and submission behavior are unchanged.
- No hooks, request serialization, server-authoritative gameplay, or player state were changed.
- Before enabling PvP: capture both-client diagnostics for candidate list identity/order, selected GUID, request ownership, resolver type (remote vs local), timer, timeout fallback, and subsequent server/client state agreement. Compare identical GUIDs and order; verify one submission, no duplicated mutation, and clean cancel/disconnect.
- Do not enable PvP simply because the local picker appears to work: local-only completion and desynchronization can corrupt a match. Treat PvP as diagnostic-only until the authoritative path is validated.

## Device checks

1. Launch PvE, confirm all three previews appear, screenshots show no square-to-rectangle stretching and the Choose button is brighter.
2. Check fighter, environment, and trick visual centers. Different authored visuals may still be uneven due to bounds fitting.
3. Verify positive selection once, cancel behavior, and re-opening the picker.
4. Keep PvP submission disabled while recording PvP diagnostic logs.
