# Custom Trigger / FireTrigger examples

New JSON-facing pieces:

- `DiscardOutsidePlayTrigger`
  - global engine/mod event for successful Deck -> Discard and Hand -> Discard.
  - does not fire for play/death cleanup.

- `CustomTrigger { "TriggerName": "X" }`
  - global named listener.

- `FireCustomTriggerEffectDescriptor { "TriggerName": "X" }`
  - broadcasts named custom event X to all matching `CustomTrigger` listeners.
  - those listeners still use stock trigger-condition validation.

- `FireTriggerEffectDescriptor`
  - targeted/card-by-card trigger activator.
  - the effect's primary target is the card whose trigger abilities are activated.
  - stock marker triggers (for example `PlayTrigger`) and targeted `CustomTrigger`
    listeners are both supported automatically.

`FireTriggerEffectDescriptor` fields:

```json
{
  "TriggerName": "PlayTrigger",
  "IgnoreTriggerConditions": false
}
```

`IgnoreTriggerConditions = false`
- run the selected card's matching listener through the normal stock
  `GenericEffectEntityFinder<T>::WillTrigger(...)` validation.

`IgnoreTriggerConditions = true`
- primary-target/source matching is authoritative.
- skip listener trigger-condition/filter eligibility.
- still create ordinary `TriggerData` and use the normal stock triggered-effect queue.

Important:
- `FireTriggerEffectDescriptor` is NOT a global broadcast.
- only effect entities whose `EffectEntity.SourceId` equals the selected primary
  target card are considered.
- `FireCustomTriggerEffectDescriptor` is the global named-event emitter.
- avoid deliberately constructing recursive loops (for example, a card's
  `PlayTrigger` firing its own `PlayTrigger` forever).
