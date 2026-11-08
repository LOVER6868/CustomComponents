# Card variables

Variables belong to a physical card. Use them to remember a choice, track a
counter, or derive a value from the current board. Two copies of a card have
independent tables. Values persist across zone changes until an effect changes
or removes them; creating another card starts from its authored defaults.

See the [example catalog](../examples/variables/README.md) for complete fragments.
Components use `PvZCards.Engine.Components`; queries use
`PvZCards.Engine.Queries`, all in `EngineLib`.

## Defaults and types

Add a `Variables` component with a `Values` list:

```json
{
  "Values": [
    { "Name": "CardsOnBoard", "Type": "Int", "Value": 0 },
    { "Name": "ChosenAnimal", "Type": "Guid", "Value": 721 },
    { "Name": "RememberedCards", "Type": "EntityArray", "Value": [] }
  ]
}
```

This is the `$data` portion; examples include the full `$type` wrapper.
Supported types are `Int`, `Guid`, `Entity`, `IntArray`, `GuidArray` and
`EntityArray`. GUIDs identify definitions; entities identify physical cards in
the current match. Arrays preserve order and duplicates. Integer values are
signed 32-bit numbers. Names are case-sensitive identifiers containing letters,
digits and underscores, starting with a letter or underscore.

The serialized backing field, `Data`, is maintained by the module. Author
`Values` rather than editing that encoding. Copies own their tables independently;
stored entity IDs still refer to the same physical entities. Prediction copies
retain IDs within their own model. Missing or retired entities do not match a
`VariableCardQuery`; a GUID can still match another copy of the same definition.

## SetVariableEffectDescriptor

Place this descriptor on an effect entity with its usual trigger and primary
target filter. It is an ordinary nonblocking effect, so native trigger ordering,
target selection and PF3 continue to apply.

| Field | Meaning |
| --- | --- |
| `Variable` | Name to write |
| `Owner` | `Target` by default; `Self` writes on the source card |
| `OwnerQuery` | Optional composite-capable query selecting owners; overrides `Owner` |
| `Operation` | `Set` by default; also `Add`, `Subtract`, `Remove`, `Append`, `Clear`, `RemoveAt`, `RemoveValue` |
| `ValueType` | `Int` by default, or another supported type |
| `ValueSource` | Value resolver, listed below |
| `Value` | Literal number, GUID, entity ID, or array index |
| `Expression` | Numeric expression, or variable name for variable/array reads |
| `Query` | Source query for query-based values |
| `ActiveZones` | Continuous source zones; default `Hand,Board` |

`Add` and `Subtract` require integers and start an absent variable at zero.
`Append` accepts a scalar or array of the destination's element type.
`RemoveValue` removes every matching occurrence; `RemoveAt` uses a zero-based
index. `Clear` empties an existing array; `Remove` deletes the named entry.
The last three use the default integer value when no input is needed.

| ValueSource | Result |
| --- | --- |
| `Literal` (default) | `Value`, or an integer `Expression` when supplied |
| `Variable` | Copy the variable named by `Expression`, with an exact type match |
| `Query` | Capture physical IDs or GUIDs from live query matches |
| `DatabaseQuery` | Capture GUIDs from card-database query matches |
| `QueryCount` | Number of live query matches, as `Int` |
| `Self`, `Target` | Source or runtime target as a GUID or entity |
| `PrimaryGuid` | Runtime target's GUID; use `DatabaseQuery` + `PickedFromThreeQuery` for database PF3 choices |
| `Attack`, `Health`, `Cost` | Owner's current calculated stat, as `Int` |
| `ArrayElement` | Scalar from the array named by `Expression`, at index `Value` |
| `ArrayLength` | Length of the array named by `Expression` |

Scalar query captures require exactly one match. Array captures accept zero or
more matches and retain finder order. Database captures cannot produce physical
entity references. Author literal arrays in defaults, then copy or extend them;
the descriptor's `Value` field is a scalar.

Expressions read variables on each selected owner. `QueryMultiplier` can multiply
an integer assignment, so `Value: 1` with a board-count query produces the count.
Alternatively use `ValueSource: QueryCount` directly. Use one multiplier component
per effect entity.

## Expressions and comparisons

Expressions support `+`, `-`, `*`, `/`, `%`, parentheses, unary signs, `min`,
`max`, `clamp`, `abs`, `len`, `sum` and `at`. For example:

```text
max(0, CardsOnBoard * 2 - 1)
sum(Numbers) + at(Numbers, 0)
```

`len` accepts any array. `sum` and `at` accept integer arrays; typed GUID/entity
elements use `ArrayElement` instead. Division truncates toward zero. GUIDs and
entity references are never implicitly treated as arithmetic values.

