# GrantAbility architecture

## Scope

The GrantAbility subsystem provides one generic lifecycle for registered custom
`GrantableAbilityType` values. Ability-specific gameplay logic should not be added here.

## Registry dispatch

`AbilityDefinitions.hpp` currently registers:

```text
Armor  = 19
Charge = 20
Thrash = 21
```

`AbilityRegistry` tells the generic pipeline whether an ability uses:

- `Numeric` storage — currently Armor, or
- `Grantable` storage — currently Charge and Thrash.

Unregistered ability IDs are forwarded to stock game processing.

## Grant

`GrantAbilitySystem::ProcessEffect` is hooked through BNM.

For a registered custom ability the custom path:

```text
GrantAbilityEffect
    ↓
AbilityRegistry lookup
    ↓
resolve target Entity
    ↓
NumericAbility::Apply
        OR
GrantableAbilityRuntime::Apply
    ↓
publish AbilityGrantedRecord
```

Supported duration values in the current implementation are `0`, `1`, and `2`.

`GrantableAbilityRuntime::Apply` uses stock `GrantableAbility.AddCounter`. For Set, the
existing source/duration counter is removed and the desired total is converted into the
counter delta before adding.

## Removal and expiry

Explicit custom removal goes through the hooked `RemoveGrantAbilitySystem::ProcessEffect`.

Duration expiry is integrated at:

```text
EndOfTurnAbilityRemovalSystem::ProcessEffect      → duration 1
NextFighterAbilityRemovalSystem::AttemptEffectRemoval → duration 2
```

The module iterates registered abilities and removes the matching counters/modifiers.

When an active custom ability becomes inactive, the module publishes an
`AbilityRemovedRecord` shaped like stock history, including target ID and
`GrantableAbilityType`.

## History is part of presentation

A key finding from Armor/GrantAbility work is that applying gameplay state alone is not
enough for stock-style visual feedback. The custom grant path recreates
`AbilityGrantedRecord`; the removal path recreates `AbilityRemovedRecord`.

Those records feed the game's normal history/presentation processors.

## Serialization

`inc/core/EnumSerializationSupport.hpp` owns the module's single narrow hook of
`DefaultDeserializer.GetValue(Type, object)`. After `AbilityDefinitions` populates the
ability registry, `main.cpp` calls `GrantAbilitySerialization::RegisterAbilityEnums()` to
register the current `AbilityRegistry` name/ID pairs. `TargetScopeSupport::Initialize()`
independently registers `TargetScopeSortValue.ListPosition = 3`; `main.cpp` then installs
**one** shared deserialization hook before the existing `GrantAbility::Initialize()` call.
`GrantAbilitySerialization` continues to own `ModifyOperation` metadata, descriptor copy
and pool handling, and its existing delegated `SetObjectProperties` callback.

For a registered custom enum name, the shared hook substitutes its numeric value as a
string and calls the stock converter again. Stock therefore still performs the final enum
conversion and boxing. Unregistered types and names are delegated unchanged.

The generic registry handles both feature registrations without adding a second hook
on the serializer boundary. The legacy `systems/EnumSerializationSupport.hpp` include
path was a compatibility shim and is removed from the cleaned source; use the canonical header.

## Presentation

`GrantAbilityPresentation` hooks:

```text
SpecialAbilityChecker::AddSpecialAbilities
CardViewIconUtils::GetSpriteTypeFromGrantableAbilityType
```

Presentation is descriptor-driven. An ability receives a stock special-ability visual
only when its `AbilityDescriptor.specialAbilityType` is set.

Armor uses stock Armor presentation (`SpecialAbilityType` 8). Charge and Thrash
currently do not request a stock presentation mapping.

## Flag abilities

Stock flag-style abilities are represented through `GrantableAbility.Counters`; a
counter value may be zero and activation is determined by the grantable ability's active
state rather than by inventing a parallel numeric field.

Thrash follows this model.

## Numeric grantable abilities

Charge also uses `GrantableAbility.Counters`, but interprets the sum of counter values as
its gameplay amount. This lets Add/Set/remove/duration behavior stay in the same generic
GrantAbility pipeline while Charge's gameplay system consumes the resulting amount.
