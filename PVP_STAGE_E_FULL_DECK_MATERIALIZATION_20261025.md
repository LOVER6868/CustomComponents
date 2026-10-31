# Stage E — Full Stock Deck Materialization (Private Lab)

The Stage D materializer succeeded on pool 6 for two Saucer Squash (GUID 704)
cards, and Stage C succeeded on pool 1 on two phones. This build extends that
same stock `PrepopulateCardPoolSystem.ProcessEffect` call to all four initial
pools: 1 (40), 2 (4), 6 (40), 7 (4). Each call uses a genuine initialized
`PrepopulateCardPoolEffect` with a match-bound factory/system, followed by a
read-only strict pool/entity identity validator. If any call fails, processing
stops immediately; there is **no rollback**.

No picker logic, Relic Hunter ability, card data, asset files, networking,
or server code is changed. Copies legitimately share GUIDs but have distinct
entity IDs. Card 704 has an `InDeckQuery` without any NotSelf rule in the
user-supplied card JSON.

## Test configuration (both matching modified phones, private match ONLY)

```ini
[PvpDeckPrepopulation]
Mode=2
BothModdedClientsLab=true
ConstructStockSystemLab=true
ConstructStockEffectLab=true
ExecuteOneCardLab=false
ExecuteTwoCardLab=false
TwoCardTargetPoolId=1
ExecuteFullDeckLab=true
ExpectedDeckFingerprint=dea298e889fb3a3c
```

This lab has no cryptographic peer negotiation. Never run in public/ranked or
with unmatched builds, config, or card data. Preserve the exact initial plan
fingerprint; a changed deck should cause a skip rather than silent execution.

Expected logs: `FULL_DECK_BEGIN`, for each pool
`FULL_DECK_POOL_BEGIN pool=...` and `FULL_DECK_POOL_RESULT pool=... exact=1`,
then `FULL_DECK_RESULT exact=1 poolsCompleted=4 cards=88 live=88 virtual=0`.
The hash covers pool GUID/order/entity IDs and appended component signatures.
It is NOT a server acknowledgment or full simulation-state equality proof.

After the run, disable `ExecuteFullDeckLab` (or set Mode=1). A new match model
can be tested in the same app process; the duplicate-mutation guard is model
scoped, and each eligible match must still start with 88 untouched entries.

Upload both phone log captures for comparison. Look at Pick3
`DECK_TARGET_POOL`, `POOL_BOUNDARY`, `WEIGHTED_POOL`, and `WEIGHTED_OFFER`
after this build reports `FULL_DECK_RESULT exact=1`.
