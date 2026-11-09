# Card stats and metadata

These queries and effects read or edit the card's live components. They don't rewrite the card database entry or change the original printed values.

## Reading a stat

`SetVariableEffectDescriptor` can use `ValueSource` to copy a stat into an integer variable. The available stat names are:

| Name | Value |
| --- | --- |
| `Attack`, `Health`, `Cost` | Calculated attack, **maximum** health, and cost |
| `BaseAttack`, `BaseHealth`, `BaseCost` | Original `BaseValue`, without modifiers |
| `CurrentHealth` | Calculated maximum health minus damage taken, not below zero |
| `DamageTaken` | `Health.CurrentDamage` |
| `Level` | `Leveled.Value` (extra sun paid) or zero |

`StatComparisonQuery` compares a stat to a literal `Value` or another `OtherStat` on the selected card. `StatModifierQuery` compares a calculated stat with its own base; `Mode: "Buffed"` matches a positive difference, and `Mode: "Nerfed"` matches a negative difference. Damage taken is not the same thing as a health modifier.

```json
{
  "$type": "PvZCards.Engine.Queries.StatComparisonQuery, EngineLib, Version=1.0.0.0, Culture=neutral, PublicKeyToken=null",
  "$data": {
    "Stat": "Cost",
    "OtherStat": "Attack",
    "ComparisonOperator": "Greater"
  }
}
```

`VariableComparisonQuery` also supports numeric expressions that read built-in stats, such as `Expression: "Health"`, `ValueExpression: "Attack"`. For cross-card comparisons, select the variable's owner with its `EntityQuery`; don't assume the trigger target and primary target are the same card. See [stat examples](../../examples/card-features/README.md).

## Tags and subtypes

Use `AddTagEffectDescriptor`, `RemoveTagEffectDescriptor`, `AddSubtypeEffectDescriptor`, and `RemoveSubtypeEffectDescriptor` on an effect entity after the primary target filter. They act on each selected card's `Tags.tags` or `Subtypes.subtypes` list. Removal clears matching entries and does nothing if the card has no such component or entry.

```json
{
  "$type": "PvZCards.Engine.Components.RemoveTagEffectDescriptor, EngineLib, Version=1.0.0.0, Culture=neutral, PublicKeyToken=null",
  "$data": { "Tag": "my_custom_tag" }
}
```

`Tag` is a string. `Subtype` uses the game's numeric subtype ID, not the display name. To see the normal stock representation, inspect `Subtypes.subtypes` in the card data. A UI that caches subtype labels may need an additional refresh after a runtime change; this hasn't been fully verified in-game.
