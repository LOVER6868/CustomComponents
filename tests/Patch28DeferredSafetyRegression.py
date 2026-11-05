from pathlib import Path
root=Path(__file__).resolve().parents[1]
r=(root/'inc/pick3/Patch2ExactRouting.hpp').read_text()
t=(root/'inc/triggers/detail/CustomTriggerDispatch.inl').read_text()
q=(root/'inc/queries/PickedFromThreeQuery.hpp').read_text()
assert 'CUSTOMCOMPONENTS_PF3_EXACT_ROUTING 1' in r
assert 'DEFERRED_ORIGIN_BLOCKED' in r
assert 'DEFERRED_EXECUTION_CANDIDATE' in r
assert 'DEFERRED_EXECUTION_MISMATCH' in r
assert 'if(!scope.depth) { TraceDeferredOrigin(queryEffect,candidateGuid,legacy); ReportFallback("NO_STOCK_FINDCARD_SCOPE",queryEffect,-1,candidateGuid);return false; }' in r
assert 'groupGuessForbidden=1 legacyPreserved=1' in r
assert 'PATCH2_TRIGGER: phase=PLAY_LISTENER_QUEUED' in t
assert 'PATCH2_TRIGGER: phase=PLAY_LISTENER_REJECTED' in t
assert 'PATCH2_TRIGGER: phase=PLAY_LISTENER_FAILURE' in t
for banned in ('get_SourceId()', 'get_TargetId()', 'get_SpringboardId()', 'get_Model()'):
    # historical comment may contain names, but never as managed method invocations
    assert 'pickedFromThreeContext'+banned not in q
print('PASS: 2.8 deferred safety and trigger failure logging')
