# Thrash

## Status

**Gameplay implementation verified working.**

```text
Custom GrantableAbilityType ID: 21
Managed class: PvZCards.Engine.Components.Thrash
Base class: PvZCards.Engine.Components.GrantableAbility
Storage: inherited GrantableAbility.Counters
Gameplay system: ThrashSystem
Semantics: non-numeric flag ability
```

Thrash uses the shared GrantAbility serialization, Add/Set/remove, duration, history, and component-construction infrastructure. Its gameplay behavior is isolated in `ThrashSystem`.

## Core gameplay rule

Excess damage dealt by a source with active Thrash to a **fighter** is dealt to the hero opposing the **source card**.

The source determines the destination. The damaged fighter's owner does not.

This is intentional and enables self-damage/self-sacrifice designs:

```text
my Thrash source
    ↓ damages/overkills
my own fighter
    ↓ overflow
opponent of the Thrash source takes Thrash damage
```

The generated hero damage retains the actual Thrash card as `Effect.SourceId`.

## Single-source overflow

For a non-Team-Up hit:

```text
overflow = max(0, resolvedDamage - targetHealthBefore)
```

Only positive overflow creates a Thrash hero-damage effect.

Example:

```text
5 Thrash damage → fighter with 3 health
overflow = 2
source-owned Thrash DamageEffect → opposing hero for 2
```

A zero allocation must create **no DamageEffect**. A zero-damage effect can still enter stock processing such as the block-meter pipeline, so zero effects are deliberately suppressed rather than routed.

## Team-Up: one shared overflow pool

Team-Up does not calculate an independent overflow for each attacker. The complete contribution group shares one pool:

```text
totalOverflow = max(0, sum(all contributions) - targetHealthBefore)
```

Non-Thrash contributors count toward total damage/overflow but cannot own a Thrash allocation.

Thrash allocation is finalized only after the complete Team-Up contribution group has resolved. Allocation order is **back-to-front**.

For each Thrash source in reverse order:

```text
allocation = min(sourceContribution, remainingOverflow)
remainingOverflow -= allocation
```

Only `allocation > 0` produces a routed effect.

### Verified/intended examples

```text
5 Thrash front + 5 Thrash back → fighter with 2 health
combined damage = 10
overflow = 8

back owns 5
front owns 3

emit back-source DamageEffect for 5
emit front-source DamageEffect for 3
```

```text
5 Thrash front + 5 Thrash back → fighter with 7 health
combined damage = 10
overflow = 3

back owns 3
front owns 0

emit back-source DamageEffect for 3
front creates NO DamageEffect
```

This prevents multiple Thrash cards from collectively dealing more hero damage than the real overflow.

## Stock effect routing and block meter

The first real Thrash implementation applied hero health damage too far downstream. Health changed, but the stock block-meter pipeline did not see the Thrash shot.

The final architecture creates a real pooled `DamageEffect` and routes it through the stock `EffectRouter`.

Each positive Thrash allocation therefore preserves:

```text
SourceId      = the actual Thrash card
TargetId      = hero opposing that source
PlayerId      = source owner/player
LaneId        = original lane
EffectGroupId = originating effect group
DamageAmount  = that source's Thrash allocation
```

This allows normal source-sensitive damage systems to participate instead of manually reproducing them inside Thrash.

## Team-Up block-meter behavior

Normal Team-Up combat remains untouched and keeps the game's stock behavior: the combined combat hit counts as one normal block-meter charge.

Thrash shots are additional, separately routed damage effects. Therefore two positive Thrash allocations may independently interact with the block meter.

This is important for source-specific abilities such as Bullseye:

```text
regular Thrash + regular Thrash
    → each positive Thrash shot can charge meter

regular Thrash + Bullseye Thrash
    → regular source's Thrash shot can charge meter
    → Bullseye source's Thrash shot does not

Bullseye Thrash + Bullseye Thrash
    → neither Thrash shot charges meter
```

The implementation does not alter how the original Team-Up combat hit itself charges the meter.

## Managed representation

`components/Thrash.hpp` intentionally has no custom amount field.

Thrash follows stock flag-style `GrantableAbility` representation:

```text
GrantableAbility.Counters
        ↓
GrantableAbilityRuntime::Active
        ↓
ThrashSystem::HasThrash
```

Counter `Value` does not define a Thrash amount. Active state is the relevant property.

## Investigation history worth preserving

Several early hook boundaries caused game hangs and should not be retried casually:

- `DamageSystem::ProcessEffect`
- early direct/Dobby `FighterDamageEffectCreator::CreateEffectsForAttackingInLane` experiments
- guessed value-type ABI around `DamageListBuilder::CreateAndAddDamageEffect(DamageData, bool)`

Later work established a working resolved-damage path and a real routed-effect solution. The important architectural lesson is not to manually emulate downstream damage side effects: preserve source identity and feed synthetic damage through the normal `EffectRouter` whenever stock damage semantics are desired.

## Completion record

Verified outcomes from the final development pass:

- custom Thrash component loads,
- GrantAbility Add/Set/remove/duration lifecycle works,
- Thrash active state is counter-backed,
- ordinary fighter overkill damages the opposing hero,
- Team-Up shares one overflow pool,
- Team-Up allocation is back-to-front,
- multiple Thrash sources cannot exceed actual overflow,
- zero allocations produce no synthetic damage effect,
- each positive Thrash shot retains its own source card,
- routed Thrash damage participates in stock block-meter processing,
- source-specific Bullseye behavior is preserved by source ownership,
- destination hero is resolved relative to the Thrash source rather than the victim,
- self-damage/self-sacrifice combinations therefore remain possible.
