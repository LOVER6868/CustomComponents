# Known issues and risks

Maintained through the Step 8 review (2026-10-16). Do not confuse this with archived handoff/phase notes.

## Active investigations

| Area | Observation | What is and is not known |
| --- | --- | --- |
| Triggered animation stall | A match stopped advancing visually after a discard-to-deck return with queued listeners (card entity 46, group 1312 in the earlier log) | The exact non-completing animation was **not identified**. The game-over animation eventually proceeded. Do not force completion or delete queue entries to mask the symptom. |
| Generated-card buff double presentation | When Velociradish creates a copy in its lane and Starch-Lord buffs both cards +1/+1 (starting with the copy), the copy appears with its normal stats, gains +1/+1 silently, and then receives the ordinary animated +1/+1 again | **Visual-only** per testing: model/combat stats are correct, but the CardView displays an extra buff. Suspect timing or duplicate application between post-animation stat reconciliation and stock buff animation; not confirmed. Track view/model stats around create, reconciliation, and buff animation before changing behavior. |
| CardView lifecycle / crash | A single mid-match crash was reported on the physical phone after adding post-animation stat reconciliation; the next run did not crash | A lifecycle race is plausible but **unproven** without the phone crash trace. Maintain view identity checks and avoid new `Awake` hooks. |
| Manual deck/discard targeting UI | On some deck/discard targeting paths, instruction text or hand dimming may be inconsistent | Gameplay targeting reportedly works; presentation root cause remains unconfirmed. |
| Attack-comparison trigger timing | Prior tests observed inconsistent `AttackComparisonQuery` threshold timing near a buff/return chain | Could be event ordering; no confirmed engine repair. |
| `DiscardedFromQuery.Source` string aliases | `"Hand"` and `"Deck"` string values previously caused deserialization/initialization failures | Numeric `1` (Hand) and `2` (Deck) are the supported tested format. Do not globally register those words as `System.Int32` aliases. |

## Recently tested (watch for regression)

- Opponent hand discards now reconcile visible hand cards via existing shared hand-view hooks; avoid the previously problematic `OpponentHandViewComponent.Awake` hook.
- Both client viewpoints displayed corrected returned-lane Puff-Shroom stats in follow-up testing. This is not a guarantee against rare timing races.
- Earlier CardView duplicate-key problems motivated the existing deferred-creation/retirement coordinator. Rapid repeated returns still deserve stress tests.

## Diagnostics to retain

Until the issues above are resolved, preserve focused discard/return logs, lifecycle errors and deferral/retirement events, animation-stall context, and hook initialization failures. Broad lane-preflight and generic tracing are compile-time opt-in. The standalone eight-hook hand-removal observer is opt-in via `config.ini`. See root `config.ini` for diagnostic switches.

For hook owners and safety boundaries see [HOOK_OWNERSHIP](HOOK_OWNERSHIP.md).
