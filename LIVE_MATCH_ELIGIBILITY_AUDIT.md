# Live PickFromThreeUnique eligibility investigation

The weighted sampler operates on physical entity IDs returned by the stock TargetEntityFinder. It does not remove deck entries. The remaining live-match pool shrink is not explained by a sampler unit test; do not add invented copies to the menu.

Added `LIVE_DECK_AUDIT` emitted on each stock UniqueGuidTargets query: `physicalEntries`, `liveEntries`, `staleEntries`, `missingIds`, `duplicateIds`, `stockEligible`, `poolMatched` on the same simulation EntityModel. This is read-only and enabled by default for the diagnostic build (not through a gameplay switch).

Interpretation: a large `staleEntries` or `missingIds` count suggests invalid deck materialization / nonpersistent copies; `physicalEntries > stockEligible` alone does not prove a defect since player and target JSON filters may exclude cards. The scanner includes both players' pool entries and cannot substitute for owner-scoped eligibility. Compare consecutive activations of the same effect and record draws/actual deck count.

No pool or target-list modifications were made, to avoid manufacturing targets and PvP divergence. This is a diagnostics release, not a confirmed live repair. Prior PvP DB5, hand highlight, button and avatar safety logic remains unchanged.