`VariableComparisonQuery` selects owners with `EntityQuery`. Omit it to use the
context's Self card (the ability owner), then its source or candidate when no
Self exists. Explicit owner queries retain the native triggering context.
Use `Variable` for a direct value or `Expression` for computed integer values.
The right side is `Value`, or `ValueExpression` when present.

Integer comparisons are `Equal`, `NotEqual`, `Greater`, `GreaterOrEqual`, `Less`
and `LessOrEqual`. GUID/entity scalars allow equality comparisons. Arrays allow
`Contains` and `NotContains` against a scalar. `MatchMode` is `Any` by default;
`All` requires every selected owner to pass. An empty owner selection fails.
`HasVariableQuery` distinguishes a missing entry from an entry containing zero
or an empty array.

## Using remembered cards

`VariableCardQuery` reads a named variable from its `EntityQuery` owners and
matches candidate cards. A GUID or GUID array matches definitions; an entity or
entity array matches exact live identities. Combine it with other queries and
use it as a subset query for drawing or creation.

To remember a database PF3 choice, write a `Guid` with `Owner: Self`,
`ValueSource: DatabaseQuery`, and `Query: PickedFromThreeQuery`. A later trigger
can draw from a `VariableCardQuery` reading that variable on `Self`.
Keep the database PF3 filter's `AdditionalTargetType: Query`,
`AdditionalTargetQuery: SelfQuery`, and `OnlyApplyEffectsOnAdditionalTargets: true`
so the stock phase has a physical completion recipient. The setter still stores
the chosen definition through `PickedFromThreeQuery`.
The examples show selection and later drawing as separate abilities. Replace
the demonstration plant subset with your animal subtype query.

## VariableMultiplier

Add `VariableMultiplier` alongside an effect descriptor that uses the native
multiplier calculator. Give it an `Expression`; its inherited `Query` selects
the cards whose variables supply that expression. Without a query it uses the
source card. `Aggregation` defaults to `Single`, requiring exactly one owner;
`Sum`, `Min` and `Max` support multiple owners. Native `Divider` applies once,
with zero treated as one, following stock rules.

This extends the shared calculator rather than replacing damage, buff, heal or
draw systems. An effect that never consults the native calculator does not gain
multiplier support merely by adding the component. The multiplier changes
numeric amounts, not GUIDs, entity references or arrays.

## Reactive continuous values

With `Continuous`, a setter defines a derived value rather than repeatedly
mutating stored state. The primary selection must be `All` with the normal
unlimited target scope. Use `ActiveZones` to select source activity from `Hand`,
`Board`, `Deck` and `DiscardPile`. Defaults are hand and board. Deck membership
uses the materialized physical deck; reading a variable never materializes cards.

Each read starts with the stored value and applies active contributions in
effect-entity ID order. An `Add` contribution is therefore a current contribution,
not an accumulating tick. Leaving the active zones or failing a condition removes
that contribution and reveals the underlying stored value. Snapshot setters
continue to write the underlying table.

The native trigger validator evaluates the rule's conditions. Use
`SelfEntityFilter` to gate a source by its variables. Keep dependencies acyclic:
reading a variable that depends on itself, directly or indirectly, fails closed.
Continuous target selections must be deterministic; do not use manual, random,
PF3, additional targets, sorted scopes or target-count limits for reactive rules.

The module tracks effect processing/completion and synchronized hand-mode
commands. A simulation revision invalidates resolved values; a structural entity
list change rebuilds the definition index. There is no frame timer or repeated
native continuous-effect enqueueing for these bindings. External C++ systems
that mutate state outside those boundaries must call
`Engine::SimulationChanges::Changed(model)` and notify `settled` after the change.

Conditional continuous SelfTeleport grants use the same derived reads and are
withdrawn when their condition stops passing. They are source-owned permission
contributions and do not emit repeated ability-grant events. Ordinary triggered
SelfTeleport grants retain the normal grant/removal/duration path.

## Limits and multiplayer

There are at most 64 variables per card, 256 elements per array, 512 characters
and 128 nodes per expression, 32 nested reads, and a 4,096-read work limit per
evaluation tree. Malformed payloads, type mismatches, invalid indices, overflow,
division by zero and missing inputs reject assignments or fail queries. They do
not partially overwrite a stored table.

State is serialized on card components and copied through native prediction and
pool lifecycles. Both clients need the same module and card definitions. No new
player command or Python server protocol is introduced: setters run as
deterministic effects and reactive values derive from model state. Device and
two-client PvP testing are still required before treating a new card as verified.

Ordered post-effect sequences are a separate feature. This foundation does not
add an `AfterEffects` field to stock descriptors.
