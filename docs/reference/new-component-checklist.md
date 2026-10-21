# New component completion checklist

Use this for every new custom component/ability.

## 1. Identity

- [ ] Ability ID chosen and registered.
- [ ] Serialized name documented.
- [ ] Managed namespace/class/base class documented.
- [ ] Numeric vs flag semantics decided.
- [ ] Storage kind (`Numeric` vs `Grantable`) justified.

## 2. State

- [ ] Reuse stock component/state where possible.
- [ ] For GrantableAbility-derived components, prefer inherited `Counters`.
- [ ] Avoid parallel fields unless the engine demonstrably requires them.
- [ ] Add/Set/remove semantics documented.

## 3. Serialization and construction

- [ ] Custom type registered in `CustomComponentRegistry` if needed.
- [ ] Serialized type resolves.
- [ ] Generic constructor/lifecycle validated.
- [ ] Card-data example recorded.

## 4. GrantAbility lifecycle

- [ ] Grant works.
- [ ] Set works.
- [ ] Add works.
- [ ] Explicit remove works.
- [ ] Permanent duration works.
- [ ] End-of-turn expiry works if applicable.
- [ ] Next-fighter expiry works if applicable.
- [ ] AbilityGrantedRecord behavior verified.
- [ ] AbilityRemovedRecord behavior verified.

## 5. Presentation

- [ ] Decide whether stock `SpecialAbilityType` can be reused.
- [ ] Icon/frame state verified.
- [ ] Grant animation verified.
- [ ] Removal animation verified.
- [ ] Any intentional lack of presentation documented.

## 6. Gameplay system

- [ ] Required engine data listed.
- [ ] Hook boundary documented.
- [ ] Hook signature verified.
- [ ] Existing known-good hook considered before adding a new one.
- [ ] Team-Up behavior documented.
- [ ] Non-combat behavior documented if relevant.
- [ ] Edge cases documented.

## 7. Investigation hygiene

- [ ] New engine findings added to `ENGINE_FINDINGS.md`.
- [ ] Failed/hanging hook boundaries recorded.
- [ ] Build-specific RVAs marked as build-specific.
- [ ] Temporary trace logs identified for later cleanup.
- [ ] Experimental hook failure is non-fatal where practical.

## 8. Custom effect lifecycle (when applicable)

- [ ] Descriptor managed type documented.
- [ ] Runtime Effect base class documented (`NonBlockingEffect`, `BlockingEffect`, etc.).
- [ ] `ToEffect` acquisition/copy behavior documented.
- [ ] `InstanceManager.Acquire` succeeds for the custom runtime type.
- [ ] EffectRouter handler registration verified.
- [ ] DeepCopyFrom/HandleReturnToPool behavior verified.
- [ ] Zero/no-op effects are suppressed when stock processing could still have side effects.
- [ ] Source/target/effect-group identity is preserved when stock interactions depend on it.

## 9. Verification

Record concrete tests, not just “works”:

```text
test:
setup:
expected:
observed:
result:
```

Include at least:

- [ ] card starts with ability,
- [ ] ability granted permanently,
- [ ] ability granted temporarily,
- [ ] ability removed,
- [ ] Team-Up case if relevant,
- [ ] PvE/PvP considerations if relevant,
- [ ] presentation refresh,
- [ ] save/load or reconstruction if relevant.

## 10. Finish line

A component is “finished” only when:

- [ ] implementation is stable,
- [ ] noisy discovery logs are removed or intentionally retained,
- [ ] component doc describes final architecture,
- [ ] investigation history preserves important dead ends,
- [ ] README/status table is updated.

## 11. Query / ordered-zone additions

For a custom `EntityQuery` or ordered-zone mechanic:

- [ ] Decide whether a concrete stock query can provide the lifecycle base.
- [ ] Provide a real public parameterless `.ctor` if stock `InstanceManager` may create the type.
- [ ] Verify whether the engine calls the method through class virtual dispatch or interface dispatch.
- [ ] If BNM creates the method but interface dispatch still reaches the parent, inspect and repair the specific interface vtable slot rather than adding a global hook.
- [ ] Keep zone membership separate from ordering/sorting.
- [ ] Use real entity IDs when stock modifiers expect `EntityId`.
- [ ] Remove diagnostic translation layers after identity equality is proven.
- [ ] Keep persistent-zone and ownership invariants documented.

For deck mechanics specifically:

- [ ] `OrderedCardPool` remains the canonical order.
- [ ] materialized `CardPoolEntry.EntityId` values survive mulligan virtualization.
- [ ] stock faction ownership is preserved.
- [ ] `DiscardFromDeck.TargetId` is validated as present in the resolved deck.
