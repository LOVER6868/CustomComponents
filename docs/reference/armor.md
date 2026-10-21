# Armor

## Identity

```text
Custom GrantableAbilityType ID: 19
Managed gameplay component: stock PvZCards.Engine.Components.Armor
Storage: Numeric
Presentation: stock Armor special-ability icon
```

Armor is intentionally **not** a custom managed component. The module adapts the stock
Armor component into the custom GrantAbility registry.

## Storage

`components/Armor.hpp` resolves:

```text
Armor.ArmorAmount
Entity.GetOrAddComponent
Entity.GetComponent
Entity.HasComponent
```

`Armor::GetValue(entity, create)` returns the stock `ArmorAmount` value used by
`NumericAbility`.

## Grant/remove

Armor uses the generic `GrantAbilitySystem` numeric path. Add/Set grouping and removal
are handled by `NumericAbility`; the GrantAbility system handles duration and history.

## Presentation

`AbilityDefinitions` maps Armor to stock `SpecialAbilityType` 8. The generic
`GrantAbilityPresentation` bridge therefore exposes active custom Armor as the stock
Armor special ability and maps custom grantable ID 19 to the Armor sprite type.

This architecture was chosen after proving that gameplay state and visual state are
separate concerns: custom history records and presentation mapping are required in
addition to changing the Armor value.

## Team-Up

No Armor-specific Team-Up logic is documented in the current module.

## Status

Working through the generic numeric grant/remove/duration/history/presentation pipeline.
