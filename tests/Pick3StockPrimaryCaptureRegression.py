from pathlib import Path
s=(Path(__file__).resolve().parents[1]/'inc/pick3/PickFromThreeSubmission.hpp').read_text()
assert 'PICK3_STOCK_PRIMARY_CAPTURE' in s
assert 'pick3PrimaryCaptureEffect == effectId' in s
assert 'std::strcmp(name, "PoolableList`1")' in s
assert 'list->items->capacity' in s
assert 'deferredPrimaryCards.ids.push_back(id)' in s
assert 'pick3PrimaryCaptureEffect = -1;' in s
assert 'secondaryPairVerified=0 decision=stock-unchanged' in s
print('PASS: first-effect scoped bounded stock primary snapshot, cleanup, and fail-closed validation')
