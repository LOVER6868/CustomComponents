# PickFromThreeUI — link taps and preview race fix

Baseline: `CustomComponents-Pick3-UI-Complete.zip` (October 18 UI release).

## Code changes

- In native-avatar rendering mode, suppress competing `CardFaceSpriteHolder` image requests and resident/card-cache application. If native art is not ready, the option's artwork stays empty; the full card icon/frame is not temporarily displayed. Non-native mode retains the old sprite path.
- Place each unmasked `Pick3_NativeAvatarRaw_N` under `Pick3_AnimatedPortrait_N` in local coordinates. The camera view and portrait backgrounds now inherit the same container motion, instead of moving apart during the native preview's transition.
- Each cost badge starts transparent and is assigned a sun or brain only *after* `CardData.Faction` resolves. When a game-owned cost sprite is not available, the existing bundled cost sprite is chosen for the actual faction, not from `previewColors`.
- The Choose button displays its authored texture un-tinted. The preexisting alpha repair and all control positions remain in place.
- Taps on description `TMP_Text` links are detected by `TMP_TextUtilities.FindIntersectingLink`, with metadata retrieved from `TMP_TextInfo.linkInfo`. Keyword strings and parameters are resolved through the game's `KeywordAbilityInfoProvider.GetKeywordDescription` using the *candidate's* faction. If that injected service is unavailable, the existing runtime localization mapper is queried for the equivalent `ability_desc_plants_*` / `ability_desc_zombies_*` key.
- The tooltip is a lightweight overlay panel with the same borrowed game font/material as the picker. It displays above descriptions, can be dismissed with a tap, and is automatically destroyed with the picker. No new game-level pointer hook, native animation call or selection hook was added.

## Test checklist on Android

1. Open a PickFromThree that offers a fighter and a trick; watch loading frame-by-frame. The preview should remain blank until native art is ready; no temporary rectangular card-face sprite should appear.
2. Repeat with multiple match/popup openings and check three image positions stay aligned with their backgrounds through entrance/idle.
3. Check plant cards display sun, zombie cards display brains, including slow metadata / cost-asset loading.
4. Tap linked words (Freeze, Conjure, Double Strike) *after* the entrance has completed. Confirm the tooltip text is the correct faction's description. Tap elsewhere to dismiss, then verify Choose still submits normally.
5. Close PickFromThree and verify the tooltip never persists into the match scene.

This was C++ syntax-checked with the supplied BNM/Bloom headers. No Android/Unity runtime test was available; native tooltip DI and TMP link markup remain integration test points.
