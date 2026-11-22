# PrimaryTargetsQuery

A JSON-serializable query for the **actual card instances selected through a stock primary-target choice**. It compares `Entity.Id` values from `PvZCards.Engine.Components.PrimaryTargets.TargetEntityIds`, not `Card.Guid` values.

This query is **not** equivalent to `TargetQuery` or `EntityQueryContext.TargetId`. In ordinary effect/trigger contexts, `Target` refers to the event's trigger target. `PrimaryTargetsQuery` reads the recorded **primary target selection**.

## Copy/paste: selection on the current effect entity

```json
{
  "$type": "PvZCards.Engine.Queries.PrimaryTargetsQuery, EngineLib, Version=1.0.0.0, Culture=neutral, PublicKeyToken=null",
  "$data": {}
}
```

Place this in a `PrimaryTargetFilter`'s `Query` field **after** the stock selection is available on the same effect entity. This uses `FromAbilityGroupId = -1` internally; it does not scan other effect entities.

## Copy/paste: choice from an earlier effect group

```json
{
  "$type": "PvZCards.Engine.Queries.PrimaryTargetsQuery, EngineLib, Version=1.0.0.0, Culture=neutral, PublicKeyToken=null",
  "$data": {
    "FromAbilityGroupId": 0
  }
}
```

For a later effect group, `FromAbilityGroupId: 0` finds effect entities in `AbilityGroupId: 0` **belonging to the same live ability source** as the current effect. It reads their existing `PrimaryTargets` components, matches each selected instance by live entity ID, and rejects conflicting donor selections.

See `examples/queries/primary-targets-after-move-example.json`: **group 0** manually selects a Plant to move; **group 1**, on the subsequent `MoveTrigger`, targets that same selected Plant to give it +1/+1. This is an illustrative effect-entities fragment, not a replacement for Shell on Wheels.

## Behavior and limitations

- Selecting two identically named cards selects two *different entity IDs*. There are no hardcoded GUIDs.
- The `FromAbilityGroupId` field is an optional integer. Omit it to use only the current effect entity; set it to the selected group's `AbilityGroupId` to look across the same owner's effect entities.
- This query is *read-only*. It does not add another manual selection or collect a new target.
- It **requires** stock `PrimaryTargets` state to still exist at query time. It does not preserve an earlier activation's selection after the engine discards that component. If you need persistence across a completed ability or into a different turn, that requires an explicitly scoped state/snapshot system with a documented lifetime.
- It does **not** fall back to trigger `Target`, `Source`, or `Self` when primary selection is missing. It fails closed.
- If matching donor effect entities supply different target lists for the chosen group, it fails closed rather than accidentally combining unrelated selections.
- Target entities must still exist in the current model. A selected card that is removed from the model will not match a later candidate, regardless of its printed GUID.
- This build has passed C++ syntax checking and a pure host regression for target-ID matching/ambiguity. **In-game behavior has not yet been tested**, especially whether `PrimaryTargets` persists until a later `MoveTrigger` fires.

## Source files

- `inc/queries/PrimaryTargetsQuery.hpp`: BNM query, runtime metadata access, registration, copy/pool lifecycle
- `inc/queries/PrimaryTargetsSelectionPolicy.hpp`: target-instance matching and conflict handling
- `tests/PrimaryTargetsSelectionPolicyRegression.cpp`: host tests

Registered during CustomComponents startup after the lane queries; the same custom-field bridge preserves the `FromAbilityGroupId` value through deserialization and cloning.

## Definition selection

Use `PrimaryTargetsGuidQuery` with the same `FromAbilityGroupId` field to select card definitions by the GUIDs of the saved physical selections. It preserves the ordinary query's ambiguity checks; it does not union conflicting selections.
