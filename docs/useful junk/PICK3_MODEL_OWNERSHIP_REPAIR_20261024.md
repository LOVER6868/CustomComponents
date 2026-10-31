# PickFromThree deck model-ownership repair (2026-10-24)

Changed: `inc/zones/DeckMaterializer.hpp` only.

- Retain separate GC-rooted `(EntityModel, CardEntityFactory)` registrations, up to 12 entries. Registrations are reused for the same model. A different factory for that model replaces only its factory handle. Oldest registrations are released when capacity is reached.
- `RepairSameModelDeckEntries` resolves a factory by *exact* model-object identity, and rejects unregistered prediction/simulation copies. Existing remote-PvP guard in `PickFromThreeTargetBridge.hpp` is unchanged.
- Snapshot the set of player entities with distinct positive deck IDs before creating cards; `CreateCardEntityWithDefaultOwner` adds entities to `EntityModel.Entities`, so mutating that list while iterating it is unsafe. A same-thread reentrancy guard prevents recursive repair.
- Validate positive `CardPoolEntry.EntityId` using `EntityModel.GetEntityById` plus `Entity.Id` and Card.Guid. Invalid positive IDs are logged as STALE_ID and left unchanged to avoid duplicate entities / PvP identity corruption. Entries with nonpositive IDs use the stock factory as before.
- Added MODEL_REGISTER, NO_MODEL_FACTORY, MODEL_REPAIR diagnostics gated by `CUSTOMCOMPONENTS_VERBOSE_DIAGNOSTICS=1`; STALE_ID always logs.

## Test expectations

1. Local match: materializer completion logs show full live deck; repeated triggers should sample the remaining deck (copy counts should decline only for genuine draws/removals).
2. If the target finder uses a simulation copy whose model is not registered, it will still skip creation. That is intentional: this change does not claim to solve the missing-ID problem for copies, or make those copies authoritative.
3. For a STALE_ID warning, retain the log and reproduce with a build that includes symbol information; ID repair was not attempted.
4. This is not a confirmed fix for the unrelated random-time native crash; symbolicate the installed libsprig.so with matching Build ID to identify the crashing C++ frame.

Syntax validation: host clang++ C++20 `-fsyntax-only` with BNM, Dobby, Android-log stub includes and host signal compatibility preinclude. Android deployment and match gameplay not tested.
