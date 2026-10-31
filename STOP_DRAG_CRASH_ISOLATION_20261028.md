# Stop-drag identity probe isolation (2026-10-28)

Phone2 logged two identical native null dereferences after TRICK_PLACE_DIAG STOP_DRAG_ENTER, before STOP_DRAG_EXIT. Both stacks included libsprig.so -> BNM::TryInvoke -> libil2cpp.so.

This diagnostic build comments out ONLY two READ-ONLY GetComponent-based identity logging calls in TrickStopDragTrace: DRAGGED and VALID_TARGET. It leaves stock StopDrag, the generic PickFromThree revive signature, the primary strip hook, and stock secondary filtering unchanged.

This is a hypothesis test, NOT a confirmed crash fix. Test ordinary card placement in PvE first, then PvP. If it crashes again, retain the new tombstone and symbolize libsprig offset 0x2a0954 using the exact build's unstripped libsprig.so. If it does not crash, expand testing before removing other diagnostics.

The uploaded ZIP also contains a second crash at 20:33:34 with the same fault address 0x38 and call stack.
