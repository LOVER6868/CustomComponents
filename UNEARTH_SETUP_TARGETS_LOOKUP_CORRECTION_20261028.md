# Unearth SetupTargetsComponents hook correction — October 28, 2026

`dump.cs` shows `PvZCards.Core.Systems.PlayOneTimeEffectSystem` (TypeDefIndex 12955), not `PlayOneTimeEffectCardSystem`. The previous diagnostic requested the wrong class, causing both `hook=0` and `modelField=0`.

Corrected the BNM class lookup to `PlayOneTimeEffectSystem`. Confirmed metadata:
- `SetupTargetsComponents(PlayCardEffect)` — private instance void, one argument, RVA `0x220344C`.
- `<EntityModel>k__BackingField` — instance field offset `0x18`.
- `PlayCardEffect.LocationId` — existing field lookup succeeded in earlier build.

The setup log now includes `method=1` if BNM resolves the method. Target snapshots remain read-only. No gameplay flow, hook guards, or deferred trial flags were changed. Android compile/runtime not available in this environment; test in PvE and look for `UNEARCH_SETUP_TARGETS: BEFORE` and `AFTER`.
