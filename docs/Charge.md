# Charge

## Identity

```text
Custom GrantableAbilityType ID: 20
Managed class: PvZCards.Engine.Components.Charge
Base class: PvZCards.Engine.Components.GrantableAbility
Storage: inherited GrantableAbility.Counters
Gameplay system: ChargeSystem
```

## Managed representation

`components/Charge.hpp` intentionally declares no custom numeric fields.

Earlier experiments used separate amount/state fields, but the final architecture uses
the stock `GrantableAbility.Counters` lifecycle directly. This keeps serialization,
granting, removal and duration aligned with stock grantable abilities.

## Amount semantics

Charge is numeric in gameplay even though its storage kind is `Grantable`.

The effective amount is:

```text
sum of GrantableAbility.Counters[*].Value
```

`GrantableAbilityRuntime::SumValues` performs this calculation.

Therefore GrantAbility Add/Set/remove can continue to operate on stock counters and the
Charge gameplay system only needs to consume the resulting amount.

## Proven gameplay hooks

Charge's working integration uses BNM `BasicHook` at:

```text
PvZCards.Core.Systems.HeroBlockingSystem
    ProcessBlockableDamage(DamageEffect, Entity)

PvZCards.Core.Systems.Utilities.HeroBlockMeterModifier
    GetChargeAmount(Entity)
```

These are currently important **known-good** gameplay hook boundaries for this project.

`ProcessBlockableDamageHook` scopes an `activeCharge` / `activeChargeAmount` override
around the stock call. `GetChargeAmountHook` returns that override while the blockable
damage is being processed and otherwise forwards to stock.

## Data resolved around the hook

Charge resolves:

```text
Effect.SourceId
Effect.TargetId
DamageEffect.LaneId
DamageEffect.get_DamageAmount()
HeroBlockingSystem.<EntityModel>k__BackingField
EntityModel.GetEntityById(int)
Entity.HasComponent(Type)
Entity.GetComponent(Type)
Owner.OwnerId
Lane.GetFightersForPlayer(ownerId)
```

The current source also contains a concise `CustomComponents/Charge/DamageTrace` log in
`ProcessBlockableDamageHook`, reporting source, target, lane, damage and target entity.
This is being used as a safe observation point for Thrash research.

## Team-Up behavior

`TryGetTeamUpChargeAmount`:

1. resolves the attacking source entity,
2. requires the attacking fighter itself to have active Charge,
3. starts with that source's Charge amount,
4. resolves the source owner and lane,
5. enumerates allied fighters in that lane,
6. selects the **highest active Charge amount** among them.

The chosen amount is exposed to the stock block-meter calculation only for the scoped
blockable-damage call.

## Grant/remove/duration

Charge uses the shared GrantAbility pipeline. It does not implement separate duration
storage.

## Presentation

Charge currently has no stock `SpecialAbilityType` mapping in `AbilityDefinitions`.
Gameplay behavior does not depend on a custom presentation hook in `ChargeSystem`.

## Lessons preserved

- Do not reintroduce `ChargeAmount`, `GrantedChargeAmount`, `GrantedOnly`, or another
  parallel state table unless a new engine requirement proves it necessary.
- Stock `GrantableAbility.Counters` is sufficient for lifecycle and numeric amount.
- The HeroBlockingSystem/BlockMeterModifier hooks are proven stable and should be
  considered before adding new low-level combat hooks.
