# PickFromThree / generic revive PvP hot-path optimization (experimental)

Source: user-supplied CustomComponents(20261029-010849).zip.

Changes (PickFromThreeSubmission.hpp only):
- Added a 64-slot per-thread cache to the generic first-PlayTrigger revive trick predicate. A lookup checks the current rooted model, entity list, entity ID, source pointer and list size. It expires after 32 predicate queries, rather than repeatedly scanning every effect's components for the same card.
- Added a per-thread recursion guard for this predicate: re-entrant evaluation fails closed rather than recursively traversing live model data.
- Reused the already-computed generic predicate in BurstTargets hook rather than evaluating it twice.
- Avoided the generic predicate in EffectIsTargeted unless the original reported targeted AND the experimental override is enabled.
- Disabled high-frequency PVP_CRASH_TRACE breadcrumbs by default; turn kPvpCrashVerboseTraceEnabled back on if further crash diagnosis is required.

NOT changed: stock picker transport/submission, lane filters, auto-one-option branch, generic first-PlayTrigger conditions, initial zone selection semantics, or existing tracing elsewhere. Existing earlier stop-drag crash isolation is retained from supplied source.

Limitations: source identity/list size and short-lived expiration cannot guarantee cached results reflect mutations of effect components with unchanged entity identity. If such mutation occurs, use the previous baseline or set cache TTL to 0. Android compile and runtime not tested here. Native crashes are not proven fixed by reducing repeated reflection.
