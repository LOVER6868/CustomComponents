# PickFromThreeUI update – October 18

Baseline: `CustomComponents-Pick3-NoGlobalSpriteScan.zip`.

- Uses resident `CardInspectionColorSpriteView` per-class sprites instead of handcrafted grass/ring; nothing is displayed for that background until the inspector asset is resident. Those sprites may depend on the inspection prefab being loaded; do not instantiate another inspection UI just for art.
- Looks up the sun/brain sprite from `CardInspectionSunCostView` (including its child graphics) and from existing collection-card refs. Bundled cost art is a fallback only when the stock UI has not loaded.
- All new TMP labels receive the same borrowed Cafeteria SDF font material as a stock label, when available; legacy font remains a fallback.
- `[Pick3Description]` dynamically measures TMP text, constrains width/height and shifts the Choose button/hitbox if the plate grows.
- `[Pick3TargetingText]` keeps your current working localization and makes its X/Y, size and draw order editable.
- Wider/taller native render targets, corrected portrait-aspect camera fitting, larger UI art bounds and three painted frames before showing the preview to avoid flashes. Game animation calls are unchanged.
- No global Sprite enumeration is reintroduced.

This build was source-tested, not run inside Android/Unity. Native inspection sprites may only become resident after inspection UI has loaded. If absent, the class background stays transparent instead of a fake.

- Button upper-face alpha repaired directly in the authored sprite data; rounded outer corners remain transparent.
- `[Pick3NativeClassArt]` crops only tall stock inspection textures to their upper circle, with adjustable crop center and width.
