# Custom effect architecture investigation

## Goal

Build first-class custom card effects using the same broad architecture as stock PvZ Heroes effects instead of implementing every new mechanic as an unrelated hook.

This document records what has been established from `dump.cs` and native `libil2cpp.so` investigation before implementation begins.

## Stock architecture

The engine separates serialized/card-data descriptors from runtime effects and from gameplay behavior:

```text
card data
    ↓
EffectDescriptor (component)
    ↓ ToEffect(InstanceManager, effectEntity)
runtime Effect
    ↓
EffectRouter
    ↓
registered EffectSystem/handler(s)
```

This separation is important:

- descriptor = serialized configuration,
- runtime effect = pooled message/data object,
- effect system/handler = gameplay behavior,
- router = type-based dispatcher.

## EffectDescriptor

`PvZCards.Engine.Components.EffectDescriptor` is an interface extending the component/pooling/copy lifecycle and exposes:

```text
Effect ToEffect(InstanceManager instances, Entity effectEntity)
```

Simple stock descriptors include:

```text
DamageEffectDescriptor            DamageAmount
HealEffectDescriptor              HealAmount
DrawCardEffectDescriptor          DrawAmount
ChargeBlockMeterEffectDescriptor  ChargeAmount
AttackInLaneEffectDescriptor      DamageAmount
```

Marker descriptors also exist with no custom payload, proving that a new effect does not inherently need a numeric value.

## Runtime Effect base

Runtime effects inherit from `PvZCards.Engine.Effect`. Common inherited context includes fields/properties such as:

```text
Data
Context
PlayerId
SourceId
TargetId
SpringboardId
EffectEntityId
SecondaryTargetId
AdditionalTargetSourceId
EffectGroupId
SortingIndex
RequiresTargetValidation
```

The base participates in `Resolvable`, `Poolable`, and `DeepCopyable` lifecycle.

Important runtime base variants include:

```text
Effect
├── NonBlockingEffect
├── BlockingEffect
└── UnresolvableBlockingEffect
```

For the first custom-effect experiment, `NonBlockingEffect` is the simplest candidate.

## Runtime effects are mostly data

Examples from the dump:

```text
ChargeBlockMeterEffect : NonBlockingEffect
    ChargeAmount

HealEffect : NonBlockingEffect
    HealAmount

DamageEffect : NonBlockingEffect
    LaneId
    ShowParticleEffect
    WillKillHero
    Tags
    DamageAmount
```

The gameplay implementation normally does not live in these classes.

## EffectSystem and EffectRouter

`EffectSystem` has the registration entry point:

```text
RegisterEffectHandlers(EffectRouter router)
```

Stock systems then expose typed processing methods such as:

```text
ChargeBlockMeterSystem.ProcessEffect(ChargeBlockMeterEffect)
DrawCardSystem.ProcessEffect(DrawCardEffect)
```

`EffectRouter` supports type-based registration, including a non-generic form useful to native/BNM integration:

```text
RegisterForEffect(Type effectType, Action<Effect> handler)
```

Multiple systems may consume the same runtime effect. Damage is the important example: validation, armor, blocking, health damage, deadly, immunity, history/presentation and other behavior can participate around the same routed `DamageEffect`.

This explains the final Thrash result: routing a correctly sourced real `DamageEffect` allowed stock block/Bullseye behavior to occur naturally.

## Effect queue

The engine also has an `EffectQueue` / `EffectCollection` layer with operations including:

```text
AddGroup
AddAfterCurrentGroup
AddToCurrentGroup
InterruptQueue
Peek
Shift
HasNext
```

`EffectCollectionProcessor` owns/uses both `InstanceManager` and `EffectRouter` while processing collections. Long term, custom effects should participate in this normal lifecycle where appropriate instead of bypassing it.

## Native ToEffect pattern confirmed

Native inspection of simple stock descriptors confirms a very small conversion pattern.

Conceptually:

```cpp
RuntimeEffect* Descriptor::ToEffect(
    InstanceManager* instances,
    Entity* effectEntity)
{
    auto* effect = (RuntimeEffect*)instances->Acquire(
        typeof(RuntimeEffect),
        true
    );

    effect->Payload = this->Payload;
    return effect;
}
```

