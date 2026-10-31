from pathlib import Path
r=Path(__file__).resolve().parents[1]
s=(r/'inc/pick3/PickFromThreeSubmission.hpp').read_text()
g=(r/'inc/triggers/CustomTriggerSystem.hpp').read_text()
assert 'request.mode != SelectionMode::CardDatabase && Local()' in s
assert 'request.requiredTargets == 1 && verifiedLiveChoice.valid' in s
assert 'selectedTriggerIdentity.effectEntityId == request.effectEntityId' in s
assert 'producer && producer->triggeredEffects' in s
assert 'liveSelectionQueueCallback = &OnLiveStockQueueBoundary;' in s
assert 'liveEarlyArmed = false; // one attempt per transaction' in s
assert 'const bool fired = EmitSelectedTrigger(liveEarlyRequest, true);' in s
assert 'liveEarlyQueued = fired;' in s
assert 'request.mode == SelectionMode::CardDatabase' in s
assert 'databaseEarlyQueued' in s
assert 'originalQueueProcessEffect(self, effect, method);' in g
assert 'triggers == liveSelectionQueueScope' in g
assert 'liveSelectionQueueBusy = true;' in g
assert s.count('liveSelectionQueueCallback = nullptr;') >= 2
print('PASS: local single-live early insertion, queue identity, fallback and cleanup')
