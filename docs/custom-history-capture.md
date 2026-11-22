# Earlier diagnostic capture notes — November 19

The schema-2 implementation supersedes the dispatch limitations described below.
Read [Custom history records](history/RECORDS_V2.md) for the current architecture.
The remaining notes document the earlier capture format and reproduction guidance.

CustomComponents owns gameplay semantics and presentation contracts. MatchRecorder
remains optional. This pass instruments the existing v6 record path; it does not
implement the planned in-band merger or deterministic replay.

## Capture settings

Enable `[CustomHistory] Trace=true` and `[Performance] Enabled=true` in the module
configuration for a diagnostic build/run. Keep both disabled for release timing
comparisons. The history setting installs one checked, read-only observer hook on
`HistoryProcessor.GetNextRecord`; existing queue-hook owners emit animation events.
No stock cursor, list, prediction state or dispatch result is modified by tracing.

Capture from before match creation through the failed action and scene exit.
Reproduce a threshold counter with multiple listeners, then discard while earlier
stock animations are busy, and immediately return a discarded card. Include both
PvP perspectives when investigating replication. Do not reveal concealed card
definitions in frontend traces.

## Evidence to correlate

- `CustomHistory/COMMIT`: custom serial, model, stock-count boundary, effect and card.
- `BATCH_BEGIN`, `BEFORE_STOCK`, `BATCH_END`: actual processor/history identity,
  next stock cursor and concrete stock record type, including skipped presentations.
- `DISPATCH`, `ACCEPTED`, `RETRY`, `MISSING_PROCESSOR`, `CAPACITY_REJECT`: delivery
  decisions. Acceptance is not visual completion.
- `ENQUEUE`, `ENQUEUE_FRONT`, `START`: queue/animation identity and monotonic time.
- `DISCARD_LIFT_ENQUEUE`, `DISCARD_SHRINK_ENQUEUE`, `LIFT_COMPLETE`,
  `SHRINK_COMPLETE`: card-specific tween identity. Join these with the record card
  and the same scene/model; pointers are diagnostic identities, not replay IDs.
- `COMPLETE`: record acknowledgment after existing visual cleanup.
- `TARGET_QUERY`: effect definition entity, outer query type, result count and AI
  scope; `Performance area=variable-query` measures nested variable-query cost.
- Variable comparisons now report defined/value pairs for evaluated numeric sides.

Tracing is limited per three-second window (512 lifecycle lines, a separate 48
target-query lines). `TRACE_SUPPRESSED` reports omissions. A trace with omissions
cannot establish complete ordering; narrow the reproduction. Native tween completion
is observed, not inferred from elapsed time. General animation cancellation and
scene-terminal outcomes still need the planned CustomHistory lifecycle migration.

## Fixes in this pass

Variable-backed effect expressions now consult the same active event snapshot as
conditions. Assignment storage and write acknowledgment still use live values.
Queued listeners share an immutable baseline and own independent write overlays;
prediction copies cannot change another listener's values. Timed-variable decoding
is cached by managed payload identity. Query scopes borrow existing contexts and
create a finder only when required; inactive hand/board-only zone tests avoid an
unnecessary finder. Missing hand-discard processors retain pending work instead
of silently acknowledging it.

## Limits and next milestone

History dispatch is still post-batch and discard still uses its existing idle gate.
Do not present these traces as a finished discard fix. Prove `(stock A, custom C,
stock B)` boundaries on-device before implementing in-band dispatch, per-model
epochs/history generations and record-owned queue sequences. Do not append new
stock RecordType values or make gameplay depend on recording being enabled.

Recent captures show successful Halfbread buff/draw listener sequences as well as
substantial native target/AI/animation cost. Host tests and syntax checks cannot
establish that every authored counter or device hitch is fixed. Compare captures
from the same deployed library, with diagnostics enabled and disabled.
