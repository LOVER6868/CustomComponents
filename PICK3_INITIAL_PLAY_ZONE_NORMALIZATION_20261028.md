# Initial Play Zone Normalization (GUID 712/713)

Based on BridgeRollback/StockValidationAudit. Does NOT reintroduce StockValidationBridge.

Changed: inc/pick3/PickFromThreeSubmission.hpp only.

The single stock-produced, already-validated candidate Play for 712/713 may target an eligible discarded card instead of the intended initial lane zone. If the entity resolves as a card, the zone resolves as non-card, and identity/type/owner checks pass, normalize that candidate Play target to the recorded zone and remember it for the existing deferred-drop correction. Otherwise do not alter the Play.

Trace: INITIAL_PLAY_BRIDGE phase=NORMALIZED / ALREADY_ZONE / SKIP; existing DEFERRED_DROP, TRICK_SERVICE, PRIMARY_SOURCE, HANDOFF_TRACE are retained.

Gameplay experiment. No C++ compilation or Android verification performed. Test PvE first.
