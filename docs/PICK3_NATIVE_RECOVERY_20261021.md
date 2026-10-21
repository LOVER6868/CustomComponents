# PickFromThree native artwork recovery — 2026-10-21

This recovery uses the native avatar loader and RenderTexture renderer from the earlier successful FactoryDI Mode 6/7 build. It retains the new UI, targeting localization, choice animation, reference layout, and darkening from the newer module.

## Deliberately conservative defaults

- 120 Unity frames between native avatar start requests in factory diagnostic modes 5–7.
- Render budgets: fighters 30, environments 24, other 24, global 90.
- Adaptive camera placement added after the successful Mode 7 test is temporarily disabled.
- `pick3_factory_di_stage.txt=5` is still needed when using diagnostic mode 6 or 7.

## Test

Stop the game, set `pick3_factory_probe.txt=6` and `pick3_factory_di_stage.txt=5`, and try one popup. If successful, set factory mode 7 and try all three. If the game crashes, return to mode 4, save the full crash buffer and Sprig log. The source-level merge is not a verified device fix.

Do not copy a historical full module over this one: this archive deliberately preserves the newer UI changes.

## Guarded fast-start follow-up

- Removed the `Transform.set_position` invocation that repeatedly faults at `position_begin` on this APK.
- Mode 7 waits for the previous native CardView to initialize before attempting the next; no overlapping unresolved `CreateCardAsync` calls.
- Config `LoadSpacingFrames=1` enables next-tick starts following successful completion.
- The older `CreateCardAsync` return path also crashed in a separate test, so this change is **not yet an on-device verified crash fix**.
