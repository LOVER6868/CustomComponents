# PickFromThree pre-Process effect identity snapshot

Scope: local, successful PickFromThree selection only. This change captures source, target, springboard, effect-entity and effect-group IDs from the EXACT group-matched blocking effect before `SelectionTransport::Dispatch`. The immutable integer snapshot survives stock effect pooling.

New logs: `PICK3_EFFECT_LIFETIME` at CAPTURE, DISPATCH_RETURN and TRIGGER_FIRE; `PICK3_SNAPSHOT_USED` at dispatcher. Compare identities and verify whether stock returns the effect to the pool. The existing `PICK3_CONTEXT`, `PICK3_VALIDATE`, `PICK3_LISTENERS` logs remain.

When firing the local custom trigger, the dispatcher still acquires the stock pooled `EntityQueryContext`, copies the underlying stock `EffectData`, executes the stock validator unchanged and queues accepted stock `TriggerData` unchanged. Only its copied four effect identity integers use the scoped pre-process snapshot; no guessed target, no validator bypass, no change to PvP emission or other triggers.

IMPORTANT LIMITATION: Effect.Data is NOT deep-snapshotted by this patch. Stock pooling may affect it, and cost reduction is NOT guaranteed. The earlier crash's native cause is still unknown. If validator returns false with restored IDs, investigate listener conditions and/or actual EffectData lifetime before applying gameplay changes.

Validation: run `python tests/Pick3EffectSnapshotRegression.py`, `python tests/Pick3ContextValidationRegression.py`, `python tests/Pick3TriggerDiagnosticRegression.py`, then build Android and test PvE with GUID 716. Host tests do not validate Android runtime.
