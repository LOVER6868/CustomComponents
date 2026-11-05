"""Source invariants for scoped incoming-PvP revive recognition (no runtime simulation)."""
from pathlib import Path
s=(Path(__file__).resolve().parents[1]/'inc/pick3/PickFromThreeSubmission.hpp').read_text()
assert 'inline thread_local Obj* receiveReviveRecognitionModel = nullptr;' in s
assert 'receiveReviveRecognitionModel ? receiveReviveRecognitionModel : Target(boundEntityModelRoot)' in s
assert 'ScopedReceiveReviveRecognition receiveRecognitionScope(self, play);' in s
assert 'receiveReviveRecognitionModel = previous;' in s
assert 'if (!model || !boundEntityModelClass.IsValid() ||' in s
assert s.count('Obj* model=ReviveRecognitionModel();') >= 3
assert 'REPAIR1711: RX_REVIVE_RECONSTRUCTION' in s
assert 'CorrectPick3ReviveRolesBeforeStockSelect(self' in s
assert 'BindCommittedPick3RevivePrimary(self, bridgeContext)' in s
print('PASS: model-scoped receive-side revive recognition invariants')
