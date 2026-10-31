# Step 2B — PvP Sow Magic Beans owner-evidence hotfix

Source: latest Step 2B Global Play model-scoping hotfix. All unrelated files are identical.

## Observed on phone

- Initial dynamic PvP deck passes: `FULL_DECK_RESULT exact=1`, all 88 physical.
- The first four newly inserted GUID 345 entries are correctly detected on the live model, but skipped with `pool-owner-unavailable`, leaving four virtual entries.
- Subsequent insertions cannot enter the strict pre-stock validation while prior virtual entries remain. The log labels that `pre-stock-snapshot-unavailable`.
- Neither skip calls `PrepopulateCardPoolSystem.CreateCardIfNotAlreadyCreated`; no new PvP card was materialized in the captured test.

## Changes

- Stop assuming that the regular deck-pool entity itself has an `Owner` component.
- Instead validate ALL already-physical cards in that exact destination pool: resolve each card by its existing entity ID and check `Entity.Id`, `Card.Guid`, and `Owner.OwnerId`, requiring every copy to agree on one owner.
- Reject if the deck has no physical cards, if a card cannot be validated, or if owners disagree. Do not invent owners based on pool number, effect owner, or faction.
- Leave the existing rooted-model check, unique-insertion delta, stock helper, GUID validation, post-creation owner check, strict virtual-entry preflight, PvE repair, and Global Play model-scoped trigger hotfix unchanged.
- Keep `MaterializeInsertedCardsPvp=true` only on identical controlled-test clients; to fall back safely, set it false on both.

## Device acceptance

On BOTH phones, a fresh PvP match must produce `FULL_DECK_RESULT exact=1` and `PVP_INSERT_BOUND` before any beans are inserted. First cast of Sow Magic Beans should report `PVP_INSERT_RESULT exact=1 pool=6 guid=345 created=4` (pool and GUID may differ for other cards) with matching `beforeNext`, `nextId`, and `deltaHash` on both phones. A second batch must also succeed and match. Verify PickFromThree, the global PlayTrigger, and PvE insertion behavior. 

If either phone logs `PVP_INSERT_ERROR` or diverges in allocated IDs/hashes, disable `MaterializeInsertedCardsPvp` on both clients, preserve logs, and stop the current experimental match. Any partial native mutation is not rollback-safe.

Host syntax and regression tests do not establish runtime determinism. This build is experimental until confirmed on two devices.
