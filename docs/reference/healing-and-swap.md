# Healing and swapping positions

These mechanics use normal effect entities. The native C++ implementation has been checked against the available game method signatures, but effect triggers and animation still need an Android gameplay test.

## Heal by the damage a target has taken

The stock `HealEffectDescriptor` keeps a **number** in `HealAmount`. To heal each recipient to full, set `HealAmount` to 1 and add `VariableMultiplier` after the heal descriptor in that effect entity:

```json
[
  {
    "$type": "PvZCards.Engine.Components.HealEffectDescriptor, EngineLib, Version=1.0.0.0, Culture=neutral, PublicKeyToken=null",
    "$data": { "HealAmount": 1 }
  },
  {
    "$type": "PvZCards.Engine.Components.VariableMultiplier, EngineLib, Version=1.0.0.0, Culture=neutral, PublicKeyToken=null",
    "$data": {
      "Expression": "DamageTaken",
      "Owner": "PrimaryTarget",
      "Divider": 1
    }
  }
]
```

This is the **pair of effect components**, not a complete effect entity. See [the complete example](../../examples/new-card-features/FullHeal_PerPrimaryTarget.json) for grouping, trigger and filters. `PrimaryTarget` reads the specific card receiving that runtime effect; it doesn't sum all recipients. `Target` elsewhere still means the trigger target.

## OverhealToHealth

The recipient needs the grantable `OverhealToHealth` ability. It may be innate or granted to a card with `GrantAbilityEffectDescriptor` and `GrantableAbilityType: "OverhealToHealth"`.

```json
{
  "$type": "PvZCards.Engine.Components.OverhealToHealth, EngineLib, Version=1.0.0.0, Culture=neutral, PublicKeyToken=null",
  "$data": { "Innate": true }
}
```

With 2 damage taken and a requested heal of 5, the stock heal repairs 2; the 3 excess is queued as a permanent +0/+3 `BuffEffect`. It is not a direct edit of `MaxHealth.BaseValue`. A cancelled or unhealable effect must not award bonus health.

See [innate ability](../../examples/new-card-features/OverhealToHealth.json), [grant it](../../examples/new-card-features/GrantOverhealToHealth.json), and [heal an Overheal recipient](../../examples/new-card-features/HealFive_OverhealRecipient.json).

## SwapPositionEffectDescriptor

A swap uses **two live board fighters**. Primary targeting selects the first fighter; a `SecondaryTargetFilter` selects the second. The secondary filter comes **after** `SwapPositionEffectDescriptor`, following the game's usual effect serialization order.

The runtime saves both lane placements, exchanges the cards, then writes native movement history for both. Their front/back order is part of the saved placement. It must not turn two fighters into one overlapping slot or leave one displaced if a move fails.

Use the [complete swap example](../../examples/new-card-features/SwapPosition.json) rather than a bare descriptor: the filters are important. Simultaneous animation and exactly-once `MoveTrigger` behavior are still unverified until tested on Android, including Team-Up and PvP replay.

## Engine points

The hooks use the game's `MultiplierSystem`, `HealSystem.ProcessEffect(HealEffect)`, `BuffSystem.ProcessEffect(BuffEffect)` and the native lane/movement-record pipeline. Keep stock healing and buff records involved so event listeners and card displays have the expected data. Host compilation verifies signatures; it cannot prove all client rendering behavior.
