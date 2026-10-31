# DeckEntryResolver foundation (PvE / PvP)

## New API
- `queries/DeckEntryResolver.hpp`: portable read-only `Selection`, `SamePhysicalEntry`, `Validate`, `ForOwner`, and `CountCopies`.
- `Queries::SnapshotDeckEntries(Obj* model)` in `queries/InDeckQuery.hpp`: reads ordered deck entries from exactly the provided EntityModel, preserving owner entity ID, pool ordinal, current pool index, card GUID and EntityId (including -1).

## What is intentionally unchanged
- `InDeckQuery.Matches` still checks exact live entity membership.
- `InDeckGuidQuery.Matches` still checks GUID presence for card definitions.
- No automatic prepopulation, pool mutations, target-enumeration hooks, PvP Play/transport extensions, or network dispatch changes.
- No materializer, sampler, popup, animation, or existing source behavior changed.

## Important limits
The index is valid only for the current ordered-pool state. `Validate` compares against a **fresh** snapshot; even then identical virtual copies can be indistinguishable after a shuffle. For a multiplayer action, both sides must agree on the owner/deck identity and the selected physical entry. This foundation must not be used to send entryIndex in a stock `SelectTarget` Play, nor may it make a virtual entry into an entity without a verified stock game action.

## Tests
`clang++ -std=c++20 -O2 -I../inc DeckEntryResolverRegression.cpp -o deck_resolver_test && ./deck_resolver_test`
Native host header syntax check passed using existing BNM stubs. Android/PvP not exercised.
