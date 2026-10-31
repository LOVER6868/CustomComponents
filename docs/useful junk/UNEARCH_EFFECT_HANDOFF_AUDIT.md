# Unearth stock effect handoff audit

Scope: instrument the **ReturnFromDiscardPileToLane** descriptor and ProcessEffect execution boundary. This is deliberately read-only: the existing selection bridge and initial-play normalization are preserved. The current device log does not prove whether the runtime effect receives selected primary card 69 and a separately selected lane, nor whether the system actually runs. Until that is observed, changing TargetId could revive the wrong card.

Expected records:
- `UNEARCH_EFFECT_HANDOFF phase=DESCRIPTOR_TO_EFFECT`: descriptor creates runtime.
- `UNEARCH_EFFECT_HANDOFF phase=PROCESS_ENTER`: primary target, secondary lane and exact model component membership.
- Existing `LANE_TARGET_TRACE`, `ReturnFromDiscardPileToLane: preflight`, `VALIDATION_AUDIT`, `STOCK_VALIDATION_BRIDGE` and `HANDOFF_TRACE` remain.

Interpretation: no PROCESS_ENTER after accepted selection means the effect hasn't reached the move-lane system (investigate secondary-target pending queue). If PROCESS_ENTER shows primary=69, primaryDiscard=1 and secondaryLane=1, focus on custom transfer preflight/record routing. If primary=169 or -1, focus on the stock effect target mapping. If secondary=-1, focus on secondary lane selection/resume, not primary card selection.

No native Android compilation or device testing has been performed.
