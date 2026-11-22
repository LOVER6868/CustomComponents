# Trick play requirements

`PlayRequirement` goes on the **effect entity** alongside its `PlayTrigger`, filters and effect descriptor. Its `Query` is evaluated for that ability, not globally for every effect on the card. This lets one trick offer several independently available actions.

```json
{
  "$type": "PvZCards.Engine.Components.PlayRequirement, EngineLib, Version=1.0.0.0, Culture=neutral, PublicKeyToken=null",
  "$data": {
    "Query": {
      "$type": "PvZCards.Engine.Queries.LeveledQuery, EngineLib, Version=1.0.0.0, Culture=neutral, PublicKeyToken=null",
      "$data": { "Value": 1, "ComparisonOperator": "Equal" }
    }
  }
}
```

A trick is playable when the normal game rules allow it **and** at least one ability has a satisfied requirement and valid primary targeting. The module checks requirements against the source before payment, then lets stock validation make the final decision. A requirement doesn't override ordinary costs or permit illegal lane placement.

You can combine conditions with `CompositeAllQuery`, `CompositeAnyQuery`, or `NotQuery`. The stock `Unplayable` marker is a separate, unconditional restriction, not a substitute for per-ability requirements.

For two different Level-gated actions on one trick, use [two-level-draw-abilities.json](../../examples/play-requirements/two-level-draw-abilities.json). Smaller queries are in [examples/play-requirements](../../examples/play-requirements/README.md).
