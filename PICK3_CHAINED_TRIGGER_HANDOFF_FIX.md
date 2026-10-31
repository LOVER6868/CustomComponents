# Chained PickFromThree trigger handoff

Problem: after a successful PickFromThree choice, a second BlockingEffect from another card on the same trigger may wait for a screen touch before becoming presented.

Prior `EffectAbsentFromQueue` required both the previous effect's complete absence from all three queue lists AND no other BlockingEffect at the active front. That second condition prevented the normal, once-only `GameStateUpdatedSignal` presentation refresh at exactly the transition to a successor blocking effect.

Change: preserve validation of the queue structure, previous effect absence from all three lists, two stable ticks, stock-success cancellation, and the one-time game-state signal. Treat a different BlockingEffect at the queue front as an eligible successor instead of blocking the refresh. Logs `HANDOFF: SUCCESSOR_BLOCKING_READY` when this case occurs.

No synthetic tap, no manual HighlightPlayableCardsCommand, no changes to popup effect-turn gating, art, materializer, sampler, or network transport.

Host C++20 syntax check passed. Live Android PvP behavior is unverified. Watch for `HANDOFF: SUCCESSOR_BLOCKING_READY` followed by `DEFAULT_REPAIR: GAME_STATE_REFRESH_SENT` and the successor's `TIMING: EFFECT_TURN_READY` without an intervening `INPUT_HANDOFF` event.
