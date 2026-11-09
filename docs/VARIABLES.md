# Card variables

Variables belong to a physical card. They're useful for a counter, a remembered card, or a value that changes with the board. Two copies of the same card don't share their variable tables.

## Define a variable

Put `Variables` on the card itself:

```json
{
  "$type": "PvZCards.Engine.Components.Variables, EngineLib, Version=1.0.0.0, Culture=neutral, PublicKeyToken=null",
  "$data": {
    "Values": [
      { "Name": "Counter", "Type": "Int", "Value": 0 },
      { "Name": "ChosenCard", "Type": "Guid", "Value": 721 },
      { "Name": "Remembered", "Type": "EntityArray", "Value": [] }
    ]
  }
}
```

Use `Int`, `Guid`, `Entity` or their array forms (`IntArray`, `GuidArray`, `EntityArray`). A GUID identifies a card *definition*; an entity ID identifies one actual copy in the match. Don't edit the serialized `Data` backing field by hand.

## Read or change a value

`SetVariableEffectDescriptor` goes on an effect entity, after its trigger and target selection. It writes to the specified owner. `Owner: "Self"` writes on the source; the setter's `Target` means the runtime trigger/effect target. `OwnerQuery` lets you choose one or more card owners. See the [setter examples](../examples/variables/set-int.json) and [variable snapshots](../examples/variables/snapshot-attack.json).

`Operation` supports `Set`, `Add`, `Subtract`, `Remove`, `Append`, `Clear`, `RemoveAt`, and `RemoveValue`. The usual `ValueSource` choices are `Literal`, `Variable`, `Query`, `DatabaseQuery`, `QueryCount`, `Self`, `Target`, `PrimaryGuid`, `Attack`, `Health`, `Cost`, `Level`, `BaseAttack`, `BaseHealth`, `BaseCost`, `CurrentHealth`, `DamageTaken`, `ArrayElement`, and `ArrayLength`.

Numeric expressions can use normal arithmetic and `min`, `max`, `clamp`, `abs`, `len`, `sum`, and `at`. Division truncates toward zero. Arrays and entity references can't be used as plain numbers. Compare a value with `VariableComparisonQuery`, or check presence with `HasVariableQuery`.

## Multiply an effect

`VariableMultiplier` works on stock effects that consult the game's multiplier calculator. Put it **after** the amount descriptor in the same effect entity. It can evaluate a stored variable or a built-in stat such as `DamageTaken`.

For a heal based on each recipient's missing health, set `HealAmount` to 1 and use `VariableMultiplier` with `Expression: "DamageTaken"` and `Owner: "PrimaryTarget"`. That mode calculates separately for each selected primary target. Do **not** use `Owner: "Target"` as a synonym; `Target` refers to the trigger context elsewhere in the game.

See [the full heal example](../examples/new-card-features/FullHeal_PerPrimaryTarget.json). Other multipliers can use `Query` with `Single`, `Sum`, `Min`, or `Max`. `Divider` follows stock integer rules (zero is treated as one).

`LevelSpentMultiplier` reads `Leveled.Value` directly, including zero for an unlevelled play. It measures **extra** cost, not total cost. You can also save that number with `ValueSource: "Level"`.

## Remembering cards and continuous values

Use `VariableCardQuery` when a stored GUID or entity variable should select cards later. A GUID may match several copies; an entity ID matches only the original copy. If you remember a PickFromThree database definition, capture its GUID with `PickedFromThreeQuery` and `DatabaseQuery`, not a physical entity that hasn't been created yet.

`Continuous` setters derive a current value from the model without repeatedly adding permanent increments. They use deterministic `All` selections and `ActiveZones` (by default hand and board). A continuous condition turning false removes that contribution. Avoid circular dependencies; an unresolved cycle fails closed.

For array operations, PF3 remembering, and continuous/conditional SelfTeleport setups, open the [variable examples](../examples/variables/README.md). For a concise list of their query rules, see [query reference](reference/queries.md).

## Limits

The native limits and expression/continuous rules are listed in [variable details](reference/variable-details.md). Treat entity IDs as match-local state and don't assume they'll point to a valid card after the entity disappears. Matching module and card data are required for multiplayer.
