# Queries

Queries nest in stock `CompositeAllQuery`, `CompositeAnyQuery` and `NotQuery`.
A database definition and a physical card with the same GUID are different domains.

## Identity and membership

| Query | Matches |
| --- | --- |
| `InDeckQuery` | The physical card represented in a main deck |
| `InDiscardPileQuery` | A physical card currently in discard |
| `InDeckGuidQuery` | A definition whose GUID occurs in a main deck |
| `InDiscardPileGuidQuery` | A definition whose GUID occurs in discard |
| `DiscardedFromQuery` | Custom hand (`Source: 1`) or deck (`Source: 2`) discards |
| `PickedFromThreeQuery` | The selected GUID |
| `PickedFromThreeEntityQuery` | The selected/produced physical card |

Combine provenance with current discard membership when both matter. Excluding
custom hand/deck discards does not prove a card was destroyed on the board.

## Ordered positions

`ListPositionQuery` checks a physical card's position in an ordered pool.

| Field | Default | Meaning |
| --- | --- | --- |
| `Pool` | `Deck` | `Deck` or `DiscardPile` |
| `Position` | `0` | Non-negative zero-based comparison value |
| `FromEnd` | `false` | Count from the bottom/last entry when true |
| `ComparisonOperator` | `Equal` | `Equal`, `NotEqual`, `Less`, `LessOrEqual`, `Greater`, `GreaterOrEqual` |

Bottom five deck cards: `Pool: Deck`, `FromEnd: true`, `Position: 4` and
`ComparisonOperator: LessOrEqual`. Zero is top/first with FromEnd false and
bottom/last with FromEnd true. Invalid pools/operators and negative positions do
not match. When negating a position query, include zone membership separately so
unrelated cards do not pass merely because they lack a position.

## Placement

`OpenLaneForCardQuery` matches lanes. `CardQuery` accepts a query or composite
selecting cards to consider. `CardQueryUsesEntities: false` searches definitions;
true searches physical cards and their active traits. A lane matches if at least
one matching card fits it.

Use `PickedFromThreeQuery` for a picked definition, or `PickedFromThreeEntityQuery`
with `CardQueryUsesEntities: true` for a physical selection. See the
[PF3 guide](PICKFROMTHREE.md) for dependent lane-query timing.

`CanBePlacedQuery` matches cards that fit the current board, optionally restricted
by `LaneQuery`. A selected additional lane can restrict the contextual check.
Both definitions and physical entities are supported.

These predicates check placement, capacity and Team-Up/Aquatic traits. They do not
check sun affordability or hand-play timing. Relevant subset-creation grants are
considered; the stock placer still decides the actual placement outcome.

See the [query examples](../examples/queries/README.md).

## Committed Level state

`LeveledQuery` compares `Value` against `ComparisonOperator`. The six comparison
names match ListPositionQuery. A missing Leveled component does not match. Combine
with SelfQuery, NotQuery or composite queries as usual. See [Level](LEVEL.md).

## Card variables

`VariableComparisonQuery` compares stored or derived integer expressions, typed
identity values, or array membership. `HasVariableQuery` checks presence.
`VariableCardQuery` matches definitions or physical cards remembered by another
card. Each accepts an `EntityQuery` for the cards holding the variables.
See [variables](VARIABLES.md) for fields, examples and missing-value behavior.
