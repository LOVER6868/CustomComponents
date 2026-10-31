# Read-only OneTimeEffectValidator.IsTargetingValid trace

Hooks the private three-argument instance helper in PvZCards.Core.Utilities.OneTimeEffectValidator. Captures validTargets and selectedTargets through managed collection methods only (first 12 values), with a count and a readable status. When collection enumeration cannot be resolved, the trace prints readable=0; never treat placeholder ids as real entries. Observes result and before/after error collection counts. Error contents are intentionally opaque until their concrete layout is established.

No modified targets, validation decisions, command fields, or effect behavior. Previous initial-zone experiment remains disabled.

Search logcat for TRICK_TARGET_VALID; compare 699 and 712/713. The nested hook is scoped to GetValidPlaysForCard logging context, so some command-time validation may not be captured.
