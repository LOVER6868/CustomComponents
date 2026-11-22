# Discard boundary and combat release candidate

The supplied November 19 capture shows hand-discard records dispatched at the
correct stock cursor but starting 8–11 seconds later. The custom wrapper was queued
normally while later stock presentation could overtake it. `StockHistoryMerge.inl`
now stops consumption at a required custom boundary, leaves the next stock cursor
untouched and resumes after the custom visual finishes. It mirrors the verified
native processed/predicted/pause/cursor loop. It never returns a null placeholder
record or rewinds history. The same mechanism applies to every required custom
visual, including swap. Scene cancellation releases the boundary.

Withering snapshots its eligible cards before end-of-turn expiry and requests
ordinary discard/destroy effects in the native current-group response list. It no
longer puts those requests in a new group behind turn advancement. The shared
discard record/presentation system remains the visual owner.

The secondary fighter path replaces its stale stock candidate list with the
validated authored result before allowing stock singleton completion. Primaries
without any legal partner are excluded for swap as well as combat. Terrain and
occupancy override options participate in that same policy.

After before-combat abilities, casualties drain through native discard, including
PreDiscard rescue handling. Two surviving fighters exchange generated attacks;
one survivor receives a native ExtraAttackEffect; zero survivors advance to the
after-combat stages. Native continuation effects retain managed roots until the
queue owns them. Missing original fighters no longer prevent resolving the model's
InstanceManager. This change does not identify particular cards by GUID.

The active configuration is a short release profile with tracing off. The previous
development profile is retained in `diagnostics/previous-development-config.ini`.
Private-server custom deck setup now has one `[Multiplayer]` feature switch; staged
one/two-card trials are not exposed by the release config. Existing native backend
checks remain. Both clients still need matching module and card data.

Live stat dirtiness is atomic; AI grant previews do not request live frame refresh.
The AI selector avoids diagnostic metadata work when profiling is off. The capture
still shows native AI scorer/candidate spikes, so no claim is made that all lag is
eliminated. Compare identical-device release and tracing captures before changing
AI scoring semantics.

Android verification is required: ordinary and Withering discard before next turn,
singleton swap for both primary/secondary queries, rescue/death during before-combat,
single/zero/two survivors, team-up lanes, scene cancellation, PvP and replay UI.
