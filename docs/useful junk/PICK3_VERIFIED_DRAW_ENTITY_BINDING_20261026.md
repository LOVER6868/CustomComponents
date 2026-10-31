# Experimental CardDatabase PickFromThree -> stock draw entity binding

Enable `[Pick3CreationTrace] Enabled=true` in config.ini. The pre-existing trace option installs the finder and DrawCardFromSubsetSystem hooks. This revision additionally binds verified draw outputs to the selected card entry. Other adapters (lane placement and transform) remain read-only.

Provenance criteria:
1. An actual `PickedFromThreeQuery.Matches()` returning true while the stock `CardFromSubsetFinder.FindCard()` is executing supplies the originating selection effect ID, and the finder result agrees with the observed GUID.
2. The same managed consuming-effect pointer later enters `DrawCardFromSubsetSystem.CreateCard()` and returns a positive live entity ID.
3. The selected entry still exists, refers to the same originating effect and GUID, and has no conflicting binding.

There is NO arbitrary GUID-to-live-entity fallback in `PickedFromThreeEntityQuery`. `PickedFromThreeGuidQuery` is unchanged. Only the latest selection participates in the pre-existing cross-effect live query. A later selection replaces the same source's earlier binding.

Expected log messages: `boundary=FINDER ... sourceEffect=28 verifiedQuery=1`, `boundary=DRAW_CREATE ... sourceEffect=28 chosenGuid=698 bound=1`. If `verifiedQuery=0` or `bound=0`, no entity binding occurs, and the new query fails closed. A `PICK3_TRIGGER queued=1` is dispatch return, not proof of listener execution or cost modification.

Safety: stock finder and draw run unchanged; no card duplication, no direct modifier, no trigger replay. Existing PvP selection prediction/trigger policy remains unchanged; this opt-in observer should be tested in PvE first, not as proof of PvP correctness. Entity source context across independent game models requires separate validation; the registry is not yet a replicated PvP entity-binding protocol.

Host regression: `clang++ -std=c++20 -O2 -Iinc tests/Pick3VerifiedBindingRegression.cpp -o /tmp/pick3_binding_test && /tmp/pick3_binding_test`.
