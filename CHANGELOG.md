# Changelog

## Unreleased

This branch builds on [upstream `e6b7823`](https://github.com/LOVER6868/CustomComponents/tree/e6b7823e140e34918accd22ad8c4d2f1daa89db9). The entries below describe the current work, not separate releases or version numbers.

### Card mechanics

- Added **Level** choices with fixed or spend-all costs, X-based buffs, a `Leveled` value, and synced hand selection. Level stat and cost displays use the existing game UI.
- Added typed **card variables**, expressions, comparisons, array operations, continuous bindings, and multipliers. `LevelSpentMultiplier` uses `Leveled.Value`; `VariableMultiplier` can read card stats and evaluate each `PrimaryTarget` separately.
- Added **OverhealToHealth** and a position-swap effect. Overhealing queues a stock health buff; swapping uses native lane placement and movement history. Both still need device-level verification of triggers and animation.
- Added base-stat comparisons, stat buff/nerf checks, and effects to add or remove tags and subtypes.
- Added per-ability **PlayRequirement** checks for tricks.

### Abilities and targeting

- Added or extended Armor, Charge, Thrash, RandomKeyword, Withering, Multishot, AttacksOnlyInAdjacentLanes, and card-local SelfTeleport grants, including duration and removal handling.
- Added **PickFromThree** selection of physical cards and database definitions, with extra-target selection, picked-card queries, and PvP synchronization.
- Added named trigger firing, custom discard/return triggers, and support for destroying hidden BoardAbility entities.

### Cards and zones

- Added deck/discard transfer effects, direct deck deployment to lanes, deck ordering, subset tutors, and creation from card definitions.
- Preserved physical card identity through selection and transfers, including copies with the same GUID.
- Added placement queries, Team-Up/Aquatic creation options, herald information, and optional custom stat icons.

### Native integration

- Consolidated shared hooks, effect queues, model/copy bookkeeping, selection state, and frontend update handling. This reduces overlapping native detours.
- Improved Level affordability, targeting previews, and PickFromThree menu recovery across consecutive selections.
- Kept development diagnostics and generated files outside the distributed module.

### Documentation

- Reorganized short-lived patch notes into the maintained guides and reference pages.
- Matched the JSON examples to stock assembly-qualified type strings and game-style effect ordering.
- Removed only verified duplicate examples; the other recipes remain available.

Host compilation and source tests are not the same as testing on Android. The [compatibility page](docs/reference/compatibility.md) keeps track of what still needs game testing.
