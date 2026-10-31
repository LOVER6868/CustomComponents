from pathlib import Path
root=Path(__file__).resolve().parents[1]
dispatch=(root/'inc/triggers/detail/CustomTriggerDispatch.inl').read_text()
submission=(root/'inc/pick3/PickFromThreeSubmission.hpp').read_text()
for part in ['b + 0x24','b + 0x28','b + 0x2C','b + 0x30','b + 0x3C']:
    assert part in dispatch, part
assert 'result.effectGroupId = *reinterpret_cast<const int*>(b + 0x3C)' in dispatch
assert 'identitySnapshot && identitySnapshot->valid' in dispatch
assert 'restore ? identitySnapshot->sourceId : sourceId' in dispatch
assert 'restore ? identitySnapshot->effectEntityId : effectEntityId' in dispatch
assert 'effectDataCopyFrom[destinationData](sourceData)' in dispatch
assert 'triggerValidatorShouldTrigger[validator]' in dispatch
assert 'triggeredEffectsAdd[triggeredEffects](triggerData)' in dispatch
assert submission.index('CaptureSelectionTriggerContext(request,svc)') < submission.index('SelectionTransport::Dispatch(svc, play)')
assert 'LogPick3EffectLifetime("CAPTURE", effect)' in submission
assert 'LogPick3EffectLifetime("DISPATCH_RETURN", Target(selectedTriggerEffectRoot))' in submission
assert 'LogPick3EffectLifetime("TRIGGER_FIRE", sourceEffect)' in submission
assert submission.count('selectedTriggerIdentity = {};') >= 4
assert 'dispatchIdentity.valid ? &dispatchIdentity : nullptr' in submission
assert 'auto dispatchIdentity = selectedTriggerIdentity;' in submission
print('PASS: exact stock offsets, pre-process snapshot, cleanup, stock validator and effect data preserved')
