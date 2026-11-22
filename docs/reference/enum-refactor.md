# Enum refactor: native and JSON compatibility

The refactor centralizes **internal C++ type safety**. It does not change card JSON,
IL2CPP field sizes, wire values, history kind IDs, or native method signatures.

| Area | New/internal enum | Native or authored representation |
|---|---|---|
| Effect duration | `Engine::EffectDurations::Duration` | int: Permanent 0, EndOfTurn 1, NextFighter 2, StartOfTricks 3; old aliases and `Parse` preserved |
| Custom ability | `Abilities::CustomAbilityIds::Id` | int 19–35; existing public `...::Id` aliases preserved |
| Comparisons | `Core::Comparisons::Operator` | Authored strings (`Equal`, `LessOrEqual`, etc.) unchanged |
| Variable assignment | `Variables::Operation` | Authored strings (`Set`, `Add`, `Subtract`, `Remove`, etc.) unchanged; saved variables still `V1;` |
| Variable multiplier owners | `Variables::MultiplierPolicy::OwnerMode` | Authored `Owner` values `Self`, `Query`, `PrimaryTarget`, or empty (legacy default); unsupported `Target` still fails |
| Variable multiplier aggregation | `Variables::MultiplierPolicy::Aggregation` | Authored `Aggregation` values `Single`, `Sum`, `Min`, `Max`; missing field still defaults to `Single` |
| Variable value types | `Variables::Kind` | Explicit numeric 0–5; saved variables still `V1;` |
| Card stats | `Engine::CardStatValues::Stat` | Authored stat names unchanged; string overloads preserved |
| Triggered grants | `TriggeredGrants::Policy::Row` and `Operation` | **Same eight integers and `;` row terminator**; history encoding `triggered-policy-v1` unchanged |
| Withering zones | `GrantModes::WitheringZone` | Int 1 Hand, 2 Deck, 3 Board; grant counters remain int |
| Target scope sorting | `TargetScopeSupport::SortValue` | Int 3 ListPosition, 4 LaneIndex; stock sort values untouched |
| Extra combat | `ExtraCombat::StepStage` | Native `ExtraCombatStepEffect.Stage` stays int, with values 0–5 unchanged |
| Target selection | `TargetSelectionTypes::Type` | Verified stock ints 1 Manual and 5 CardDatabase only |
| Play DTO | `PlayTypeCodes::Type` | Verified wire codes 0 control and 3 surprise; no layout change |
| Ability removal | `AbilityRemoval::RemovalMode` | JSON `All` and `SourceAndDuration`; native Complete sentinel unchanged |
| Deck move | `MoveCardInDeckSystem::MoveMode` | JSON `Add` or `Move`; binary carrier encoding unchanged |
| PF3 target lights | `TargetLightStage` | Cache-only state, not serialized |

**Important:** Because the serialized representations and IL2CPP layouts are
fixed, these enums should NOT be placed directly into existing BNM custom fields
or persisted payloads unless explicitly converted to the original native type.
The old `int` APIs are retained as native boundaries. Ability IDs, GUIDs,
custom tags, dynamic registry names, and unknown stock enum codes remain open
values rather than being forced into closed C++ enums.

`tests/EnumRefactorRegression.cpp` checks the wire IDs, string parsing,
variable compatibility and triggered-grant ledger roundtrip. Syntax checking
`main.cpp` also covers the full combined module. **Device tests are still
required** for IL2CPP enum deserialization, replay/AI prediction and stock
ability/buff/cost expiration (particularly StartOfTricks).

## VariableMultiplier owner resolution follow-up

`variables/MultiplierPolicy.hpp` is a dependency-free policy for parsing and
validating owner/aggregation choices and accumulating integer expression results.
`variables/Multiplier.hpp` still owns `QueryScope`, live entity resolution and
last-known lane fallback. `ResolveOwners` uses a vector rather than an optional
vector, retaining the existing empty-or-invalid failure convention.

- Empty `Owner`: default to Self without a query, or evaluate the supplied query.
- `Self`: rejects an explicit owner query.
- `Query`: requires an explicit owner query.
- `PrimaryTarget`: requires an active primary recipient, rejects an owner query,
  and supports only `Single`; it evaluates separately for each effect recipient.
- `LaneDistance` remains special **only** for PrimaryTarget, including the
  existing departing-fighter last-known-lane fallback.
- Invalid owner modes (including `Target`), invalid aggregations, no matches,
  wrong Single cardinality and 32-bit arithmetic overflow all still fail closed.

Host checks: `tests/MultiplierPolicyRegression.cpp` tests the pure policy;
`tests/OverhealSwapSourceRegression.py` checks the native integration shape.
Runtime evaluation, native hook behavior and effect routing still require
Android gameplay testing.