This pattern was confirmed for:

```text
ChargeBlockMeterEffectDescriptor → ChargeBlockMeterEffect.ChargeAmount
HealEffectDescriptor             → HealEffect.HealAmount
DamageEffectDescriptor           → DamageEffect.DamageAmount
```

For these simple descriptors, `effectEntity` is not needed by the conversion itself.

## InstanceManager findings

`InstanceManager` owns a mapping conceptually equivalent to:

```text
Dictionary<Type, Pool<Poolable>>
```

Important methods include:

```text
Acquire(Type type, bool shouldInject = true)
GetPoolForType(Type type)
CreateInstance(Type type)
Return(Poolable instance)
```

Native investigation indicates `Acquire(Type,bool)` obtains the pool for the supplied managed `System.Type` and acquires an instance from it. `GetPoolForType` lazily creates a pool when that type has not been seen before.

This is a key custom-effect finding: the runtime type does not appear to require a hard-coded stock effect registration merely to obtain a pool.

The practical requirements for a BNM-created runtime effect are therefore expected to be:

1. expose a legitimate managed `System.Type`,
2. be dynamically constructible,
3. satisfy `Poolable` through inheritance,
4. have a usable constructor/lifecycle,
5. tolerate the normal injection path when acquired with `shouldInject=true`.

Inheriting from stock `NonBlockingEffect` should provide most of the required base contracts rather than reimplementing them.

## Planned first proof

Do **not** begin with damage or another gameplay-sensitive mechanic. First prove the framework with a harmless effect:

```text
SprigTestEffectDescriptor
    Amount
        ↓ ToEffect()
SprigTestEffect : NonBlockingEffect
    Amount
        ↓ EffectRouter
Sprig handler
    log Amount only
```

Success criteria:

```text
[ ] card data deserializes the custom descriptor
[ ] descriptor is a real managed custom class
[ ] ToEffect is called
[ ] InstanceManager.Acquire(custom System.Type, true) succeeds
[ ] runtime effect participates in pooling
[ ] Amount copies descriptor → runtime effect
[ ] EffectRouter recognizes the custom runtime type
[ ] registered handler receives it
[ ] effect resolves normally
[ ] return-to-pool/deep-copy lifecycle is safe
```

Only after this proof should the framework be generalized into a `CustomEffectRegistry` or used for gameplay effects.

## Proposed module direction

Conceptually:

```text
CustomComponents/
├── components/
│   └── effects/
│       └── TestEffectDescriptor.hpp
├── effects/
│   └── TestEffect.hpp
├── systems/
│   └── CustomEffectSystem.hpp
└── core/
    ├── CustomComponentRegistry.hpp
    └── CustomEffectRegistry.hpp
```

The exact file layout is not committed yet. The architectural requirement is to keep descriptor construction, runtime effect identity, routing/handler registration, and gameplay behavior separate.

## Additional lifecycle lesson from custom query work

The `InDeckQuery` investigation produced a reusable `InstanceManager` lesson for future custom runtime effects.

A BNM-created managed type may be allocatable through `BNM::Class::CreateNewInstance()` yet still fail when stock pooling creates it. Stock `InstanceManager` ultimately uses an Activator-style factory for a fresh pooled instance.

Therefore a custom pooled runtime type should expose a real public parameterless `.ctor` on its managed type.

Observed failure before this was fixed for `InDeckQuery`:

```text
MissingMethodException:
Default constructor not found for type PvZCards.Engine.Queries.InDeckQuery
```

For future custom runtime effects, treat a legitimate default constructor as a concrete requirement, not just an expectation.

## Current custom-effect boundary

`DiscardFromDeckEffectDescriptor` proves that a custom JSON-visible descriptor can bridge into an existing stock runtime effect/system while preserving stock behavior.

This is **not yet** the planned proof of a completely new custom runtime `Effect` type. The first-class custom runtime-effect experiment described above is still useful and should remain isolated from gameplay-sensitive effects.
