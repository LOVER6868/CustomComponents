# Healing, movement and transformation

These are properly nested card-component fragments. Add an `EffectEntitiesDescriptor` to `entity.components`, or merge its `entities` into an existing descriptor. See [healing and swapping](../../docs/reference/healing-and-swap.md).

## Start with these

- [Heal each target for its own DamageTaken](FullHeal_PerPrimaryTarget.json)
- [Innate OverhealToHealth](OverhealToHealth.json)
- [Grant OverhealToHealth](GrantOverhealToHealth.json)
- [Heal an Overheal recipient](HealFive_OverhealRecipient.json)
- [Swap two fighters](SwapPosition.json)
- [Shuffle face-up Plant fighters across lanes](swap-randomly.json)
- [Transform into a fresh copy](transform-copy-fighter.json)
- [Transform and consume the donor](transform-consume-fighter.json)
- [Transform with a subset fallback](transform-copy-or-fallback.json)

## Lane and damage features

See `BuffPlantsInFullLane.json` for the two-Plants-in-a-lane condition, `MoveToAnyLaneOnRight.json` for lane targeting, and `DamageByLaneDistance.json` for a per-target distance multiplier. The `*_Innate.json` and `Grant*.json` files show the damage and healing abilities. The single-query files are meant to be nested inside a target filter, not loaded as complete cards. See [lane and damage reference](../../docs/reference/lane-and-damage.md).
