from pathlib import Path

root = Path(__file__).resolve().parents[1]
producer = (root/'inc/triggers/detail/TriggerProducerContext.inl').read_text()
dispatch = (root/'inc/triggers/detail/CustomTriggerDispatch.inl').read_text()
lane = (root/'inc/zones/detail/DiscardToLane.inl').read_text()

assert 'FindStockTriggerContextForModel(' in producer
assert 'candidate.model != expectedModel' in producer
assert 'if (preferredInstances && candidate.instances != preferredInstances)' in producer
assert 'exact-model producer missing' in producer
assert 'Obj* expectedModel = nullptr' in dispatch
assert 'FindStockTriggerContextForModel(' in dispatch
assert 'PLAY_RESULT model=%p expectedModel=%p exactModel=%d candidates=%d rejected=%d queued=%d' in dispatch
assert 'CustomTriggerSystem::FireInModel("PlayTrigger", effect, model)' in lane
assert 'CustomTriggerSystem::Fire("PlayTrigger", effect)' not in lane
assert 'CustomTriggerSystem::FireReturnedFromDiscardPile(effect)' in lane
assert 'lanePlayCard[preflightLaneModifier]' in lane
assert 'originalMoveLaneProcessEffect' in lane
print('GlobalPlayTriggerScopeRegression: PASS')
