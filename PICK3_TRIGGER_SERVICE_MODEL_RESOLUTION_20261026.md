# PickFromThreeTrigger local producer scope repair

Observed failure on 2026-10-26: manual CardDatabase selections succeed, but `PickFromThreeTrigger` reports `exact-model producer missing` and `PICK3_TRIGGER queued=0`.

`PickFromThreeSubmission.hpp` now resolves the `EntityModel` injected into the **same local SelectTargetService finder** used for the accepted target transaction. It checks that model for an exact registered stock TriggerSystem producer; only if absent does it test the UI/command model for an exact producer. Neither fallback uses recycled entity IDs or picks arbitrary TriggerSystems.

The log `CustomComponents/Pick3/PICK3_SCOPE` prints `uiModel`, `serviceModel`, `producerModel`, and `serviceMatch`; `PICK3_TRIGGER` still records the result. If no exact producer exists the event is NOT fired. PvP prediction remains deferred. One-choice `UniqueGuidTargets` continues to select precisely one physical entity via `Submit({0})`, not every duplicate GUID. Deck materialization and discard-to-lane behavior are unchanged.

Test in PvE with GUID 716: check `PICK3_SCOPE producerModel != nullptr`, then `PICK3_TRIGGER queued=1`, and separately confirm the chosen card's cost change. A queued trigger does not itself prove downstream effects executed. Test UniqueGuidTargets using many physical copies of a single GUID; confirm no popup and only one chosen entity.

Host coverage: `GlobalPlayTriggerScopeRegression.py` and `Pick3SingleGuidAutoRegression.cpp`. No Android runtime verification has been performed on this version.
