# Shared deck entry: PvP execution validation boundary

Added `queries/DeckEntryExecutionBridge.hpp` with a read-only capture and execution-model validation API:

- `DeckExecution::Capture(snapshot, physicalEntry)` records exact owner, pool ordinal, index, GUID, entity ID, and a deterministic per-owner pool fingerprint.
- `DeckExecution::ValidateForExecution(executingSnapshot, request, executingOwner)` revalidates on the actual executing model, rejecting a wrong owner, any deck-state changes, ambiguous entity IDs, and invalid selections.
- Valid existing entities return `ReadyEntity` and the genuine entity ID.
- Virtual entries return `RequiresMaterialization`, **never** an invented entity ID.
- `Queries::ValidateDeckActionOnModel()` provides the model-scoped adapter in `InDeckQuery.hpp`.
- Added `tests/DeckEntryExecutionBridgeRegression.cpp` for owner isolation, virtual/live entries, virtual-to-live changes, removed entries, duplicates and mismatched pools.

**This is not a finished multiplayer targeting or mutation implementation.** The stock `Play.Targets` values are entity IDs; the game does not expose an established network payload for a pool ordinal + index. No serializer or `PlayServiceProcessor` hook has been installed, and no virtual cards are created or removed. A complete synchronized virtual-card action requires validated server/client wire encoding, handling model divergence/reordering, and stock materialization within the executing simulation. Do not reuse an ordinary `SelectTarget` slot to smuggle a pool index.

Tested with `clang++ -std=c++20` native host regression and `InDeckQuery.hpp` syntax checks. Android and online match behavior have not been tested.
