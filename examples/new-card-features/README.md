# Healing and swapping

These are properly nested card-component fragments. Add an `EffectEntitiesDescriptor` to `entity.components`, or merge its `entities` into an existing descriptor. See [healing and swapping](../../docs/reference/healing-and-swap.md).

## Start with these

- [Heal each target for its own DamageTaken](FullHeal_PerPrimaryTarget.json)
- [Innate OverhealToHealth](OverhealToHealth.json)
- [Grant OverhealToHealth](GrantOverhealToHealth.json)
- [Heal an Overheal recipient](HealFive_OverhealRecipient.json)
- [Swap two fighters](SwapPosition.json)

## Lane and damage features

See `BuffPlantsInFullLane.json` for the two-Plants-in-a-lane condition, `MoveToAnyLaneOnRight.json` for lane targeting, and `DamageByLaneDistance.json` for a per-target distance multiplier. The `*_Innate.json` and `Grant*.json` files show the five new grantable abilities. The single-query files are meant to be nested inside a target filter, not loaded as complete cards. See [lane and damage reference](../../docs/reference/lane-and-damage.md).
