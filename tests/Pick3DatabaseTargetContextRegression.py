from pathlib import Path
root = Path(__file__).resolve().parents[1]
s = (root/'inc/pick3/PickFromThreeSubmission.hpp').read_text()
p = (root/'inc/pick3/PickedFromThreeSelection.hpp').read_text()
assert 'request.mode == SelectionMode::CardDatabase' in s
assert 'Picked::TryGetSingleBoundDatabaseEntity(' in s
assert 'dispatchIdentity.targetId = boundDatabaseEntity' in s
assert 'selectedTriggerIdentity.targetId = boundDatabaseEntity' not in s
assert 'dispatchIdentity.valid ? &dispatchIdentity : nullptr' in s
assert 'card.entityId != -1 || card.cardGuid <= 0 || card.boundEntityId <= 0' in p
assert 'entry.effectEntityId != effectId || entry.effectGroupId != groupId' in p
assert 'entry.cards.size() != 1' in p
print('PASS: database-only event target context uses one verified creation; regular path unchanged')
