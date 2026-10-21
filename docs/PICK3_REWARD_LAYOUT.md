# PickFromThree — configurable reward layout / native inspection (test build)

Use `[Pick3RewardLayout]` and `[Pick3CardViewCamera]` in `config.ini`.
All numbers are integers; **change the value and rebuild Sprig** to test.
Gameplay selection, PvP/PvE submission, async resource cleanup and matching
are unchanged by these settings.

### Reward geometry (percentage values)

| Key | Default | Meaning |
| --- | ---: | --- |
| TopYPercent | 21 | Vertical center of first card (positive moves up) |
| LowerYPercent | -24 | Vertical center of bottom two cards |
| SideGapPercent | 24 | Horizontal spacing of bottom card centers, as screen-width % |
| PortraitYPercent | 28 | Portrait center inside option group, as card-height % |
| PortraitSizePercent | 104 | Disc and frame diameter vs card width |
| NameBelowStatsPercent | 80 | Name distance below stats, relative to badge diameter |
| PlateBelowNamePercent | 28 | Description box center below name, relative to card height |
| ButtonBelowPlatePercent | 31 | Button center below description, relative to card height |
| PlateWidthPercent | 136 | Description plate width vs card width |
| PlateHeightPercent | 39 | Description plate height vs card height |
| ButtonWidthPercent | 98 | Button width vs card width |
| ButtonHeightPercent | 120 | Button height vs computed button height |
| NameSizePercent | 120 | Name font multiplier |
| StatSizePercent | 116 | Base multiplier for badge fonts; each stat also has its own controls and auto-fit |
| ButtonTextSizePercent | 100 | Centered Choose label font multiplier |
| ButtonTextYOffsetPercent | 0 | Choose text vertical center offset, relative to button height |
| BadgeSizePercent | 24 | Stat and cost badge width vs card width |
| StatSpacingPercent | 13 | Left/right attack/health spacing vs card width |
| StatsBelowPortraitPercent | 40 | Stats below portrait center, relative to portrait size |

The card row origins and touch hitboxes update with these numbers. The
selected/unselected exit transforms still use saved card row origins.
The Choose button uses the existing authored rounded sprite directly;
there is no rectangular green underlay to show around its transparent corners.
The native Cafeteria font is used when loaded.

### Native portrait camera (card-view source)

| Key | Default | Meaning |
| --- | ---: | --- |
| ZoomPercent | 100 | Larger than 100 = enlarged art; less than 100 = smaller |
| PaddingPercent | 122 | Padding around selected renderer bounds (minimum 100) |
| MinOrthoHundredths | 20 | Minimum orthographic half-height (0.20 Unity units) |
| MaxOrthoHundredths | 1200 | Maximum orthographic half-height (12 units) |
| OffsetXPercent | 0 | Camera center X relative to measured extent |
| OffsetYPercent | 0 | Camera center Y relative to measured extent |
| UseDominantRenderer | true | Ignore extreme effect-particle bounds and fit dominant 2D art |
| EnvironmentZoomPercent | 100 | Additional environment camera zoom relative to normal (40–400) |
| EnvironmentCameraYOffsetPercent | 15 | Shift environment capture focus upward by % of measured art half-extent (-100 to +100) |

Native `CardViewComponent.ShowForInspection()` is invoked on completion of
`CreateCardAsync`. Fighters do **not** receive an extra `HandlePlayedToBoard()` call after
inspection, since the stock AnimatorController may already have triggered
its entry animation. There is no timed `PlayIdle()` override, and environment
cameras no longer reframe at 0.12/0.34/0.68 seconds. For tricks, a loaded native avatar is preferred;
if absent the stock `InHandSprite` is used instead.

`Pick3/CARD_SCALE` reports the game's `GetDesiredScale()` and
`GetDesiredAvatarScale()` for diagnosis. Those getters expose *desired
transforms*, not an automatic camera fitting API. The native card view owns
and maintains those prefab transforms; our camera fit still needs an Android
visual verification before it can be described as stock-exact.

### Independent native artwork layer

`[Pick3NativeArtwork]` controls the animated native `CardView` display.
It is a square UGUI `RawImage` **sibling of the round portrait**, not
a masked child of the ring. The UGUI rectangle does not limit its draw to the
disc; only the source camera texture bounds and screen viewport limit art.

| Key | Default | Meaning |
| --- | ---: | --- |
| `ViewSizePercent` | 135 | Native fighter/art square as percent of card width (50–600) |
| `EnvironmentViewSizePercent` | 135 | Separate size for long environments (50–600) |
| `EnvironmentYOffsetPercent` | 15 | Shift environment art upwards by this fraction of card width (-150 to +150) |

The in-game CardView is displayed using `ShowForInspection()` **once**; we do
not additionally invoke `HandlePlayedToBoard()` on fighters. The game
AnimatorController is solely responsible for native character animation.
Unloaded `RawImage`s are disabled (Unity would otherwise render its default
white texture); they are enabled only after a native RenderTexture has a
rendered frame. The static card-face sprite remains the fallback if native
loading fails. The round disc can still use its separate UI reveal motion.

These settings require a rebuild; they do not change the original prefab,
ability definitions, cards, or gameplay target selection.

### Native cost icons, targeting and badge text

- Native sun/brain sprites come from the game-owned `CollectionCardFrameView` cost assets or verified loaded cost sprites. The embedded cost placeholders are **not** displayed. Resource lookups retry while the popup is open (up to 48 slow refreshes); when absent, the icon area remains empty rather than showing a fake sprite. `REAL_COST_SPRITE_READY` logs a successful resolution.
- `TargetIconViewComponent` is no longer hooked or forcibly hidden. The older `MaskStockTargetUI` setting only adjusts the backdrop dim alpha and does **not** suppress target icons. The popup retains its input safety guard.
- For CardDatabase mode 5, a **single** eligible additional-target recipient is used only in the normal `Submit()` call **after** the player picks their card. No other Pick3 mode auto-picks additional targets. Multiple/zero DB5 recipients are not auto-picked (the existing one-recipient DB5 bridge still requires one for its popup).
- `[Pick3StatText]` provides independent size, X/Y offset, and width controls for attack, health, and cost; description has independent size, X/Y offset, width and height. All are percentages (integer config values). Offsets for numbers are relative to badge size; description offsets are relative to the cream plate. Positive Y moves up. Number fonts use shrink-to-fit, with the default base size capped at 45% of badge width to prevent overlap.
- Localization lookup implementation is intentionally **unchanged** in this pass.
