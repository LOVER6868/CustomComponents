# Global PlayTrigger: exact-model dispatch hotfix (October 26, 2026)

## Evidence and boundaries

- User log `/mnt/data/PvZHeroes_logswewant(20261026-033428).txt`, current session at 23:33:24: `LANE_FLOW: GLOBAL_PLAY_ENTER card=114 effect=86`, then `GLOBAL_PLAY_EXIT dispatched=1`.
- The earlier `FireEvent` returned `true` after its loop even when `queued=0`; the log **did not** prove a listener effect was queued.
- Discard-to-lane already obtains the exact `EntityModel` from the stock `MoveCardToLanesSystem`, but the broadcast previously called `Fire("PlayTrigger", effect)`. Without the active StateCheck scope, that call selected a TriggerSystem by entity ID. Predictions can reuse the same IDs.
- Step 2B did not modify either of these trigger files; this may be an older latent problem, not a proven Step 2B regression.

## Changes

1. `TriggerProducerContext.inl`: add `FindStockTriggerContextForModel(expectedModel, preferredInstances)`. Require pointer-identical model and (when supplied) pointer-identical instance manager; do not fall back to reused ID matches.
2. `CustomTriggerDispatch.inl`: `FireEvent(..., expectedModel=nullptr)` supports exact-model selection; new `FireInModel` helper. PlayTrigger broadcasts log `PLAY_RESULT model=... expectedModel=... exactModel=... candidates=... rejected=... queued=...` even in quiet builds.
3. `DiscardToLane.inl`: the **existing** global `PlayTrigger` broadcast on successful discard-to-lane movement now uses `FireInModel("PlayTrigger", effect, model)`. The subsequent custom returned-from-discard trigger, lane placement, record creation, and stock forwarding are unchanged.
4. `GlobalPlayTriggerScopeRegression.py`: source-level guard for exact-model selection, logging, and preserved zone behavior.

## Retest

- Build this code on both phones and use the same private server.
- Return a card from discard into a lane under conditions where a global PlayTrigger is expected.
- Look for `CustomComponents/CustomTrigger: PLAY_RESULT` immediately before `LANE_FLOW: GLOBAL_PLAY_EXIT`.
- If `exactModel=1 candidates>0 queued>0`, the producer found and queued listeners in the intended model; observe actual effect execution.
- If `candidates=0`, no listener components were found in the live model; inspect card data / effect entities.
- If `candidates>0 rejected=candidates queued=0`, investigate listener trigger validator conditions.
- If `exact-model producer missing`, the stock TriggerSystem context for that model was not captured; investigate lifecycle and hook installation.
- `GLOBAL_PLAY_EXIT dispatched=1` on its own is not proof that a listener fired.
- Normal `PlayCard` event dispatch is untouched. If the issue concerns normal card-play triggers rather than discard-to-lane broadcasts, collect the card GUID and event; this hotfix is not evidence of a normal-play fix.

## Other issues preserved for separate work

- Step 2B logs show `PVP_INSERT_SKIP reason=pool-owner-unavailable` and `reason=pre-stock-snapshot-unavailable`; inserted PvP cards remain virtual in the attached capture. This fix does not claim Step 2B insertion succeeded.
- Set `MaterializeInsertedCardsPvp=false` on both clients as a safe A/B isolation test if problems persist. Keep the proven Step 2A materialization enabled.

## Verification

- `clang++ -std=c++20 -fsyntax-only` succeeded with Android/Bloom/BNM host includes.
- Six C++ regression test executables passed.
- Existing Python integration/config tests and the new trigger-scope check passed.
- **No Android runtime test has been performed for this patch.**
