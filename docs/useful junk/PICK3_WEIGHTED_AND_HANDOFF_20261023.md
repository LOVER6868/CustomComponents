# PickFromThree weighted stack and handoff safety update

Changed:
- inc/pick3/PickFromThreeCore.hpp: explicitly group distinct eligible live entities by card GUID, weight each GUID by number of copies, sample stacks without replacement, and then select one live representative of the chosen GUID. Three distinct GUIDs are emitted when at least three distinct eligible GUID stacks exist.
- inc/pick3/PickFromThreeTargetBridge.hpp: log `WEIGHTED_POOL`, `SHORT_POOL` and `WEIGHTED_OFFER` to separate pool shortage from UI/render bugs.
- inc/pick3/PickFromThreeSubmission.hpp: do not replace the active service/model with an unrelated stock SelectTargetCommand while our modal picker is capturing input.

Validation: pure C++ weighted sampler test passed 10,000 repeated 3-of-10 samples, uniqueness assertion, and two-stack edge case. Android/BNM integration has NOT been tested on a device.

Important limitations: The previous sampler (entity ticket drawing and GUID-group removal) already had mathematically equivalent weights. This rewrite clarifies and tests the requested stack semantics but does not create a third GUID when the source exposes fewer than three. The stock PvP/PvE command lifecycle is not rebuilt here; input locks/highlight issues are not claimed fixed. `WEIGHTED_POOL` logs will establish whether the source list itself has fewer than three GUIDs. Existing PvE notification and PvP stock timer preserved.
