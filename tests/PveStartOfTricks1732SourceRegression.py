from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
bridge=(ROOT/'inc/pick3/PickFromThreeTargetBridge.hpp').read_text()
ui=(ROOT/'inc/presentation/PickFromThreeDiscoverPresentation.hpp').read_text()
assert 'VerifiedCachedModelForActivePve' in bridge
assert 'age>1500' in bridge
assert 'delta<=200' in bridge
assert 'offer.request.ownerPlayerId!=owner' in bridge
assert 'guid!=option.cardGuid' in bridge
assert 'if(!published && local && !remote && localOwner>=0' in ui
assert 'ExactTurn(identity,remote)' in ui
assert 'if(published) model=cached;' in ui
assert 'PublishForActiveEffect(\n                            h.effectEntityId,h.effectGroupId,cached,localOwner)' in ui
assert 'PVE_START_TRICKS: phase=RETRY_RESULT' in ui
print('PvE start-of-tricks guarded retry regression: PASS')
