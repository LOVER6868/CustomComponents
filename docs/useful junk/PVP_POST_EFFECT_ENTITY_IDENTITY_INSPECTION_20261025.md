# PvP one-card post-effect entity inspection (October 25, 2026)

Baseline: CustomComponents_PvP_Staged_OneCard_Lab_FULL_20261025.zip.
Only the PvP diagnostics header is modified; gameplay paths, PvE, and picker remain untouched.

## What it actually does

The existing **opt-in Stage B still calls stock ProcessEffect exactly once**.
That call MUTATES the local match model. The **new inspection code itself is
read-only** and runs after ProcessEffect returns without an exception, and only
when the resulting model has `Entities.Count=18` and `NextEntityId=18`.
No new card/factory calls, no writes, and no entity creation occur in the
inspector. This is not a peer synchronization handshake.

The inspector logs:

* The GUID/entity ID of any deck-pool entry associated with entity 16 or 17;
* A stable fingerprint of the four post-call pools, including GUID and entity ID
  for all 88 entries;
* Each of entities 16 and 17: its internal ID, list index and component count;
* Each component's namespace and type, and known fields `Card.Guid`,
  `Owner.OwnerId`, `DeckAssociation.DeckEntityId`, `HandAssociation.HandEntityId`,
  `HandElement.HandEntityId`, `CreationSource.SourceGuid`, and `EffectEntity.SourceId`;
* Two per-entity hashes and a combined logical-structure hash.

Fingerprints deliberately exclude addresses, timestamps, device settings and
pointer/GC handles. They compare the recorded semantic fields and component
ordering, **not all hidden component state**. Matching hashes are useful but
not a complete network determinism or state-equivalence proof.

## Recommended build/test sequence

First: keep the safe defaults (Mode=1, all lab flags false) during ordinary use.

When testing, install identical full builds on TWO *consenting*, modded clients,
with matching card data. Test only in a controlled PRIVATE match. Have the
same `ExpectedDeckFingerprint` value previously verified on both phones:

```ini
[PvpDeckPrepopulation]
Mode=2
BothModdedClientsLab=true
ConstructStockSystemLab=true
ConstructStockEffectLab=true
ExecuteOneCardLab=true
ExpectedDeckFingerprint=dea298e889fb3a3c
```

The four lab switches and initial fingerprint gate the single mutating call.
No separate inspection switch is needed; the new code runs automatically
AFTER that call. Stop the experiment if either phone crashes or reports
`ONE_CARD_EXCEPTION`, or the inspector reports `complete=0`.

Search logs for `PVP_PREPOP: ONE_CARD_RESULT`, `PVP_PREPOP: ENTITY_INSPECT
POOL_LINK`, `PVP_PREPOP: ENTITY_INSPECT COMPONENT`, and `PVP_PREPOP:
ENTITY_INSPECT SUMMARY`. Compare the **same match** logs from each device.

After a single controlled match, disable Stage B again:

```ini
ExecuteOneCardLab=false
```

If you want only non-mutating startup inspection, run Mode=1 instead. This
snapshot cannot examine the *new* entities without the existing Stage B
operation, because entity 16 and 17 do not exist in the untouched base model.

## Expected reference only (not validated on Android yet)

Prior observations on both phones: one live entry in pool 1, 87 remaining
virtual entries, `firstId=16`, `nextId=18`, total 18 entities. We DO NOT
assume that entity 17 is a particular component type. The new logs are meant
to discover this fact. Retain `exact=0` in `ONE_CARD_RESULT` until we have
validated what the two-entity stock side effect means.

## Validation / limitations

This is source-level work, not a tested APK. C++ syntax check and existing
regression tests can be run on host. Verify runtime logs on both phones.
The protocol still does not negotiate with remote peers. NEVER use these
mutation flags for public/ranked PvP or unknown/unmodified opponents.
