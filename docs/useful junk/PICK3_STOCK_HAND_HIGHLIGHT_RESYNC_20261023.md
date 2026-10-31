# PickFromThree stock playable-card highlight resync — October 23, 2026

Baseline: Pick3_full_source_default_on_repairs_20261023.zip.

## Intended behavior
- Preserve the working default-on target submission, button handoff, server-visible target reconstruction, and existing game-state update repair.
- After a successful PickFromThree service completion and verification that its effect has retired, wait for the game input holder to return to its normal `SelectCardToPlayInactive` (3) or `SelectCardToPlayActive` (4) state, and the target subturn to end.
- Then resolve the game's *injected* `HighlightPlayableCardsCommand` from the active command binder. Invoke its stock Execute() exactly once if every injected dependency exists and the command's injected input state matches the holder's current state. This reuses game playability rules and `EntityBecamePlayable/Unplayable` signals, not fabricated visual highlights.
- Do not modify deck entries or PickFromThreeUnique sampling: each option targets one physical entity. MoveCardInDeck's existing RemoveAt/Insert reorders that one CardPoolEntry; other copies remain.

## Safety/fallback
- Gameplay repair is always enabled when normal PickFromThree submission/completion is enabled. No ADB gameplay switch is introduced.
- Does not synthesize another Play, force visual subturn to 0, manipulate cards, or run during another targeting effect.
- If the stock highlight command cannot be safely resolved/injected, the refresh is skipped after 180 observation ticks instead of invoking an unsafe command. Android/PvP behavior is not yet verified.
- Optional trace in `/sdcard/Android/data/com.ea.gp.pvzheroes/files/pick3_net_trace.txt`: 0 off, 2 custom + stock events. Look for `HAND_HIGHLIGHTS: STOCK_REFRESH`, `WAIT_STATE`, `EXPIRED`.

## Validation
- C++20 host syntax check against bundled BNM dependencies passed.
- ZIP CRC tested, changed source checked against baseline; no gameplay testing performed.
