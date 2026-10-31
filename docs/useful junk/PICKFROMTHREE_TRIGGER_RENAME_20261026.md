# Generic PickFromThree trigger rename

Renamed the managed marker `PvZCards.Engine.Components.UnearthedCardTrigger` to `PvZCards.Engine.Components.PickFromThreeTrigger` so the trigger type is reusable across mechanics that use PickFromThree, not only Unearth.

Updated the registration list, C++ includes, marker class, serialized `$type` example, and currently unused helper name `FirePickFromThreeSelection`. No new producer is installed; this rename does **not** fix the one-option auto-selection dispatch gap. The current read-only UnearthTrace diagnostics are unchanged. Shuffle and existing runtime code paths are unchanged.

**Migration:** change any card JSON component `$type` references from `PvZCards.Engine.Components.UnearthedCardTrigger, EngineLib, ...` to `PvZCards.Engine.Components.PickFromThreeTrigger, EngineLib, ...` (same `$data: {}`). Remove the old component type from your definitions; the old name is intentionally no longer registered. Ensure both PvP devices use matching card data.

**Semantics to establish before wiring producer:** emit only after a *confirmed* PickFromThree selection resolves in simulation, on both one-option and multi-option routes, exactly once per selection. Avoid firing on UI close or prediction-only callbacks.
