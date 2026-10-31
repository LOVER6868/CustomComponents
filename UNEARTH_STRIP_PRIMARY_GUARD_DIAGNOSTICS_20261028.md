# Unearth strip-primary guard diagnostics — 2026-10-28

Baseline: CustomComponents_UnearthSourceFieldFixed_20261028_FULL.zip.

Added diagnostic logs to `UnearthStripCastingPrimaryTrial` in `inc/pick3/PickFromThreeSubmission.hpp`.

- SOURCE: SourceId, whether the entity and Card were located, resolved GUID, count.
- REJECT: argument/model/entity failures and out-of-scope source GUID.
- GUARD: matching effect entities and reasons for rejection (target components, target IDs, PlayTrigger or missing removal API).
- SUMMARY: descriptor, primary, eligible and PlayTrigger match counts and removal count.

No targeting checks were removed or relaxed, and no gameplay behavior was changed except the addition of diagnostics. The feature flag remains OFF (`kUnearthStripCastingPrimaryTrialEnabled=false`) in the packaged source. To inspect the enabled trial, set the flag to true before building (same as previous test). Logs are rate-limited to the first 500 setup calls plus any successful removal attempts.

Collect lines with `UNEARCH_STRIP_PRIMARY:` and `UNEARCH_SETUP_TARGETS:` after playing GUID 712 in PvE. A successful `sourceField=1` at startup only indicates field resolution, not source identity resolution.

Not compiled or runtime-tested in this environment.
