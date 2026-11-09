# New card mechanics — engine-shaped examples

All `*.json` files in this folder use **full assembly-qualified** `$type` strings,
matching the stock card data. No `note`, `component`, or `components` wrappers are
serialized into a card. These are **card component fragments**, not independent
card definitions. Copy a `EffectEntitiesDescriptor` into your card's
`entity.components`, or merge its `entities` with the existing descriptor.

| File | Insert into | Demonstrates |
| --- | --- | --- |
| `FullHeal_PerPrimaryTarget.json` | `entity.components` | On Play, **all damaged friendly plants** heal by their own `DamageTaken`: `HealAmount=1`, then `VariableMultiplier(Owner=PrimaryTarget)` |
| `OverhealToHealth.json` | **recipient** `entity.components` | Innate `OverhealToHealth`, not an effect descriptor |
| `GrantOverhealToHealth.json` | caster `entity.components` | On Play, choose a friendly plant and grant the ability permanently |
| `HealFive_OverhealRecipient.json` | healer `entity.components` | On Play, choose a friendly plant to heal for 5; overheal converts to +0/+N **only if that plant has the ability** |
| `SwapPosition.json` | trick/caster `entity.components` | On Play, choose two friendly plants on the board; swap their original lane and front/back positions |

## In-game conventions

- `EffectEntityGrouping` → `PlayTrigger` → `TriggerTargetFilter` → `PrimaryTargetFilter` → **effect descriptor** → **multiplier** (when used) → `SecondaryTargetFilter` (when used). This is the order shown in stock card data (including stock move effects' secondary filter **after** their descriptor).
- `Target` in trigger context is **not** synonymous with `PrimaryTarget`. The new multiplier's `Owner: "PrimaryTarget"` evaluates each runtime recipient **separately**, with no `Query`. The full-heal example intentionally uses `SelectionType: "All"`.
- `SwapPositionEffectDescriptor` uses both selected targets as fighter **card IDs**. This differs from stock `MoveCardToLanesEffectDescriptor`, whose secondary target normally selects a **lane**. Do not reuse the stock lane secondary filter in the swap example.
- The swap effect runtime rejects selecting the same fighter twice. Its manual UI may still allow attempting to select the same fighter, because the game's available stock queries do not expose a verified exclude-current-primary query. In-game target selection and animations remain to be tested.
- For the `FullHeal` example, all selected cards are Plants on board, and `DamageTakenComparisonQuery` ensures they have damage to heal. Adapt the faction if this is a zombie card.
- A heal of **exactly** `DamageTaken` has no excess. Test `OverhealToHealth` with the separate `HealFive_OverhealRecipient.json` sample.
- Custom runtime hook behavior (move animations and events, Overheal buff triggers/colors) is **not confirmed** until tested on Android. These JSON files are valid authoring structures, not proof of successful native execution.
