# October 25: combined crash rollback, first-pick service binding, cross-model deck receiver

Baseline: Pick3_FirstTap_HolderWake_V2_full_20261024.zip (the version under test).

## First picker
- Replaced the experimental holder-driven `PickFromThreePreviewSystem.hpp` with the last audit version, then added a **UI-tick-only** dependency resolver for the exact case supported by the device log: holder in state 6/7, an existing pending stock finder offer, but neither Local nor Remote service root has been captured.
- The new path calls the preexisting `Submission::PumpDependencies()` (existing injected SelectTargetService lookup). It does not construct a service, execute a command, run finder, send a target or tick Unity UI inside GameInputStateHolder.Update.
- Preserved exact holder identity, model, owner, mode and stock eligibility checks in the existing activation code.
- Disabled `DriveFirstPickerFromHolder` in config and removed the V2 holder callback code entirely. Legacy startup assignment remains harmless.
- New marker: `FIRST_SERVICE_BIND` shows effect/group, binder presence, and whether Local/Remote service binding succeeded; emitted on effect change or resolution change.
- This is a **code-level repair attempt** until Android testing confirms first-open-without-tap. A failed DI resolution will be visible, not masked by an artificial target command.

## Deck
- Added shared, strictly read-only `DeckEntries::ResolveAcrossModels`. It resolves by independently authenticated deck owner, ordered-pool ordinal, entry index, GUID and the recipient's own entity IDs. It rejects duplicate identities, foreign owners, changed cards and ambiguous duplicate entity IDs. Virtual -1 entries remain `VirtualEntry`.
- `DeckWire::ValidateReceiver` now uses this tested resolver instead of a second inline implementation. Tests include sender and receiver with differing live IDs.
- Nothing here fabricates network IDs or materializes virtual cards. Virtual-card PvP **remains disabled**, pending a compatible receiving action plus a stock per-simulation card factory. Do not map GUID or pool index to Play.Targets.

## Preserved
- `GuardedStockButtonRecovery=true`, `SuspendInterruptedPick3=true`; underlying trace and submission logic untouched.
- The log's first blocked picker `50/264` at 08:57:09 has state=6, binderModel=1, exactCache=1, local=0, remote=0. After stock SelectTargetCommand fails at 08:57:19, `IDENTITY: ACTIVATE` follows. The old V2 HOLDER_WAKE ran three times but did not overcome the missing service gate.
- Source log shows process restart between runs but no conclusive PvZ native crash stack; do not attribute fault to a specific instruction.

## Test matrix, one build
1. Fresh first PickFromThree in PvE; do not tap hero or another card. Look for `FIRST_SERVICE_BIND` with `resolved=1` followed by `IDENTITY: ACTIVATE`.
2. Fresh PvP, player locally owns PickFromThree; verify no opponent-owned popup.
3. Relic Hunter environment-triggered picker and interrupted/reentered effect.
4. Surprise/Done button with chained effects.
5. Deck test on both sides: inspect `DECK_TARGET_POOL` and confirm virtual cards still refuse unverified stock submission.

## Verification
- C++20 host syntax check passed with uploaded BNM/Sprig headers (Android log and signal portability shims, no device linker/runtime check).
- Existing DeckEntryExecutionBridge, DeckEntryResolver, Pick3PhysicalCopies, DeckEntryWireIdentity tests passed, including new receiver cross-model assertions.
- ZIP integrity checked. Android deployment not tested.
