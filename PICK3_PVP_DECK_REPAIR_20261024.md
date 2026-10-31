# PickFromThreeUnique PvP exact-model deck repair

The previous target bridge blocked same-model materialization for all remote PvP sessions. The target bridge now checks whether the finder uses the exact EntityModel registered with its own stock CardEntityFactory during InitializeDeck, and allows the existing guarded materializer on that registered model even in PvP.

- Exact factory/model pointer identity is mandatory; cloned/unregistered prediction simulations are skipped.
- Existing CardPoolEntry positive IDs are validated as before; stale positive IDs are logged, not overwritten.
- The existing snapshot iteration, recursion guard and matching GameEntity GUID checks remain in place.
- No changes to the sampler, preview timing, art, PvP GUID transport or handoff.

New logs: LIVE_DECK_REPAIRED includes remote=1 when cards are created in an exactly registered PvP model; LIVE_DECK_SKIPPED identifies models without registered factories (in diagnostic builds).

Important: This is not a guaranteed fix for a PvP simulation that uses an unregistered clone, and it has not been device-tested or checked for both clients' authoritative convergence. Compare both clients' logs and validate the deck entity counts before deploying beyond tests.
