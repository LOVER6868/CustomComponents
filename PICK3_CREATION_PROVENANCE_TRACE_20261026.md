# Pick3 CardDatabase creation provenance: read-only tracing

This build adds an opt-in diagnostic observer (disabled by default to preserve the supplied baseline configuration).
All stock methods execute untouched; no entity reference is bound and trigger timing is unchanged.

Events under `CustomComponents/Pick3/CREATION_TRACE`:
- `FINDER`: stock `CardFromSubsetFinder.FindCard(query,effect)` returned a GUID.
- `DRAW_CREATE`: stock `DrawCardFromSubsetSystem.CreateCard(effect)` returned an entity.
- `PLACE_SOURCE`: `CreatedCardPlacer.AddCreatedCardEffect(card,effect)` completed (card creation into lane).
- `TRANSFORM`: stock `TransformIntoCardFromSubsetSystem.TransformCard(card,guid,effectEntityId)` completed.

These are **candidate correlation boundaries**, not proof of provenance. Pointer/effect identity and exact model must be verified before gameplay binding is implemented. A `FINDER` without `DRAW_CREATE` is expected for creation/transform effects. `DRAW_CREATE` without `FINDER` may arise from other stock logic or other overloads. `CREATE_FROM_SUBSET` may call the three-argument `FindCard` overload, which this first, conservative probe does not yet detour.

Selection queries remain unchanged. One-physical-copy auto-selection and PvP deck/shuffle code remain unchanged.

To enable the observers for the next Android capture, add the following to `config.ini` and rebuild:

```ini
[Pick3CreationTrace]
Enabled=true
```

To disable, set `Enabled=false` and rebuild.
