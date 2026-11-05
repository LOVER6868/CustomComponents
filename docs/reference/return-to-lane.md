# ReturnFromDiscardPileToLaneEffectDescriptor

Returns an existing discarded fighter or environment to the lane selected by the
secondary filter. The primary target is the original card entity; it is not a
fresh card or a copy. Ownership and existing modifiers are preserved.

```json
{
  "$type": "PvZCards.Engine.Components.ReturnFromDiscardPileToLaneEffectDescriptor, EngineLib, Version=1.0.0.0, Culture=neutral, PublicKeyToken=null",
  "$data": {}
}
```

Pair this descriptor with a primary filter that selects a card in a discard pile
and a secondary filter that selects a legal lane. Filters still define terrain,
owner, faction, and other card-specific restrictions.

- Fighters use the owner's fighter slots. A second friendly fighter requires a
  plant with Team-Up on either the incoming or resident fighter. Full sides reject.
- Environments use `EnvironmentPlacer.PlaceEnvironmentInLane(int,int)`, the same
  placement method reached by stock created environments. It replaces and discards
  the old environment and updates the environment slot and LaneElement.
- Tricks and malformed cards marked as both fighter and environment reject.
- Successful returns emit `PlayTrigger` and `ReturnedFromDiscardPileTrigger` using
  the executing model and original card target. They do not emit `MoveTrigger`.
- Fighters produce `CreateCardInLaneRecord`; environments produce
  `CreateEnvironmentInLaneRecord`, so the frontend uses the correct animation path.

The fighter path retains its existing face-up behavior. `ForceFaceDown` remains a
serialized compatibility field; this patch does not implement new face-down rules.
No resources are charged and placement is not replayed as a hand play.

Zone membership, metadata, model identity, lane capacity, and view-lifecycle gates
are checked before removal. A stale mapped view causes a skipped return. Native
placement/history failures are logged; this is not a general rollback transaction.

Patch 4 requires device verification, particularly environment replacement and
presentation in PvP. See the [release notes](../../CHANGELOG.md).
