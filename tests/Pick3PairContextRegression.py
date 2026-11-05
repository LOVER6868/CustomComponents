from pathlib import Path
root = Path(__file__).resolve().parents[1]
submission=(root/'inc/pick3/PickFromThreeSubmission.hpp').read_text()
bridge=(root/'inc/pick3/PickFromThreeTargetBridge.hpp').read_text()
for tag in ('GENERATION_ENTER','PRIMARY_CACHE_MISS','PRIMARY_CACHE_READ'):
    assert 'PICK3_PAIR_CONTEXT: phase='+tag in submission
assert 'PICK3_PAIR_CONTEXT: phase=SELECTION_FINDER' in bridge
assert 'primaryIds=not-captured secondaryPairs=not-proven stockUnchanged=1' in bridge
assert 'noValidationBypass=1' in submission
assert 'originalDeciderValidPlays(self, player, cardId, validator, method)' in submission
print('Pick3 pair-context diagnostic/source regression passed')
