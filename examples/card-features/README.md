# Base stats, stat queries and card metadata

These are **individual snippets**, intended to be inserted into the game card's
normal JSON structure rather than treated as standalone cards:

- `AddTagEffectDescriptor`, `RemoveTagEffectDescriptor`, `AddSubtypeEffectDescriptor`, and `RemoveSubtypeEffectDescriptor` go inside an **effect entity's** `components` array, after its trigger and primary-target filter. An effect can modify only the fighter it actually selects.
- `StatComparisonQuery`, `StatModifierQuery`, and `VariableComparisonQuery` go into a `Query` field on a corresponding filter or requirement.
- `SetVariableEffectDescriptor` requires the surrounding effect to execute and a `Variables` component defining the referenced variable, where appropriate.
- Subtypes are **numeric IDs** in `Subtypes.$data.subtypes`; tags are strings in `Tags.$data.tags`. The demonstrated subtype ID `4` is stock **Mushroom**; change it to the ID you want to add/remove.

All class strings include `EngineLib, Version=1.0.0.0, Culture=neutral, PublicKeyToken=null`, including nested `ComponentType` references.
