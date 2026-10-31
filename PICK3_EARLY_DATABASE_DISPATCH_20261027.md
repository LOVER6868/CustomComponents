# Experimental CardDatabase early trigger dispatch

The verified stock DrawCardFromSubsetSystem.CreateCard return hook now offers a narrowly scoped early PickFromThreeTrigger emission. It requires an armed LOCAL CardDatabase choice, matching source-effect ID, exactly one verified bound entity, and the captured immutable effect context. Regular PickFromThree is unchanged; remote predicted selections are not dispatched early.

If an early emission queues an accepted listener, FinishTransaction does not requeue it. If no listener is queued, FinishTransaction retains the existing late fallback. The previous startup-safe rollback of EffectCollectionProcessor.ProcessEffects observer is preserved.

WARNING: this is a gameplay experiment. Because CreateCard returns during stock effect processing, the selected entity might not yet have finished all downstream hand-placement/notification steps. No Android NDK compilation or device verification has been performed. Test in an isolated battle, inspect PICK3_EARLY and PICK3_QUEUE_GATE, and revert if effects fire too soon, cards misbehave or startup fails.
