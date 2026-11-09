# Game-code investigation: overhealing, full healing, swapping positions

**Status (2026-11-08):** The currently attached sources contain CustomComponents C++/notes and stock card JSON examples, but **not the actual IL2CPP `dump.cs`, `script.json`, EngineLib.dll, or decompiled native method bodies**. Therefore exact stock HealSystem and swap/move method signatures and state-transition semantics have not been validated. These three systems are NOT installed by this patch.

## 1. OverhealToHealth

**Behavioral invariant:** A card with max health 4 and current damage 2, receiving requested heal 5, consumes 2 heal and should apply a stock +0/+3 permanent `BuffEffect` (end max health 7). Do not directly increment `MaxHealth.BaseValue`, reduce damage below zero, or fake a buff history event.

**Source evidence already in this repo:**
- `inc/zones/detail/ReturnEffectQueueCompletion.inl` has `BuffHandlerHook` around the original `PvZCards.Core.Systems.BuffSystem.ProcessEffect` and publishes `Engine::Events::afterBuff`.
- `inc/presentation/detail/OffBoardEffectAudit.inl` lists the BuffSystem method with expected RVA `0x21FD444` for the previously investigated build; any updated APK needs RVA revalidation.
- `.local/notes/examples/unsorted/11_global_card_was_discarded_listener.json` has the real stock `BuffEffectDescriptor` fields `AttackAmount`, `HealthAmount`, `BuffDuration`.
- `inc/abilities/SelfTeleport.hpp` and `inc/abilities/GrantableAbilityRuntime.hpp` show how to register a grantable ability and evaluate active `GrantableAbility` counters. `OverhealToHealth` should follow this pattern, with its own ability ID and managed type.

**Still need from game code:** actual heal effect conversion, heal `ProcessEffect` entry/exit, effect data fields for requested/applied amounts and source/target, how the engine queues stock `BuffEffect`, and the normal buff trigger/history/visual path. Convert only positive `max(requested - actualHealed, 0)`; do not generate recursive healing. Trigger timing and expiration should follow stock effect queue.

## 2. `HealAmount: "Full"`

Stock card data uses `PvZCards.Engine.Components.HealEffectDescriptor` with a numeric `HealAmount`; direct substitution of the string `"Full"` cannot be assumed to deserialize to an integer. For user-facing syntax, `HealAmount: "Full"` requires an explicit deserialization translation **before** stock integer deserialization, or a custom descriptor with a separate correctly typed field and a bridge to stock HealEffect.

Resolve the actual amount **when the heal executes** using target `Health.CurrentDamage` and effective healing limits. That makes full heal exact even when damage changes between selection and execution; use stock healing code to emit normal heal triggers/records. Overheal conversion should be based on requested heal minus actually restored HP: `Full` itself should not produce extra health, since requested amount is capped by damage taken.

**Still need:** HealEffectDescriptor `ToEffect(...)` signature, `HealEffect` effective fields, HealSystem `ProcessEffect` signature, serializer branch parsing descriptor `$data` and `HealAmount`, and whether heal amount is transformed by other factors.

## 3. `SwapPositionEffectDescriptor`

The repository already contains hooks for `PvZCards.Core.Systems.MoveCardToLanesSystem.ProcessEffect` and uses `PvZCards.Core.Systems.Utilities.LaneModifier`, `PvZCards.Engine.Components.Lane`, `LaneElement`, and `MoveCardToLanesEffect` in `inc/zones/DiscardPileTransferSystem.hpp` and `inc/engine/BoardPlacement.hpp`. These implement transfers into lanes, NOT a verified two-card swap.

**Required invariant:** read and validate both target entity ids, two distinct live board fighters; snapshot each (lane id, player side, front/back/stack index), exchange both placements transactionally, emit/queue native moves/triggers for **both** fighters and render two simultaneous animations (or non-flickering batch). An unmodified trick has no hand-source reconciliation when its targets are already on board. Guard teamup/water/ownership/legal occupancy where appropriate; do not make sequential `RemoveCardFromLane` + `AddCard` calls and assume atomicity.

**Still need:** exact MoveCardToLanes/MoveEffect fields, `LaneModifier` APIs for swapping card order, record/history creation for two moves, `MoveTrigger` resolution (which event records cause it), and move animation processor/view APIs. A diagnostic hook/log on stock move operations in-game can confirm the sequence on two test fighters, before implementation.

## 4. Newly added removals

`inc/effects/CardMetadataEffects.hpp` now registers `RemoveTagEffectDescriptor` and `RemoveSubtypeEffectDescriptor`, in addition to the add counterparts, through the existing `AbilityConversion` and `EffectGuards` pipeline. They call managed `List.RemoveAt` in reverse index order, remove all exact matches, and do not create a missing metadata component. This mutates the card's current tags/subtypes; it does not edit permanent backing card database data. If stock renderers cache subtype labels/tags, a separate refresh may be needed.

## Best next inputs for a game-code-level implementation

1. The same game's IL2CPP `dump.cs` and `script.json` (method signatures and RVAs).
2. If available, the `GameAssembly`/`libil2cpp.so` disassembly around Heal/Move/Effects/serializer functions, or decompiled managed stub bodies from the build. A `dump.cs` alone provides signatures, not method implementations.
3. One Android logcat trace from a stock heal and a stock move, plus a stock BuffEffect causing a visible stat buff.

With these, implement the handlers on verified methods and run PvE then PvP tests, keeping the hooks centralized in existing owners.
