# Unearth sequential targeting trial (2026-10-28)

Observed for GUID 712: stock CreateEffectsForPreselectedTargets receives the artificial casting lane (163) and immediately creates a secondary target effect, before the discarded-card picker is resolved. After selection, the runtime move effect carries primary=163 secondary=91.

Trial: in the existing PrimaryTargetEffectCreator preselected hook, for scoped GUID 712/713 initial effect creation only (no active submission validation), call stock CreateEffectsForPrimaryTargets instead. That method should create the primary picker before SecondaryTargetEffectCreator runs. No effect queue entries, selected card, lane, or runtime Effect target IDs are manually rewritten by this change. Keep previous role-fix guards as fallback (not expected to activate if stock sequencing works).

Diagnostic: UNEARCH_SEQUENCE: PRIMARY_FIRST / PRIMARY_FIRST_EXIT. Verify that stock gameplay opens PickFromThree first, then presents lane selection, and that ReturnFromDiscardPileToLane PROCESS_ENTER receives primary discarded plant and secondary destination lane.

This remains an experimental native hook. The constructor and game behavior have not been tested. If no PRIMARY_FIRST appears, the scoped condition was not met. If startup or play crashes, revert this trial to the preceding RoleHandoff source.
