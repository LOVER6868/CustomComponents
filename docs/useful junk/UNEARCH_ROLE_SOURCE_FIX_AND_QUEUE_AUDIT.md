# Unearth role/source fix and complete queue audit (2026-10-28)

Based on the RoleHandoff source, NOT the stalling SequentialPrimary experiment.

## Changes
- Extend `TargetBridge::ScopedValidationRecipient` with `sourceCard` and populate it from the blocking effect SourceId, already validated as GUID 712 or 713. `owner` remains the player ID.
- Use `recipient->sourceCard` for `FindDeferredInitialZone` in `UnearthSecondaryContextAudit`. Previously this function incorrectly used the player ID and could never find the recorded casting lane. Existing marker, selected-card, and original target guards remain.
- Expand passive `UNEARCH_SECONDARY_QUEUE` effect enumeration from the first 5 to up to 64 entries, enough to cover the 13 entries observed. Does not mutate or reorder the queue.

## Caveat
This corrects a demonstrable bug and improves queue visibility, but it is NOT a validated two-stage sequencing fix. The secondary chooser may still overlap the PickFromThree menu. Do not enable the previous `PRIMARY_FIRST` reroute. Test one discarded card first and capture UNEARCH_ROLE_FIX and UNEARCH_SECONDARY_QUEUE lines.
