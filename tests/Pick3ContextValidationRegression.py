from pathlib import Path
s=(Path(__file__).resolve().parents[1]/'inc/triggers/detail/CustomTriggerDispatch.inl').read_text()
for marker in ['PICK3_CONTEXT: firingEffect=', 'queryModel=%p expectedModel=%p producerModel=%p modelMatches=%d', 'PICK3_VALIDATE: listenerEffect=', 'sourceExists=%d validatorAvailable=%d validatorCalled=%d validatorResult=%d', 'Pick3ValidationObservation', 'observation->validatorCalled = true', 'observation->validatorResult = result']:
    assert marker in s,marker
assert 'if (sourceExists)' in s
assert 'if (!eligible)' in s
assert 'triggerValidatorShouldTrigger[validator]' in s
assert 'triggeredEffectsAdd[triggeredEffects](triggerData)' in s
assert 'returnToPool[instances](queryContext)' in s
print('PASS: read-only Pick3 context and validation diagnostics retained stock flow')
