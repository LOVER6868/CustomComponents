# Unearth target propagation audit (2026-10-28)

Baseline: UnearthSecondaryQueueAudit. Only PickFromThreeSubmission.hpp changed, plus this note.

Read-only instrumentation:
- PrimaryTargetEffectCreator.CreateEffectsForPreselectedTargets: entry/exit.
- PrimaryTargetEffectCreator.CreateEffectsWithPrimaryTargets: primary List<int> IDs at entry and resulting collection pointer.
- SecondaryTargetEffectCreator.CreateEffectsForSecondaryTargets: primary List<int> IDs at entry.
- SecondaryTargetContextSetter.SetSecondaryTargetingContext: Effect.TargetId / SecondaryTargetId before and after stock call.

No mutations, creation overrides, or queue edits introduced. Output tag: UNEARCH_TARGET_FLOW.

Trace is gated to the most recently observed 712/713 trick focus and capped at 160 events; other events within that window may appear. The preselected PrimaryTargets object and returned PoolableList<Effect> remain opaque, so no speculative layout reads are performed. This audit is not a proven fix.

Android compilation and device behavior have not been verified.
