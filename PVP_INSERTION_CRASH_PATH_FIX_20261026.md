# Step 2B — PvP inserted-card native crash-path fix (2026-10-26)

## Evidence and scope

The exact matching `libsprig.so` Build ID was `a3fa2b6902e366955df1788ac17f153047feafd4`.
Three observed native crashes (00:00:07, 00:06:29, 10:47:43) followed the same
`CreateCardInDeckSystem` -> shared observer -> `MaterializePvpInsertions`
-> `ExistingPvpDeckOwner` -> `BNM::TryInvoke` / `EntityModel.GetEntityById`
chain. The null+0x38 fault happened inside the IL2CPP lookup. The evidence
identifies the call path, **not** the reason that IL2CPP's receiver was null.

## Source fix

`inc/zones/DeckMaterializer.hpp` now resolves entities through the **already
available `EntityModel.Entities` managed list** using a read-only, ID-verified
scan (`FindPvpModelEntityWithoutInvoke`), not an IL2CPP method call. The scan:

- refuses missing, null, invalid, and duplicate IDs;
- never allocates game entities or calls `GetEntityById`;
- validates the same existing-card Guid, EntityId, and Owner fields;
- is used by PvP pool lookup, existing-deck owner consensus, and stock-created
  card result verification (all three optional PvP insertion lookup sites).

`EntityModel.GetEntityById` remains in the **unchanged existing PvE deck
initialization path**, outside the crash-affected PvP insertion code.

No changes were made to stock `ShuffleCardPoolSystem`, Lima-Pleurodon / Magic
Beanstalk card data, the 88-card PvP prepopulation algorithm, the existing
shared hook owner, the PvE insertion flow, or the Global PlayTrigger hotfix.

## Staged validation / rollback

The packaged configuration deliberately remains:

```ini
[PvpDeckPrepopulation]
MaterializeInsertedCardsPvp=false
```

This is a **crash-isolation build**, not proof that PvP creation is safe to
turn on. With `false`, the game uses stock random-position insertion and
shuffle; generated PvP deck entries may remain virtual, and stock-only
physical-card targeting may not select them. The optional new lookup code
is not exercised by default.

Test a fresh PvP match on **both identical clients** with `false`, including
Lima-Pleurodon's triggered Magic Beanstalk and Sow Magic Beans. If and only
if that is stable, a controlled follow-up may try `true` on *both* phones,
comparing `PVP_INSERT_RESULT`, `beforeNext`, `nextId`, and `deltaHash` from
both clients and their prediction/remote effects. A successful local
`exact=1` is **not** evidence that the other phone is synchronized. End the
match on any divergence. Do not switch modes mid-match.

## Validation performed

- Complete `main.cpp` C++20 host syntax checks in default and verbose builds.
- 8 C++ regressions, including `PvpInsertionSafeLookupRegression.cpp`.
- All 8 Python regressions, including `PvpInsertionCrashPathRegression.py`.
- No Android NDK APK build, no device gameplay test performed for this patch.

Fixes the demonstrated unsafe lookup call path, but **does not yet establish
cross-client determinism of optional physical insertion**. The latter remains
a separate acceptance requirement.
