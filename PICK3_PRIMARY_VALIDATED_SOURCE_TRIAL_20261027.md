# Pick3 primary validated-source trial (712/713)

This experiment retains the existing deferred initial-zone play, validator and drag correction. The only new candidate acquisition behavior is scoped to UniqueGuidTargets (mode 4) for a blocking effect whose SourceId resolves to GUID 712 or 713.

During the actual LocalPlayCardService validation of the initial play, it copies the original effect validator's primary target IDs into a native vector. Later, FindValidTargetsFromEffect still runs normally and returns its unmodified stock list. For the local PickFromThree offer only, the bridge tries the copied primary IDs, verifies each still resolves to an Entity with a Card component and positive GUID, and falls back to the existing stock-derived options if no cached candidates survive. It does not modify stock targeting, service selection or secondary lane behavior.

Look for PRIMARY_SOURCE phase=CAPTURE followed by PRIMARY_SOURCE effect=... cached=... verified=... . Check the existing WEIGHTED_OFFER and STEP2 logs; a nonempty offered set does NOT guarantee the subsequent SelectTarget service accepts the choice or the revive succeeds.

LIMITATIONS: The cached primary IDs are not separately re-evaluated for discard zone/ownership in this trial; they were obtained from the original service validator's eligible set, and are rechecked only for entity and Card component at picker time. Cache is matched by source card entity ID, not a unique completed-effect transaction. No compilation/device testing performed. Revert both modified headers to roll back.
