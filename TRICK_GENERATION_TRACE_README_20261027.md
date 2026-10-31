# Trick valid-play generation observer (read-only)

Baseline: TrickAcceptanceTrace 20261027. Modified: inc/pick3/PickFromThreeSubmission.hpp.

Logs TRICK_GENERATION from GameFacadeDecider.GetValidPlayTargetsForCard and GetValidPlaysForCard, only for real entities with GUID 699, 712, 713 or 27. Reads returned pooled lists during the hook (never retains/releases them). Bounded to 100 interesting generation calls and 16 target IDs / 12 play entries each. Prints play fields from dump.cs: type=0x10, CardId=0x14, PlayerId=0x18, Targets=0x20, Order=0x28. Uses entity model for card identity; non-card IDs show targetGuid=-1. Existing TRICK_ACCEPT diagnostics remain.

This does not modify targeting, validation, play result, secondary targeting, or other card families. Diagnostic compilation / Android runtime unverified. Test GUID 699, 712, 713 and stock GUID 27; compare TRICK_GENERATION TARGET/PLAY_TARGET rows to TRICK_TARGET_IDS and TRICK_ACCEPT results.


## Hook resolution correction (October 27)

`GameFacadeDecider` is a facade-selection interface; it does not implement the three-argument target/plays methods. Both are on `PvZCards.Core.Utilities.GameStateUtils`. The pre-existing `TrickValidTargetsTrace` already hooks `GetValidPlayTargetsForCard`; target-generation logging now shares its returned result, while `TrickGeneratedPlays` hooks `GameStateUtils.GetValidPlaysForCard` directly. No second hook is applied to the same target method. Startup logs both method-lookup and hook installation outcomes. All modifications remain read-only.
