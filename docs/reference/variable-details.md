# Variable rules and limits

Use the [variable guide](../VARIABLES.md) for common card patterns. These are the details that matter when you're building queries, arrays, or continuous rules.

## Storage and types

Variables are stored on each physical card. Names are case-sensitive and begin with a letter or underscore; the remaining characters may also be digits. Integers are signed 32-bit. `Guid` values point to definitions, `Entity` values to a physical card in the current model, and the three array forms hold ordered values of one type. Arrays may contain duplicates.

The module maintains its own serialized backing field (`Data`). Author `Values` on the card; don't write `Data` yourself. Copies get their own tables, while stored entity IDs continue to refer to the corresponding physical entity in the copied model. A missing or retired entity won't match a physical-card query.

## Setters and comparison queries

`SetVariableEffectDescriptor` can write a literal, copy another variable, read a query/count or built-in stat, capture a GUID/entity, or read an array item. `Operation: "Append"` requires matching types; `RemoveValue` drops every matching value; `RemoveAt` takes a zero-based index. Scalar query captures require exactly one match. Array captures can contain zero or more matches in finder order.

`VariableComparisonQuery` supports `Equal`, `NotEqual`, `Greater`, `GreaterOrEqual`, `Less`, `LessOrEqual` for numbers. GUID/entity scalars support equality comparisons, while arrays use `Contains`/`NotContains` with a scalar. `EntityQuery` selects the cards whose variables or stats are evaluated; default matching uses `Any`, while `All` requires every owner to pass. An empty selected-owner set fails. `HasVariableQuery` distinguishes a missing value from zero or an empty array.

## Expressions and multipliers

Expressions allow arithmetic, unary signs, parentheses and `min`, `max`, `clamp`, `abs`, `len`, `sum`, `at`. `sum` and `at` work on integer arrays; `len` accepts any array. Division truncates toward zero. A GUID or entity ID isn't silently converted to an arithmetic number.

`VariableMultiplier` only affects stock effects that ask the multiplier calculator for their amount. A query can choose one or several owners; `Single` needs exactly one, while `Sum`, `Min` and `Max` aggregate them. `Owner: "PrimaryTarget"` is a separate mode that evaluates the receiving card of **each runtime effect**. `Owner: "Target"` is not a replacement for that mode. Do not combine incompatible ownership modes.

## Continuous values

A `Continuous` setter contributes a current derived value rather than repeatedly changing the stored table. Contributions are applied in effect-entity order. Leaving an active zone or failing a condition removes that contribution. The normal active zones are hand and board, but deck and discard can be selected explicitly.

Continuous rules must have deterministic `All` target selection; they cannot use random, manual or PF3 choices, extra-target interactions, sorting, or target-count limits. Dependencies must be acyclic; a cycle fails closed. Systems that mutate model state outside the normal effect/command boundaries need to publish a simulation revision and settled notification so derived values refresh.

## Safety limits

A card can store at most **64 variables**, each array at most **256 elements**. An expression is limited to **512 characters**, **128 nodes**, and **32 nested reads**, with a **4,096-read** budget for an evaluation tree. Type errors, invalid indices, overflow, division by zero and missing required data fail rather than partially writing the table. Multiplayer needs matching module code and card definitions. These values are model state, not a new server message format.
