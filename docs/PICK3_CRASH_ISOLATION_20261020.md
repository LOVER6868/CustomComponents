# PickFromThree crash isolation — first popup after match transition (2026-10-20)

## Evidence from reported Android log

- Latest release was Stage=1 with native factory disabled (`nativeLoad=0 nativeRender=0`). It never reached an `ANIMATION_EXPERIMENT: PREFLIGHT` log on the crashing popup. Therefore this crash is **not evidence that CardViewFactory/CreateCardAsync ran**.
- Eight sprite creations finished during the *previous* match (`PREWARM: COMPLETE fallbackSprites=8`). The match then ended and game-owned stock references were reset.
- In the next match, collection/inspection stock `COSTS source=inspection-view` succeeded. During the first picker `ART_CACHE: REUSE kind=5/6`, `COST_USE`, and `ICON_USE type=0/1` all occurred. The log ends immediately after `ICON_USE type=1`, before `STATIC_ASSETS: ready`, `CARD_UI`, or the animation preflight.
- The file contains no Android tombstone, `SIGSEGV` line, or native backtrace. The exact faulting function cannot be proven solely from the last Sprig log.

## Conservative isolation patch

- Reverts **only** the native Sprite construction schedule for prewarm to the last-good four kinds: `CostSun`, `HealthHeart`, `DescriptionPlate`, `InlineTooltipPlate`. Grass, reference rim, and split rim halves revert to popup-on-demand, as in the prior working build. The ten CPU-only PNG checks are retained.
- Restores the previous eight-item `PreloadOwnedStaticArt` sequence instead of expanding it to ten. Adds `PRELOAD_TRACE: ENTER/EXIT kind=X` surrounding each lookup, for native crash localization.
- Sets animation Stage 0 by default while investigating. Native animation remains gated and disabled. No changes to the pinned non-destructive PNG cache, hooks, trait deciders, class colors, or UI renderer.

## Testing

Build/reinstall. In one app process: play match A long enough for `PREWARM: COMPLETE fallbackSprites=4`, return to menu, start match B, open PickFromThree. Repeat opening several times. If it crashes again, collect `adb logcat -b crash -d -v threadtime` immediately, plus the regular Sprig log. The last `PRELOAD_TRACE` shows which asset operation was running. If the crash disappears, it supports (but does not prove) a cross-match prewarm lifetime problem. Test Stage 1 separately only after the picker is stable.

This patch is *diagnostic and conservative*, not a proven root-cause repair or an animation fix.
