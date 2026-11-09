# Card Stat & Metadata Feature Integration (2026-11-08)

**Status:** Source-level implementation, **NOT Android-built or in-game-validated**.

This patch is based on `CustomComponents_LeveledValue_full.zip` (which includes the previous LevelSpentMultiplier and Level ValueSource implementation). You do not need to install any earlier patch first.

## Implemented in this source patch

### Read base, current, and damaged card statistics

`SetVariableEffectDescriptor.ValueSource` additionally accepts `BaseAttack`, `BaseHealth`, `BaseCost`, `CurrentHealth`, and `DamageTaken` (all of `ValueType: "Int"`). The existing `Attack`, `Health`, `Cost`, and `Level` sources retain their original behavior. `Base*` is the raw `ModifiableInt.BaseValue`. `CurrentHealth = max(0, calculated max health - CurrentDamage)`; `Health` when used in the NEW query shorthand below means **calculated maximum health**.

### `StatComparisonQuery`

This is an entity query, serializable as:

```json
{
  "$type": "PvZCards.Engine.Queries.StatComparisonQuery, EngineLib",
  "$data": {
    "Stat": "BaseAttack",
    "ComparisonOperator": "Greater",
    "Value": 2
  }
}
```

`Stat` accepts `Attack`, `Health`, `Cost`, `BaseAttack`, `BaseHealth`, `BaseCost`, `CurrentHealth`, `DamageTaken`, or `Level`.

For card-to-card comparisons, set `OtherStat` and provide a single-card `OtherQuery` (e.g., `SelfQuery`, a source-relative query). With no `OtherQuery`, both operands belong to the candidate card:

```json
{
  "$type": "PvZCards.Engine.Queries.StatComparisonQuery, EngineLib",
  "$data": {
    "Stat": "Cost",
    "OtherStat": "Attack",
    "ComparisonOperator": "Greater"
  }
}
```

### `StatModifierQuery`

Compares calculated values against printed base values, independently for each stat:

```json
{
  "$type": "PvZCards.Engine.Queries.StatModifierQuery, EngineLib",
  "$data": {
    "Stat": "Health",
    "Mode": "Buffed"
  }
}
```

`Stat`: `Attack`, `Health`, `Cost`. `Mode`: `Buffed`, `Nerfed`, `Modified`, `Unmodified`. For health, `CurrentDamage` is not counted as a stat modifier. Cost uses the effective game cost (including `IsFree`), but the original cost is always the printed `SunCostValue.BaseValue`. This tests the *net* difference, not whether modifier records exist (equal buffs and nerfs cancel).

### `VariableComparisonQuery` stat expressions

Existing integer `Expression` and `ValueExpression` now recognize stat identifiers when no variable of that name exists. For example:

```json
{
  "$type": "PvZCards.Engine.Queries.VariableComparisonQuery, EngineLib",
  "$data": {
    "Expression": "Cost",
    "ValueExpression": "Attack",
    "ComparisonOperator": "Greater"
  }
}
```

`CompareEntityQuery` is an OPTIONAL one-card query for the right-hand expression. `EntityQuery` continues to designate owners of the left-hand expression. Exactly one right-hand entity must resolve; unresolved comparisons fail closed. Existing variables take precedence over built-in stat identifiers on naming collisions.

### `AddTagEffectDescriptor` and `AddSubtypeEffectDescriptor`

These plug into the existing custom descriptor-to-effect conversion and effect preparation pipeline. The target is the *effect's resolved primary TargetId*; use normal PrimaryTargetFilter selection. No extra target query is needed.

```json
{
  "$type": "PvZCards.Engine.Components.AddTagEffectDescriptor, EngineLib",
  "$data": {"Tag": "my_custom_tag"}
}
```

```json
{
  "$type": "PvZCards.Engine.Components.AddSubtypeEffectDescriptor, EngineLib",
  "$data": {"Subtype": 4}
}
```

Each append is idempotent. `AddSubtype` locates stock `Subtypes` list by reflection; if the installed EngineLib uses a different storage layout, it **fails closed** with a warning (check logs), rather than writing into an unknown field. The stock JSON sample uses `subtypes`. The tag effect uses the existing `Tags.TagList` API. Neither is asserted to emit a special visual animation or replay/history record.

## Not implemented in this patch

- **OverhealToHealth grantable ability**: needs a verified native heal-application callback with actual applied heal amount, and a stock `BuffEffect` queued with the excess. Directly increasing MaxHealth would violate the request for buff triggers/history/colors.
- **`HealAmount: "Full"`**: stock `HealAmount` is an integer field, so writing a string without a pre-deserialization bridge would break stock deserialization. Need to identify stock HealEffect/HealEffectDescriptor conversion and change exactly that effect's *effective* amount to `CurrentDamage` at resolution, not fake a large constant.
- **SwapPositionEffectDescriptor**: needs a verified atomic two-entity lane exchange with stack/front-back preservation and two stock Move triggers/history records. Sequential placement on occupied lanes risks transient invalid state and missing triggers.

These features require the PvZ Heroes EngineLib/runtime method metadata and device tests, and are deliberately **not** represented as working code in this archive.

## Metadata removal effects (2026-11-08 follow-up)

`RemoveTagEffectDescriptor` and `RemoveSubtypeEffectDescriptor` mirror the existing add-effects and use the same resolved primary target.

```json
{"$type":"PvZCards.Engine.Components.RemoveTagEffectDescriptor, EngineLib","$data":{"Tag":"my_custom_tag"}}
```

```json
{"$type":"PvZCards.Engine.Components.RemoveSubtypeEffectDescriptor, EngineLib","$data":{"Subtype":4}}
```

Removal is idempotent: if the target has no corresponding `Tags` / `Subtypes` component, it succeeds without adding one; if the managed list contains multiple matching items, all are removed. The implementation calls the stock managed list `RemoveAt` operation, not `memmove` or native direct-list edits. The actual live `Tags.TagList` and `Subtypes` field layout must still match the existing add-effects. This is source-implemented, **not compiled or runtime-tested on Android**.
