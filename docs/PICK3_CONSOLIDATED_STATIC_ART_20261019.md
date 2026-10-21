# Pick3: consolidated static-art integration — 19 October 2026

Starting point: `CustomComponents-Pick3-StableArtCache-Full.zip` (the runtime-confirmed non-destructive Sprite-cache fix). The stable PNG cache, portrait lookup, submission, localization, and native-avatar isolation are **not replaced**.

## Newly implemented

1. **Trait-specific fighter badges.** `Pick3CardMetadata::Read` reads the original `CardData.SpecialAbilities` managed list and calls the game's own `PvZCards.Game.AttackIconDecider.GetAttackSpriteType(List<SpecialAbilityType>)` and `HealthIconDecider.GetHealthSpriteType(...)`. Both methods are verified in `dump.cs`. Valid enum indices 0–14 select a GC-rooted sprite already captured by the game-owned `CardFrameIconSprites` registry. Each candidate gets its own attack/health selection. If either method is absent/fails, or the requested sprite is not cached, the previous basic badge remains. This is printed/card-data abilities, not live temporary buffs after a card enters play.
2. **Inspection cost icons.** On stock `CardInspectionSunCostView.SetFaction` and `ShowAndSetSunCost`, *after forwarding the original call*, the registry reads the `SunIcon` / `BrainIcon` GameObject references and checks their own `Image`/`SpriteRenderer` (or immediate descendants) for initialized Sprites. A valid Sprite is GC-rooted by the registry. No card view/prefab is created, no `FindObjectsOfTypeAll` scan, and no AssetBundle is loaded by these hooks. Cost icons may still fall back if stock inspection never activates or its objects do not expose a direct Sprite.
3. **Class rims.** We embedded two transparent left/right halves derived **offline from the verified 498×497 inspection rim PNG**, and use the existing non-destructive `Pick3RewardArt` lifetime handling. For each candidate, class flags select the applicable class colors captured from `ScriptableObjectColorScheme`, falling back to the already verified color literals. The halves are tinted independently for dual-class cards. Both halves are also prepared in the existing stable-match prewarmer, on separate ticks, reducing first-popup work. The unchanged plain rim remains underneath. No `Image.Filled` ABI calls, UI class prefab instantiation, scene-wide sprite search, duplicate bundle loading, or stock animator calls.

## Deliberately unchanged

- Grass, portraits, description panels, inline tooltips, choice/submission, and stable cache lifetimes.
- Existing Choose-button renderer. The available traces identify stock *button controls* and animations, but do not establish a validated, reusable choice-button Sprite or a safe ownership path. `social/button_tint` is not the game's reward choice sprite; avoid presenting it as such.
- Animated native avatars and stock reward transitions (still isolated).

## Config

`[Pick3GameOwnedAssets] Enabled=true, UseInPicker=true, UseTraitIcons=true, UseClassRims=true, LogCaptures=true`.
Keep `[Pick3OwnedUnityFS] Enabled=false, ProbeMode=0` and `NativeAvatarCrashIsolation=true`. Keep the temporary `Pick3AssetTracer` disabled because of shared hooks.

If **only** the class tint shows unexpected geometry, set `UseClassRims=false` and rebuild. If **only** trait badges misbehave, set `UseTraitIcons=false` and rebuild. `UseInPicker=false` disables use of captured stock Sprites while leaving the capture implementation present. The stable static fallback remains intact.

## Device test sequence

1. Enter PvE, open PickFromThree with a plain fighter; verify basic attack/health and unchanged portrait/description artwork.
2. Open PickFromThree with candidates that have Armored, Deadly, Frenzy, Strikethrough, Double Strike, Bullseye, etc. Look for `STOCK_TRAITS: READY available=1` and `STOCK_TRAITS: option=... attackType=... healthType=...`. These are **card-data** abilities and should map through the stock deciders.
3. Open cards in *stock* inspection to trigger cost-icon capture, then open PickFromThree again. Look for `STOCK_REGISTRY: COSTS source=inspection-view ...` followed by `COST_USE` rather than `COST_FALLBACK`. A fallback is not a failure of the popup; it means the source Sprite was not available.
4. Observe class-tinted rims, including a dual-class candidate. `CLASS_SHELL ... source=owned-split-rim` means the independent halves were selected.
5. Finish the match, enter another **without restarting**, and open PickFromThree again. Look for `ART_CACHE: REUSE` with no destructive `INVALIDATE → CREATE` loop.

The C++ host syntax check passes. **This is not an Android runtime/visual validation**. Any optional stock hooks that couldn't be installed should fail closed and preserve fallbacks.
