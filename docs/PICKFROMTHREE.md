# PickFromThree

PickFromThree (PF3) shows a choice menu while keeping the normal card-targeting system in charge of selection and validation. You can choose from physical cards in a deck or discard pile, or from card definitions in the database. Those are not interchangeable: two copies with the same GUID are two physical cards, but one definition.

## Which selection mode?

| `SelectionType` | Picks from | Use when |
| --- | --- | --- |
| `PickFromThreeRandomTargets` | Physical cards | Every actual copy is a possible choice |
| `PickFromThreeUniqueGuidTargets` | Physical cards grouped by GUID | Show distinct definitions, but keep a real representative |
| `PickFromThreeCardDatabase` | Card definitions | Choose a type of card to create or conjure |

The menu shows up to three candidates, subject to each mode's selection rules. Use the standard `PrimaryTargetFilter` fields and a real query; don't add made-up wrapper objects.

## Selecting another target

A PF3 ability can also use an additional target. `AdditionalTargetSelectionType` can differ from the primary mode, and `AdditionalTargetFirst` selects it before PF3 when needed. This is useful when you pick a card first and then a lane for it—or select a lane before choosing which card can fit there.

For database choices, an effect may need a physical completion recipient. The working examples use `AdditionalTargetType: "Query"`, `AdditionalTargetQuery` with `SelfQuery`, and `OnlyApplyEffectsOnAdditionalTargets: true` where required. Keep those values when adapting the database templates.

## Using the chosen card later

`PickedFromThreeQuery` matches the selected GUID. `PickedFromThreeEntityQuery` matches the actual chosen/created entity. Use the latter when you must affect a specific physical copy. For longer-lived choices, store a GUID or entity in `Variables` and query it later.

The starting examples are [database choice](../examples/pick-from-three/effect-group-card-database-draw.json), [unique-GUID choice](../examples/pick-from-three/unique-guid-pick-1-from-deck.json), and [choose then place](../examples/pick-from-three/create-fruit-card-then-lane.json). More variations live under [examples/pick-from-three](../examples/pick-from-three/README.md).

PF3 has special handling for multiplayer commands, preview state, and consecutive menus. If you change its selection flow, test two menus in a row as well as a single choice. Details about queries and placeable lanes are in [query reference](reference/queries.md).
