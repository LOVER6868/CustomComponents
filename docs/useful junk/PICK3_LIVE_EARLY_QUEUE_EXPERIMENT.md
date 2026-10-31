# Experimental regular PickFromThree early queue insertion

This patch extends the existing passive TriggeredEffectQueuingSystem.ProcessEffect hook rather than installing an EffectCollectionProcessor hook (which previously conflicted with startup).

For **local single-target live** modes 3 and 4 only, the existing verified effect/group/entity/GUID capture arms a scoped callback for the exact producer's TriggeredEffectCollection. At the first matching stock queue-system ProcessEffect call within the selection transaction, the callback attempts `EmitSelectedTrigger` before the original queue method, retaining original source identity and setting target to the verified live entity. The callback is attempted at most once, and the FinishTransaction fallback remains if it does not queue.

CardDatabase mode 5 retains its established stock CreateCard early callback unchanged, and still only supports GUID/subset-query compatible effect use. Multi-target live selections preserve the existing behavior. The trick board-drop targeting exception is **not** part of this patch.

This is an experimental timing change, not proven safe for all effects: stock queuing callbacks may occur before the selection has finished every downstream operation, and `FireInModel`'s completion boolean does not guarantee one listener was accepted. Test first in PvE. Examine PICK3_LIVE_EARLY, PICK3_LIVE_CONTEXT, PICK3_VALIDATE, PICK3_LISTENERS, PICK3_QUEUE_GATE and resulting effect behavior. Revert if startup, event ordering, cost payment, or zone changes become inconsistent. No Android NDK build or runtime validation was performed here.
