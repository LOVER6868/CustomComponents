# Step 2B — Match-bound PvP newly inserted card materialization

## Why this exists
The Oct 25/26 device capture verified the original 88-card PvP stock
prepopulation and dynamic deck fingerprints, but **Sow Magic Beans** inserted
four virtual CardPoolEntries (EntityId=-1) at a time. PvE already repaired
newly inserted cards using its bound `CardEntityFactory`; PvP deliberately
observed these insertions without changing them.

## New test behavior
The **existing** `DiscardPileZoneEffects` stock `CreateCardInDeckSystem.ProcessEffect`
hook remains the **sole** hook owner. It captures the two *regular* deck pools
(IDs 1 and 6) before the genuine stock call, then compares them afterward.
The custom return-from-discard sentinel path is unchanged.

PvP writes require all of the following:

1. `[PvpDeckPrepopulation] MaterializeInsertedCardsPvp=true` with its original
   Mode=2 / lab flags / full-deck execution enabled (in both clients).
2. The initial match's 88-card run must finish with `FULL_DECK_RESULT exact=1`
   **before** binding a rooted model and the very same constructed stock
   `PrepopulateCardPoolSystem` used by initial materialization.
3. The original effect must add exactly `AmountToCreate` new, virtual entries
   of its stated `CardGuid` to **one** regular deck. Every pre-existing entry
   must still be present with its original GUID and entity ID.
4. The model and stock factory must match the rooted PvP match model, with
   intact CardDatabase metadata and a valid deck owner. Prediction copies
   or old models do not qualify.
5. Only then call the engine's private stock
   `PrepopulateCardPoolSystem.CreateCardIfNotAlreadyCreated(entry, elementType)`
   for each new entry in final deck order. Never rerun `ProcessEffect` across
   the whole deck. Check each created entity's GUID/owner/identity.

If a check fails before mutation, leave the stock insert virtual and log
`PVP_INSERT_SKIP` with `writes=0`. If the stock creation throws or a post-write
invariant fails, log `PVP_INSERT_ERROR partialStatePossible=1`, and do not retry
in that match. This is not a rollback-capable operation.

PvE uses the existing bound-factory repair logic without any new hook. Newly
created cards do not get extra shuffle operations. The existing `InDeckQuery`
main-deck-only filter and PickFromThree submission pipeline remain untouched.

## Required test sequence (two identical private-server clients)

1. Build/install this same ZIP on **both** devices. Start a normal match and
   verify `pvpInsertStockHelperReady=1`, `PVP_INSERT_BOUND`, and
   `FULL_DECK_RESULT exact=1` on **each** device.
2. Cast Sow Magic Beans once; confirm `PVP_INSERT_RESULT exact=1 created=4`
   on both phones, with matching `pool`, `guid`, `beforeNext`, `nextId`, and
   `deltaHash`. There should be no remaining `EntityId=-1` entries from this
   insertion in the chosen pool.
3. Repeat once. Confirm both phones allocate the next four physical cards
   in the same order and produce the same per-batch `deltaHash`.
4. Exercise drawing / PickFromThree and one full match. Check no new
   `PVP_INSERT_SKIP`, `PVP_INSERT_ERROR`, `HOOK FAILED`, or deserialization
   errors appear. Also test PvE Sow Magic Beans separately to guard against
   regressions.
5. If either client differs, **stop using the new insertion mode in PvP** and
   set `MaterializeInsertedCardsPvp=false` on both clients; retain the known-good
   initial deck materialization and read-only detection. Avoid continuing a
   match with divergent live entity IDs.

No peer handshake is added; both-client compatibility still depends on identical
private-server builds/card data. Host tests cannot prove runtime determinism.

## Notes
- New option defaults `false` in code, is `true` only in the provided test
  config; disable it to return to Step 2A behavior (observer + PvE repair).
- Missing optional stock helper metadata disables PvP insertion without
  disabling initial 88-card materialization.
- Result hash includes pool ID, generated GUID, amount, previous NextEntityId,
  insertion positions, assigned card IDs, and final NextEntityId.
- This implementation intentionally does not process unrelated creation paths
  or retroactively materialize old virtual cards.
