# Step 2A — Dynamic initial decks and created-in-deck tracking

## Implementation

This build is based on the most recently validated configuration/trace cleanup build.

### PvP initialization (automatic deck-specific preflight)

- `PvpDeckPrepopulation.ExpectedDeckFingerprint=auto` opts into dynamic initial-deck authorization.
- Retains strict **16 initial entities**, `NextEntityId=16`, four initial pools **1/2/6/7**, cardinalities **40/4/40/4**, every entry `EntityId=-1`, valid positive GUID, initial hash and immediate pre-mutation recheck.
- In automatic mode, requires the **match-bound CardDatabase** to return a backing entity with exactly one correct `Card.Guid` for each distinct GUID. A missing definition skips all stock writes. This is important for custom-card decks and must be verified on actual Android builds. This does not check peer mod compatibility.
- Original four stock `PrepopulateCardPoolSystem.ProcessEffect` calls (pool 1,2,6,7) and complete post-call card/owner/effect validation are untouched. `ExpectedDeckFingerprint=<16 hex digits>` remains an immediate rollback for the original deck-only laboratory mode.
- A fingerprint is a deterministic **local** check, not a signature of the opponent's installed card definitions; private-server peer compatibility remains an external operational requirement.

### PvE and newly inserted virtual deck entries

- The existing PvE `DeckInitializer.InitializeDeck` post-hook continues to materialize arbitrary selected deck entries (no fixed fingerprint).
- The existing `DiscardPileZoneEffects` hook on `CreateCardInDeckSystem.ProcessEffect` remains the **sole hook owner**. On the stock path, it snapshots virtual entries before forwarding the original method and invokes the `DeckMaterializer` observer afterward. This observes effects such as **Sow Magic Beans** without double-hooking the method. Custom return-from-discard routing is left unchanged.
- Only on an **identical live PvE EntityModel with the original rooted CardEntityFactory** does it call the existing `RepairSameModelDeckEntries`, which materializes all currently virtual entries in the regular deck with stock `CreateCardEntityWithDefaultOwner` (no manual entity IDs).
- On PvP/prediction models it does **not** allocate or change deck entries. Instead it logs `INSERT_VIRTUAL` with counts; making runtime PvP insertion materialization deterministic on both peers remains an explicit later task.
- Actual `ShuffleCardPoolSystem` was NOT hooked or modified. A shuffle reorders entries; a `CreateCardInDeck` effect inserts new GUID entries. Mixing those mechanisms is incorrect.

## Startup hook conflict resolved (2026-10-26)

A previous Step 2A test build installed `CreateCardInDeckSystem.ProcessEffect` twice:
first in `DeckMaterializer`, then in `DiscardPileZoneEffects`. The second BNM hook failed,
which stopped startup before `InitializeInDeckQuery()` and caused
`TypeNotFoundException: PvZCards.Engine.Queries.InDeckQuery` during card-data deserialization.
This hotfix removes the duplicate hook and forwards the insertion observer through the
existing owner. No gameplay mutation should occur during module installation.

## On-device acceptance tests

1. Existing exact test decks, PvP on both phones: all 88 cards materialize; Pick3 main-deck-only offers; compare `FULL_DECK_RESULT` hashes.
2. Change ONE card in either player main deck and rerun both phones: automatic mode must log `DECK_DEFINITIONS_OK`, then `DECK_AUTHORIZATION mode=auto`, then `FULL_DECK_RESULT exact=1` with new matching hashes. A missing definition must log `dynamic-definition-validation-failed writes=0`.
3. For PvE play Sow Magic Beans, and confirm the new entries transition to physical only in its live model. Verify stock shuffle/draw and end-of-match behavior.
4. For PvP play Sow Magic Beans: expect `INSERT_VIRTUAL` showing new entries but do **not** expect them to be physical yet. Observe without changing gameplay selection rules.
5. Repeat a match within the same process. Confirm no materialization on stale models and normal startup on a new match.

No Android execution was run during packaging. If dynamic CardDatabase lookup rejects a registered custom GUID, capture `DECK_DEF_MISSING` and do NOT disable preflight or force mutation; restore the prior pinned build while investigating the correct match-local card catalog API.
