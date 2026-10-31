from pathlib import Path
root = Path(__file__).resolve().parents[1]
dispatch = (root/'inc/triggers/detail/CustomTriggerDispatch.inl').read_text()
query = (root/'inc/queries/PickedFromThreeQuery.hpp').read_text()
creation = (root/'inc/pick3/Pick3CreationProvenanceTrace.hpp').read_text()
assert 'PICK3_LISTENERS: model=%p' in dispatch
assert 'PICK3_LISTENER: verdict=rejected' in dispatch
assert 'PICK3_LISTENER: verdict=queued' in dispatch
assert 'return true;' in dispatch
assert 'PICK3_ENTITY_QUERY: contextEffect=' in query
assert 'PICK3_COST_EFFECT: phase=before' in creation
assert 'PICK3_COST_EFFECT: phase=after' in creation
assert 'originalModifyCost(self, effect, method);' in creation
assert 'GetField("TargetId")' in creation
print('PASS: diagnostic markers present; original cost handler retained')
