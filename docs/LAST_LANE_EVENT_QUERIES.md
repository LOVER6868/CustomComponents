# Last-lane event-context queries

Two no-field query types are registered under `PvZCards.Engine.Queries`:

- `LastLaneOfTargetQuery`: matches a **Lane entity** whose entity ID equals the `LastLane.LaneId` of `EntityQueryContext.GetTargetEntity()`.
- `LastLaneOfSourceQuery`: same for `EntityQueryContext.GetSourceEntity()`.

They use **trigger context**, not `Self`; both return false if the context entity, LastLane component, or candidate Lane is absent. They do not select the moving fighter itself. The copyable minimal JSON is in `examples/queries/last-lane-of-target.json` and `last-lane-of-source.json`.

`examples/queries/shell-on-wheels-last-lane-example.json` demonstrates a second effect group on GUID 727: `MoveTrigger` + `TriggerSourceFilter(SelfQuery)` + `PrimaryTargetFilter(LastLaneOfTargetQuery)` + `CreateCardEffectDescriptor(CardGuid: 77)`.

**Important runtime caveat:** This example assumes the movement event sets its **Source** to Shell on Wheels and its **Target** to the moved Plant, and that `LastLane` is populated before MoveTrigger evaluation. The C# signatures alone do not prove these event semantics. Test in-game before relying on the effect. The example implements only the base Wall-Nut leave-behind, not the Upgrade +2 Pluck replacement. Card GUID 77 is taken from existing stock CreateCard examples; confirm the resulting card in game.
