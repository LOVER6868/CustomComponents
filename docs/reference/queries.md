# Queries

Queries work inside stock `CompositeAllQuery`, `CompositeAnyQuery`, and `NotQuery`. A physical card and a card definition with the same GUID are different objects, so pick the query that matches the domain you're using.

## Card identity and zones

| Query | Checks |
| --- | --- |
| `InDeckQuery`, `InDiscardPileQuery` | Physical membership in deck/discard |
| `InDeckGuidQuery`, `InDiscardPileGuidQuery` | Whether a definition's GUID is in the corresponding zone |
| `DiscardedFromQuery` | Custom hand (`Source: 1`) or deck (`Source: 2`) discard origin |
| `PickedFromThreeQuery` | The chosen definition GUID |
| `PickedFromThreeEntityQuery` | The chosen or created physical entity |
| `VariableCardQuery` | A card remembered by a variable |

A card being in discard doesn't prove it was destroyed on the board. Combine membership and discard origin when the distinction matters.

## Position and placement

`ListPositionQuery` compares a zero-based position in `Deck` or `DiscardPile`. `FromEnd: false` starts at the top/first card; `true` starts at the bottom/last. `Position` is the comparison value; `ComparisonOperator` supports `Equal`, `NotEqual`, `Less`, `LessOrEqual`, `Greater`, and `GreaterOrEqual`.

`OpenLaneForCardQuery` asks whether any card returned by `CardQuery` could fit a lane. `CardQueryUsesEntities: true` checks live traits on physical cards; false uses definitions. `CanBePlacedQuery` checks whether a candidate can fit the current board and can take a restricting `LaneQuery`. Neither query pays for a card or overrides a card's phase restrictions.

## Level, stat and variable queries

`LeveledQuery` compares a selected `Leveled.Value` with a number, but a missing `Leveled` marker does not match. `StatComparisonQuery` compares any supported base or calculated stat against a literal or another stat. `StatModifierQuery` checks whether attack, health or cost is above/below its base; it doesn't confuse lost HP with a health nerf.

`VariableComparisonQuery` reads stored values or derived expressions, including built-in card stats, with a selectable `EntityQuery` owner and `Any`/`All` matching. `HasVariableQuery` tests whether a variable exists. See [variables](../VARIABLES.md) and [card stats](card-stats.md).

[Query examples](../../examples/queries/README.md) · [PF3 guide](../PICKFROMTHREE.md)
