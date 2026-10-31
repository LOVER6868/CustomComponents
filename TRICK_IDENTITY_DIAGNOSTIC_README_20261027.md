# PickFromThree drag entity identity diagnostic — 2026-10-27

This is a **read-only extension** to the working TrickTargetResolver baseline. Gameplay, card targeting, the stock drag resolver, and early PickFromThree trigger scheduling remain unchanged.

New marker: `CustomComponents/Pick3/TRICK_IDENTITY`.

- `phase=DRAGGED`: dragged live entity's ID and, if it has a Card component, its GUID.
- `phase=VALID_TARGET`: identity of each of up to 12 `DragModel.Targetids` at `StopCardDragCommand.Execute` entry.
- `phase=OVERLAP_RESOLVED`: identity of the stock `GetTopDraggedEntityId` result.
- `SETUP`: whether read-only field bindings were found.

`found=1` means the entity exists in the currently injected match `EntityModel.Entities`. `cardGuid=-1` means the entity was missing, had no `Card` component, or the read failed; it does not identify the zone. The stock overlap finder input is still treated as opaque IEnumerable, not a List. Up to 650 identity entries are logged. No `GetEntityById(int)` call is attempted.

**Testing:** Reproduce a refused board-area release with the problematic GUID 699 trick. The new `phase=DRAGGED` entry will establish whether that GUID was actually dragged. Compare its `VALID_TARGET` and `OVERLAP_RESOLVED` entries with one successful stock targeted trick. A GUID of 699 in the log is not a guarantee that its effect mode can be introspected; primary selection-mode attribution still needs a separate descriptor inspection.

Android compilation/gameplay are unverified; validate on a backup PvE build.
