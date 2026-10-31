# Pick3 live blocking effect preflight (experimental)

Source: user-provided CustomComponents and PvZ Heroes dump.cs.

`ServiceFailure::NoBlockingEffect` is enum value 16 in the game assembly metadata. The prior implementation validated only the popup-open candidate snapshot, not the local service's current EffectCollection front at dispatch time. LocalSelectTargetService.Process() then reported NoBlockingEffect in Fisherman tests.

The submission now reads the local service's injected EffectCollection and the EffectQueue.Peek() front, checks its EffectEntityId and EffectGroupId, verifies BlockingEffect.IsResolved()==false, and rejects a mismatched or absent current effect *before* any Play DTO is dispatched. No forced completion, queue modification, or automatic retry is made. PvP continues through the unchanged remote service path.

This is deliberately fail-closed: if the queue cannot be verified, PvE submission will not proceed. This check may need adaptation for engines that expose a non-EffectQueue collection or a different front-of-queue contract. Neither compiled Android output nor gameplay tests were available here; do not treat this as a fix for PvP remote lifecycle, gameplay input lock, or every custom card. Logs now include LIVE_EFFECT messages explaining rejected local submissions.
