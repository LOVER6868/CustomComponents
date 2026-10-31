#!/usr/bin/env python3
"""Guard the actual source route; host-only, not an IL2CPP runtime test."""
from pathlib import Path
s=(Path(__file__).resolve().parents[1]/'inc/zones/DeckMaterializer.hpp').read_text()
assert 'ExistingPvpDeckOwner(model,after[target])' in s
assert 'const int expectedOwner=ownerProof.ownerId' in s
assert 'reason=ownerProof.reason' in s
assert 'poolOwner=FindComponent<Obj>(after[target].entity,"Owner")' not in s
assert 'proof.cardEntityId = entityId[cardEntity].Get()' in s
assert 'proof.cardGuid = cardComponentGuid[card].Get()' in s
assert 'proof.ownerId = pvpOwnerId[owner].Get()' in s
assert 'PVP_INSERT_OWNER pool=%d owner=%d verifiedCopies=%d' in s
assert 'pvpCreateCardIfMissing[system](entry,elementType)' in s
assert 'model==PvpRootedModel()' in s
print('Step2B existing-card owner validation integration: PASS')
