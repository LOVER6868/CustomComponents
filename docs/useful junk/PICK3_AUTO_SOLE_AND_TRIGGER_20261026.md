# Unique-GUID auto-selection and PickFromThreeTrigger test patch

This patch is layered over `CustomComponents_PickFromThreeTrigger_Rename_FULL_20261026.zip`.

## Automatic selection

`UniqueGuidTargets` already samples physical entities while excluding repeated card GUIDs in the **offer**. If the final offer contains exactly one distinct GUID and `requiredTargets == 1`, the presentation layer now bypasses `Open()` entirely. It calls the existing `Submission::Submit(request, {0}, false)` once, from the same live stock-holder activated request. Index zero represents one exact entity ID with one GUID. All other copies remain in the deck. It polls the same completion result and calls the same transaction cleanup as a manual menu choice. Failure/scene-transition paths fail closed.

No changes to weighted sampling, nonunique targeting, DB5, PvP deck materialization or shuffle.

## Generic trigger

Before entering the stock selection service, submission roots the matching `Effect` object from the service's concrete effect queue. On confirmed local service completion, the code dispatches `PickFromThreeTrigger` in the exact rooted match model, once. It releases the root even on failure. The normal card's PlayTrigger is not manually fired.

**PvP limitation:** `RemoteSelectTargetService` success may reflect client prediction, not server acknowledgement. To avoid unilateral trigger state and desynchronization, this patch logs `PICK3_TRIGGER deferred` rather than broadcasting it in PvP. A future replicated/stock completion hook is still required for multiplayer support. Local PvE is experimental, not device-tested.

## Inspect logs

`CustomComponents/Pick3/AUTO_SOLE: DECISION` should show `distinctOptions=1 submitCopies=1 popup=0`, followed by `SUBMITTED ... targets=1` and `COMPLETE ... accepted=1`.
`CustomComponents/Pick3/TRIGGER_CONTEXT` shows whether the exact stock blocking Effect was preserved.
`CustomComponents/Pick3/PICK3_TRIGGER` shows queue dispatch status (not proof that downstream effects executed).

Test first in PvE; keep a working APK backup. Do not interpret remote predicted success as authoritative.
