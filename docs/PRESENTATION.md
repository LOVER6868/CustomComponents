# Card presentation

## Inspection and stats

Discard inspection carries a snapshot of the exact physical card's current stats.
Other inspections leave stock stat values alone. Herald identity can be retained
without applying discard-specific stat reconciliation.

Mapped frames are reconciled after their animation/history work catches up.
Reconciliation assigns current values instead of replaying buff records onto a
created card.

## Discard heralds

Custom hand/deck discards receive the `discarded` origin from CustomComponents.
This works without HeraldModifiers. When that optional module is present, its
summary can follow the origin, for example `discarded — Gained …`. Both modules
cooperate through shared presentation boundaries.

## Stat icons

Configure registered-ability icons in `config.ini`:

```ini
[StatIcons]
Enabled=true
Folder=/sdcard/Android/data/com.ea.gp.pvzheroes/files
# ability, Attack/Health, unused SpecialAbilityType, PNG filename, scale
Icon1=Thrash, Attack, 6, thrash-frame.png, 1.0
```

Place the PNG under `<Folder>/sprig/sprites/`. Supply your own image and reserve
an unused special-ability ID. Avoid mapping abilities to the same slot.

`FrameIconRegistry` is a C++ extension point for other icon providers. It is not
a card JSON component. See the [configuration example](../examples/configuration/README.md).
