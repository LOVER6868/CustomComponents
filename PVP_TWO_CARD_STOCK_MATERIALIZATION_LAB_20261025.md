# PvP stock two-card materialization lab (October 25, 2026)

## Baseline / purpose

Based on `CustomComponents_PvP_Entity_Identity_Inspection_FULL_20261025.zip`.
Both private test phones independently confirmed that one `ProcessEffect` with
`NumCardsToCreate=1` creates card entity 16 (GUID 711) and effect entity 17
(`EffectEntity.SourceId=16`); full prior combined hash `38238e3553383b53`.

Stage C probes **one** stock `PrepopulateCardPoolSystem.ProcessEffect` call
with `NumCardsToCreate=2` and `Effect.TargetId=1` on the untouched match
initialization model. It is **one mutating call**, not two sequential calls,
so it does not bypass the stock factory's own ID/side-effect ordering.

## Safe defaults and flags

Defaults remain `Mode=1`, `BothModdedClientsLab=false`,
`ConstructStockSystemLab=false`, `ConstructStockEffectLab=false`,
`ExecuteOneCardLab=false`, `ExecuteTwoCardLab=false`, and an empty fingerprint.
Existing one-card flag remains backward compatible. Both execution flags TRUE
is an **error** that disables *both* mutation choices. Stage B and Stage C
share the same strict initial 16-entity, 88-virtual-entry fingerprint gate.

For exactly one controlled private test on **two matching modded clients**,
change ONLY this section on both clients to:

```ini
[PvpDeckPrepopulation]
Mode=2
BothModdedClientsLab=true
ConstructStockSystemLab=true
ConstructStockEffectLab=true
ExecuteOneCardLab=false
ExecuteTwoCardLab=true
ExpectedDeckFingerprint=dea298e889fb3a3c
```

Rebuild and install the same module build on both devices. This config does NOT
implement peer negotiation or server simulation validation. Neither stock
ProcessEffect nor a failed partial mutation can be rolled back safely. Stop
using the lab build if either phone crashes or reports an exception; do not
run against unknown, unmodded, ranked, or public opponents.

The runtime enforces at most **one mutation attempt per app process**, even if
a second match is started. Restart the game to retry and restore the OFF
setting immediately after collecting the logs.

## New log keys

- `PVP_PREPOP: TWO_CARD_BEGIN pool=1 count=2 ...`
- `PVP_PREPOP: MATERIALIZE_INSPECT POOL_LINK ...` (two expected)
- `PVP_PREPOP: MATERIALIZE_INSPECT ENTITY_BEGIN ...` for appended IDs 16..N
- `PVP_PREPOP: MATERIALIZE_INSPECT COMPONENT ...` (read-only)
- `PVP_PREPOP: MATERIALIZE_INSPECT ENTITY_END ... kind=card|effect|unknown ...`
- `PVP_PREPOP: MATERIALIZE_INSPECT CARD_REL ...` (source-link counts)
- `PVP_PREPOP: MATERIALIZE_INSPECT SUMMARY ... combinedHash=...`
- `PVP_PREPOP: TWO_CARD_RESULT exact=... firstId=... secondId=...`

The inspector does **not** assume 2 new entities per card. It finds the new
IDs, checks both first deck entries, card GUIDs, owners, effect source IDs,
list indices, component order, all four pool entries, and stable semantic
fingerprints. It bounds reading to at most 40 total entities and fails
closed for unknown/unlinked supporting entities, duplicated/absent card
relationships, card GUID 711 without an effect, or inaccessible fields.
The *inspector* never mutates the model; the preceding stock call does.

A `TWO_CARD_RESULT exact=1` (if achieved) means ONLY that these local checks
passed. Compare **both phones'** `combinedHash`, `poolHash`, all `CARD_REL`
entries, and element GUID/IDs. Matching hashes do not prove all hidden
state, peer ACK, or full match/server synchronization. No outcome of Stage C
is claimed as tested on Android yet.

After collection:
```ini
ExecuteTwoCardLab=false
```

The feature remains experimental, private-lab-only; existing picker, PvE,
transport, and server paths are unchanged.
