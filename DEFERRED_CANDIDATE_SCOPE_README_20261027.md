# Deferred Pick3 candidate validation scope (experiment)

Changed: `inc/pick3/PickFromThreeSubmission.hpp`.

The opt-in `kDeferredPick3TrialEnabled` remains **false** by default. When enabled,
`TrickValidateObserved` scopes the currently validated `Play*` during
`GetValidPlaysForCard` candidate generation for GUID 712/713. The
`IsTargetingValid` hook now distinguishes `scope=candidate` from
`scope=service` and only accepts an exact previously generated zone ID if the
stock effect reports a nonempty valid-target collection.

The actual primary and secondary effect target filters are not changed by this
candidate adaptation. It does not redirect drag overlap targets; a real drop
still has to submit the matching zone target. A candidate's nonempty valid
collection is necessary but does not prove a valid discarded *fighter*;
subsequent tests should confirm the returned entities' types and zones.

Expect `DEFERRED_TRIAL: phase=ZONE_COMPARE_ACCEPT scope=candidate` during
candidate validation when the conditions match. Compare `TRICK_GENERATION:
phase=PLAYS count=...` and eventual service failures. Absence of the accept
means the stock validator path was taken. The setup log advertises
`candidateScope=1`.

Test in an isolated game. This is not a completed deferred-selection solution.
