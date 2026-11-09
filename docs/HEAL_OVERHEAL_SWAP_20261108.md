# Heal, OverhealToHealth, and SwapPosition implementation (2026-11-08)

This release builds **on top of** `CustomComponents_MetadataRemoval_full.zip`. It preserves the Level/Leveled, variable, stat-comparison, and add/remove-tag/subtype features. It **does not** require the obsolete `LevelSunPayment` component.

## 1. Heal the actual damage of each primary target

The stock `HealEffectDescriptor` retains its numeric `HealAmount`. A `VariableMultiplier` now supports an optional `Owner` string. `"PrimaryTarget"` reads `Effect.TargetId` **of the particular runtime effect** and evaluates the expression for that recipient; `"Self"` reads the effect source; the previous absent-Owner/`Query` aggregation path is preserved.

Example components on the **same effect entity** (the effect still needs its existing primary-target selection setup):

```json
[
  {"$type":"PvZCards.Engine.Components.HealEffectDescriptor, EngineLib","$data":{"HealAmount":1}},
  {"$type":"PvZCards.Engine.Components.VariableMultiplier, EngineLib","$data":{"Expression":"DamageTaken","Owner":"PrimaryTarget","Divider":1}}
]
```

Each primary target gets a separate amount based on its own damage. `"DamageTaken"` is read from the card's `Health.CurrentDamage` when no user-defined variable of that name is found. The existing native `MultiplierSystem.ProcessEffect(HealEffect)` multiplies `HealAmount` before `HealSystem.ProcessEffect(HealEffect)` clamps it. Do **not** write `"HealAmount":"Full"`; that changes the native field type and is not needed.

When `Owner` is omitted, the previous behavior is retained: the optional `Query` selects owners and `Aggregation` applies the existing `Single`, `Sum`, `Min`, `Max` behavior; without `Query` the source card is read. `Owner: "PrimaryTarget"` requires no `Query` and `Aggregation: "Single"` (or omitted). `"Target"` is *not* an alias for PrimaryTarget because your trigger-context target has different semantics.

## 2. OverhealToHealth (grantable)

Give a card the custom component `PvZCards.Engine.Components.OverhealToHealth` or grant `GrantableAbilityType: "OverhealToHealth"` through the existing custom grant-ability bridge. The marker follows the established SelfTeleport pattern: absent `Innate` means innate=true when deserialized as a direct component; granted cards can instead activate via counters (with `Innate: false`).

Example direct card component:

```json
{"$type":"PvZCards.Engine.Components.OverhealToHealth, EngineLib","$data":{"Innate":true}}
```

The new hook surrounds the stock `HealSystem.ProcessEffect`. It records incoming requested healing **after** the stock multiplier stage, invokes the real stock heal, then compares the target's `Health.CurrentDamage` before and after. An eligible recipient with an active `OverhealToHealth` component receives a **stock permanent BuffEffect** (+0/+excess) enqueued using `EffectCollection.AddAfterCurrentGroup`. No direct modification of `Health.MaxHealth` is used.

Example: target 2/4 with 2 damage taken, heal requested 5 -> stock repairs 2, excess 3 -> native +0/+3 buff (2/7). Healing prevented by `Unhealable` or a cancelled effect should not create excess buffs. The buff uses the original heal's effect source and target IDs, preserving source tracking.

Known caveat: hooking native heal and recording native buffs is designed to preserve history, triggers, stat colors and effects, but those integrations still need to be confirmed **in-game** (including a healing attempt with Unhealable and a heal triggered by a different source).

## 3. SwapPositionEffectDescriptor

Example descriptor on an effect entity:

```json
{"$type":"PvZCards.Engine.Components.SwapPositionEffectDescriptor, EngineLib","$data":{}}
```

Use your existing effect-target selection mechanisms to choose a **primary** board fighter and a **secondary** board fighter. Their runtime `Effect.TargetId` and `Effect.SecondaryTargetId` are interpreted as card IDs in this custom effect. This is distinct from a normal `MoveCardToLanesEffect`, where `SecondaryTargetId` is the destination **lane** ID.

The implementation:

1. Validates two different fighters on the board, with exactly one lane apiece.
2. Captures original lane, team owner, and front/back order for each.
3. Checks basic occupancy/Teamup/Aquatic conditions with both participants excluded from the destination occupancy calculation.
4. Removes both fighters using native `LaneModifier.RemoveCardFromLane`, then places both with native `LaneModifier.PlayCardInLane` to the other fighter's original **lane and order**. Both placements are verified; a failed swap attempts native rollback and emits no move records.
5. Invokes native `MoveCardToLanesSystem.RecordCardMove` **twice** (one for each card), using that card's original lane and destination order. The original runtime effect IDs are restored after writing records.

The native records are expected to drive the stock move-trigger and rendering pipelines. **Exact trigger count and simultaneous animation are not confirmed without an actual PvZ Heroes runtime test.** The two move records may render one after another, depending on the view-side processor. The operation does not intentionally touch cards in hand/deck or manipulate `LaneElement` directly.

If either target cannot be resolved to one on-board fighter, or the destination cannot be occupied, the effect safely rejects the swap.

## 4. Quick validation checklist

- [ ] Android build links, loads, and logs module initialization.
- [ ] One damaged plant: `HealAmount:1 * DamageTaken` restores precisely its damage.
- [ ] Three primary targets with 1/2/3 damage taken: heals 1/2/3 independently, no aggregation.
- [ ] A target with no damage heals zero when using this expression.
- [ ] An active OverhealToHealth target with 2 damage receives heal 5: damage fully repaired, stock +0/+3 applied, health displays the buff color, buff triggers fire.
- [ ] Same heal on an unhealable target: no healing and no new buff.
- [ ] A temporary OverhealToHealth grant stops working when its counter expires.
- [ ] Two fighters in different lanes and front/back slots swap exactly, with each move trigger firing once.
- [ ] Two Teamup partners in the same lane exchange front/back when both are selected.
- [ ] Invalid secondary target or occupied prohibited slot: no partial move.
- [ ] In PvP/simulation replay, move history and view stay synchronized.

### Native signatures checked

From the provided `dump.cs` and `libil2cpp.so`:

- `PvZCards.Core.Systems.MultiplierSystem.ProcessEffect(HealEffect)` RVA `0x21F1630`
- `PvZCards.Core.Systems.HealSystem.ProcessEffect(HealEffect)` RVA `0x2201A98`
- `PvZCards.Core.Systems.BuffSystem.ProcessEffect(BuffEffect)` RVA `0x21FD444`
- `PvZCards.Core.Systems.MoveCardToLanesSystem.RecordCardMove(Effect,int,int)` RVA `0x21FD070`
- `PvZCards.Core.Systems.Utilities.LaneModifier.RemoveCardFromLane(int)`, `PlayCardInLane(LanePlacementData)`
- `PvZCards.Engine.Effects.BuffEffect.set_HealthDelta(int)`, `.set_AttackDelta(int)` and `BuffDuration = Permanent (0)`

Host C++ compile and source/policy tests are useful but not substitutes for native Android or an in-game test.
