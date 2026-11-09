# Level

Level lets a card offer extra payment options while it's in hand. The normal cost is always one of the choices; you don't put that base option in `Levels`.

```json
{
  "$type": "PvZCards.Engine.Components.Level, EngineLib, Version=1.0.0.0, Culture=neutral, PublicKeyToken=null",
  "$data": {
    "Levels": [
      { "SunCost": 2, "Attack": 2, "Health": 2 },
      { "SunCost": 3, "Attack": 4, "Health": 0 }
    ]
  }
}
```

A `SunCost` in this table is **additional** sun/brains, not the card's printed cost. The options keep their written order and replace one another: their bonuses never accumulate just because you switch modes. Unaffordable fixed options can stay selected, but the normal play validator will reject them. Your base cost is still stored in the usual `SunCost.SunCostValue.BaseValue`.

## Spend everything and X

Set `SunCost` to `-1` to spend all currently available resources, provided the normal cost is affordable. If a 1-cost card has 5 sun available, the extra payment is 4, so `"X"` gives +4. The dynamic option returns to base when no extra payment is possible. `"X"` in Attack or Health uses the extra amount paid, not the whole card cost. Negative stat bonuses work too. The ready-to-copy examples are [spend all and X](../examples/abilities/level-spend-all-x.json) and [fixed costs](../examples/abilities/level-fixed-cost-x.json).

## Reading the selected Level

When an upgraded Level is selected, `Leveled.Value` holds the **extra sun spent**. `Leveled.EntryIndex` identifies the selected option (starting at zero); it's a different number. The ordinary unlevelled choice has no positive `Leveled` marker, so the Level value reads as zero where the custom variable resolver uses it.

You can check the value with `LeveledQuery`, copy it into a variable with `SetVariableEffectDescriptor` and `ValueSource: "Level"`, or multiply a supported stock effect with `LevelSpentMultiplier`. A base damage amount of 2 and Level value 3 produces 6 damage, not 2 times the printed total cost.

See [level multiplier](../examples/variables/level-spent-multiplier.json), [snapshot Level](../examples/variables/snapshot-level.json), and [requirements](../examples/play-requirements/requires-level-three.json).

## Timing and cleanup

Level applies normal `ModifiableInt` cost/attack/health modifiers while the card is in hand; changing modes does **not** produce a stock `BuffEffect` or fire buff triggers. The selected cost affects the hand glow and available drag targets. The game’s shake gesture cycles the choices, and the selected cost gets a blue tint. Level keeps committed payment information for play effects. Hand choices reset as the card moves between zones. Don't assume that an effect running long after a card has left its play state can still read its former Level value; snapshot it when needed.

The card UI cycles the available modes, updates costs and stats, and synchronizes choices for supported multiplayer paths. For affordability rules and trick restrictions, see [Play requirements](reference/play-requirements.md). Any new Level UI behavior should be tested in both the hand preview and the actual play, not just in the JSON loader.
