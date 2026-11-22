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

### Counting events before reward conditions

Use a normal triggered `SetVariableEffectDescriptor` with `ExecutionTiming: "BeforeTriggers"` when reward conditions must include the current event. See [the complete effect entity](../examples/variables/event-counter.json). The default timing remains queued. `Duration` accepts `Permanent`, `EndOfTurn`, or `NextFighter`.

This uses the game's `TriggerValidator` and `EffectCreator`: add `TriggerSourceFilter`, `TriggerTargetFilter`, `SelfEntityFilter`, and ordinary conditions exactly as on other effects. Their nested queries retain their normal meanings. Early setters require `PrimaryTargetFilter.SelectionType: "All"`; they cannot pause trigger validation for a manual choice. Put zone/alive restrictions in the normal filters instead of assuming counters only work on the board.

The update commits once per effect entity and event before reward conditions run. Each queued trigger keeps its event's variable snapshot across target choices and concrete effect creation. Explicit variable assignments made by that chain update its own snapshot. Replace the old increment rather than keeping both. `VariableEventCounter` remains readable for older packs, but new effects should use the normal descriptor so source filters, conditions, and recipient selection are available.

Queued listeners from the same event begin with equal, independent snapshots. A write in one listener's chain does not rewrite a different listener's saved conditions. Continuations within that chain retain its own explicit writes.

For a one-time threshold response, count qualifying events with an early `Add` setter, then require the counter to equal 1 on the ordinary action. Put one descriptor on each effect entity. For example, Fruit of Shine counts its qualifying self-play/buff events before validation; its extra-attack entity checks that count is exactly 1. Later qualifying events advance the count and cannot grant another attack. This uses the normal source/target filters and condition language; no card-specific condition is needed.

### Changing other cards and remembering a death

`OwnerQuery` selects every recipient of a variable write. Use normal queries for several cards, including cards in hand, deck, or discard. The setter creates variable storage when needed; recipients do not have to declare a `Variables` component in advance. A primary selection and `OwnerQuery` are separate: avoid selecting several primaries that all resolve to the same recipients unless repeated writes are intentional.

To remember the original trigger participant, use `ValueSource: "TriggerTarget"` or `"TriggerSource"`, `ValueType: "Entity"`, `Operation: "Set"`, and a variable on a surviving owner. These sources read the saved event context even when the effect selects a different primary target. Use `Guid` instead if you want the definition rather than that physical card. A later effect can select the remembered entity with `VariableCardQuery`.

A death notification can occur before the card enters discard. Saving its ID and replaying it are separate actions in an ordered chain: the return effect must execute after native discard transfer and pass normal lane/zone legality. A saved ID does not move the card or make a premature return legal. If the chosen trigger runs before transfer, use the corresponding post-transfer trigger for replay; do not bypass the engine's zone bookkeeping or repeatedly retry in a loop.

Use `VariableCardQuery` when a stored GUID or entity variable should select cards later. A GUID may match several copies; an entity ID matches only the original copy. If you remember a PickFromThree database definition, capture its GUID with `PickedFromThreeQuery` and `DatabaseQuery`, not a physical entity that hasn't been created yet.

`Continuous` setters derive a current value from the model without repeatedly adding permanent increments. They use deterministic `All` selections and `ActiveZones` (by default hand and board). A continuous condition turning false removes that contribution. Avoid circular dependencies; an unresolved cycle fails closed.

For array operations, PF3 remembering, and continuous/conditional SelfTeleport setups, open the [variable examples](../examples/variables/README.md). For a concise list of their query rules, see [query reference](reference/queries.md).

## Limits

The native limits and expression/continuous rules are listed in [variable details](reference/variable-details.md). Treat entity IDs as match-local state and don't assume they'll point to a valid card after the entity disappears. Matching module and card data are required for multiplayer.
