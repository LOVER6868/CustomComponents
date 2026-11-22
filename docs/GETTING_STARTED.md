# Getting started

## Install

Put this folder in `Sprig-main/Bloom/CustomComponents`. From PowerShell, apply the bundled BNM fix once for your Sprig checkout:

```powershell
.\patches\BNM-Android\Apply.ps1 -SprigRoot 'C:\path\to\Sprig-main'
```

Then build Sprig using the normal project build script. The patch has its own instructions at `patches/BNM-Android/README.md`.

## Where card JSON goes

A card definition has an `entity.components` array. Individual components, such as `Variables` or `OverhealToHealth`, go in that array. Triggered effects go inside an `EffectEntitiesDescriptor`, whose `$data.entities` array contains effect entities with their own `components` arrays.

A normal effect entity is arranged like this:

1. `EffectEntityGrouping`
2. Trigger (for example `PlayTrigger`)
3. `TriggerTargetFilter` and any other trigger conditions
4. `PrimaryTargetFilter`
5. Effect descriptor (for example `HealEffectDescriptor`)
6. Multiplier and/or `SecondaryTargetFilter`, if the effect uses them

This is the layout found in the stock card data. Don't flatten effect components into the card's top-level component list.

A complete example of that nesting is [per-target healing](../examples/new-card-features/FullHeal_PerPrimaryTarget.json). A smaller [variable example](../examples/variables/defaults.json) shows a single component.

## JSON conventions

Stock and custom components both use `$type` and `$data`. Examples use the full assembly-qualified form, such as:

```json
{
  "$type": "PvZCards.Engine.Components.OverhealToHealth, EngineLib, Version=1.0.0.0, Culture=neutral, PublicKeyToken=null",
  "$data": { "Innate": true }
}
```

Queries use `PvZCards.Engine.Queries` instead of `PvZCards.Engine.Components`. For names supplied as *values* (such as `GrantableAbilityType: "OverhealToHealth"`), keep the name format shown by that descriptor; not every field is an assembly-qualified type.

When testing, start with one card and one effect. For PvP, both players must have the same native module and gameplay data.
