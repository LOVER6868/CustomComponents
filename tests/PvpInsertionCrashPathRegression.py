#!/usr/bin/env python3
"""Guards the crash fix's actual PvP call sites (host-side static test)."""
from pathlib import Path
r=Path(__file__).resolve().parents[1]
s=(r/'inc/zones/DeckMaterializer.hpp').read_text()
cfg=(r/'config.ini').read_text()
assert 'MaterializeInsertedCardsPvp=false' in cfg
assert '#include "diagnostics/PvpInsertionSafeLookup.hpp"' in s
assert 'FindPvpModelEntityWithoutInvoke' in s
# All match-bound PvP insertion calls must use the no-invoke lookup.
for fn, end in [('ReadPvpPool','ExistingPvpDeckOwner'),
                ('ExistingPvpDeckOwner','RemoveExtraCardsProcessHook'),
                ('MaterializePvpInsertions','AfterStockCreateCardInDeck')]:
    start=s.index(fn+'(')
    region=s[start:s.index(end+'(',start)]
    assert 'modelGetEntityById[model]' not in region, fn
assert 'FindPvpModelEntityWithoutInvoke(model,poolId)' in s
assert 'FindPvpModelEntityWithoutInvoke(model,entry.entityId)' in s
assert 'FindPvpModelEntityWithoutInvoke(model,id)' in s
# The single retained managed lookup is PvE startup, not PvP insertion.
assert s.count('modelGetEntityById[model](deckId)') == 1
assert 'pvpCreateCardIfMissing[system](entry,elementType)' in s
assert 'pvpSessionFailed=true' in s and 'pvpSessionFailed=false' in s
assert 'originalInitializeDeck(self, playerData, playerEntity, method);' in s
print('PvP inserted-card IL2CPP crash-path guard: PASS')
