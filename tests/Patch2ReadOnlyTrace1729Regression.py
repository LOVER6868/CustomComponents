from pathlib import Path
root = Path(__file__).resolve().parents[1]
selection = (root / 'inc/pick3/PickedFromThreeSelection.hpp').read_text()
query = (root / 'inc/queries/PickedFromThreeQuery.hpp').read_text()
submission = (root / 'inc/pick3/PickFromThreeSubmission.hpp').read_text()
assert '#define CUSTOMCOMPONENTS_PF3_SNAPSHOT_TRACE 1' in selection
for token in ['TX_STAGED', 'RX_STAGED', 'RX_DB_STAGED', 'STOCK_PROCESS_BEFORE', 'STOCK_PROCESS_AFTER', 'CALLBACK_SUCCESS_NOT_COMMIT_PROOF', 'CALLBACK_FAILURE']:
    assert token in submission, token
for token in ['patch2ContextModel', 'patch2EffectContext', 'patch2SelfId', 'patch2EffectSourceId', 'patch2EffectTargetId', 'patch2EffectSpringboardId', 'TracePick2QueryContext("GUID"', 'TracePick2QueryContext("ENTITY"']:
    assert token in query, token
# Preserve the baseline's last-per-effect replacement and unscoped query semantics.
assert 'return old.effectEntityId == effectId;' in selection
assert 'MatchesLatestLiveCard(entityId, cardGuid)' in query
assert 'Picked::Store(' in submission
print('Patch2 read-only trace regression PASS')
