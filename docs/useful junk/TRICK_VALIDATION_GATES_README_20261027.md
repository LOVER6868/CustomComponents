# Pick3 initial play validation trace (read-only)

This revision **disables** `kDeferredPick3InitialZoneExperiment` and adds `TRICK_VALIDATION` logs without modifying stock results.

Instrumented:
- `GameStateUtils.GetPossiblePlayOrdersInTargetForCard(Entity,int,int)` (placement orders)
- `CompositeValidator.Validate`, `OneTimeEffectValidator.Validate`, `NullValidator.Validate`, and `InHandValidator.Validate` (concrete implementations). Only captures executions made while `GameStateUtils.GetValidPlaysForCard` is in scope, for GUIDs 699/712/713/27.

The interface `Validator.Validate` has no single native implementation; **validator coverage is partial**. If the summary shows `ordersCalls > 0` and `tracedValidateCalls=0`, do not infer that no validator was called. Other concrete validators may have been used.

Look for:
```
TRICK_VALIDATION: SETUP orders=1 ... readOnly=1 ...
TRICK_VALIDATION: phase=ORDERS guid=712 target=... orders=0
TRICK_VALIDATION: phase=VALIDATE guid=712 type=... accepted=0
TRICK_VALIDATION: phase=SUMMARY guid=712 ordersCalls=... tracedValidateCalls=... coverage=partial
TRICK_GENERATION: phase=PLAYS guid=712 ... count=0
```

Compare 699 and 712 at similar board states. The absence of a VALIDATE line is inconclusive if the relevant concrete validator was not hooked. Native Android build and on-device test have not been performed.
