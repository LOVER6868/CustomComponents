# PickFromThree

PickFromThree (PF3) presents a card-choice menu for a primary target filter. Stock
selection services still validate and execute the choice. Physical selections
retain the exact entity, including copies sharing a GUID.

## Selection modes

| SelectionType | Candidates | Use case |
| --- | --- | --- |
| `PickFromThreeRandomTargets` | Physical entities | Weighted deck/discard candidates |
| `PickFromThreeUniqueGuidTargets` | Physical entities grouped by GUID | Distinct definitions in the offer |
| `PickFromThreeCardDatabase` | Database definitions | Creation or a subset payout |

`PickFromThree` is also accepted for random-target selection. Use a physical zone
query to select an existing card; GUID membership does not identify a buffed copy.
Unique-GUID offers retain a random physical representative. A sole eligible GUID
uses the normal automatic handoff, including query binding and trigger delivery.

## Additional targets

`AdditionalTargetSelectionType` is optional on `PrimaryTargetFilter`. Values are
`Inherit`, `All`, `Manual`, `Random`, `PickFromThreeRandomTargets` and
`PickFromThreeUniqueGuidTargets`. Omission retains stock additional-target behavior.
Interactive additional selection requires one primary target.

For a database card followed by a lane choice, use this filter configuration plus
the primary `Query` and `AdditionalTargetQuery` from a complete
[creation example](../examples/pick-from-three/README.md):

```json
{
  "SelectionType": "PickFromThreeCardDatabase",
  "AdditionalTargetSelectionType": "Manual",
  "AdditionalTargetFirst": false,
  "NumTargets": 1,
  "AdditionalTargetType": "Query",
  "OnlyApplyEffectsOnAdditionalTargets": true
}
```

Database PF3 requires this one-target, query-recipient shape. The selected GUID
travels separately from the real recipient entity.

`AdditionalTargetFirst=true` reverses interactive selection order. The first target
restricts primary candidates before sampling. Board targets appear during their
selection stage; card popups hide those highlights.

When card-first PF3 uses `OpenLaneForCardQuery` with a picked-card `CardQuery`, lane
finding runs after the card choice. Ordinary creation retains stock lane selection.

## Picked-card queries and listeners

- `PickedFromThreeQuery` matches the selected GUID for a subset payout.
- `PickedFromThreeEntityQuery` matches the physical card. A database choice gains
  that identity after its created/drawn entity is verified.
- `PickFromThreeTrigger` belongs on a separate effect entity. Target the picked
  physical card there to apply a buff or cost change.

A definition is not a physical card. Listeners needing a produced card wait for
verified entity binding. Provisional lane queries do not override committed
identity during actual gameplay effects.

## PvE and PvP

Both use the stock command/service handoff. The frontend observes its rooted input
holder each frame so consecutive menus can advance without a board tap. It
refreshes presentation for an actual local PF3 blocker and submits no fabricated
play. Model/effect/group/generation checks separate speculative copies from the
accepted physical-card binding. Use matching native code and card data on both
PvP clients.
