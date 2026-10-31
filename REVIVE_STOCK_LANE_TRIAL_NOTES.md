# Stock secondary-lane choice experiment (2026-10-28)

The previous REVIVE_AUTO_LANE assignment was removed because the casting lane is not guaranteed legal for the discarded plant. No replacement destination is fabricated.

For a single-option UniqueGuidTargets picker **only when** the exact effect entity belongs to a Burst trick whose **first PlayTrigger** uses PickFromThree + ReturnFromDiscardPileToLane + SecondaryTargetFilter, the UI auto-submit shortcut is bypassed, allowing the ordinary one-card picker submission path to run. Multi-option paths and unrelated tricks/fighters remain unchanged.

This is a diagnostic trial, not a verified end-to-end one-option fix. The engine may itself auto-complete a single target; this patch does not alter its sole-target decision. If the secondary chooser does not run, further instrumentation of stock CreateEffectsForSecondaryTargets / target-choice creation will be needed. Do not assume the card/lane is safe unless stock secondary selection is observed.

Keep kUnearthStripCastingPrimaryTrialEnabled=true for the earlier confirmed primary removal.

Look for REVIVE_STOCK_LANE_TRIAL: SKIP_AUTO_SOLE, UNEARCH_CHOOSE_BUILD, and UNEARCH_EFFECT_HANDOFF. Confirm secondaryLane=1 before accepting as fixed.
