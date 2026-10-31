#!/usr/bin/env python3
"""Static PvP insertion scope/lifetime guards, not a runtime test."""
from pathlib import Path
import configparser
r=Path(__file__).resolve().parents[1]
pvp=(r/'inc/diagnostics/PvpDeckPrepopulatePrototype.hpp').read_text()
mat=(r/'inc/zones/DeckMaterializer.hpp').read_text()
hook=(r/'inc/zones/detail/DiscardToDeck.inl').read_text()
transfer=(r/'inc/zones/DiscardPileTransferSystem.hpp').read_text()
main=(r/'main.cpp').read_text()
cfg=configparser.ConfigParser(interpolation=None);cfg.read(r/'config.ini')
assert cfg['PvpDeckPrepopulation']['MaterializeInsertedCardsPvp']=='false'  # Crash-isolation default
assert 'BLOOM_CONFIG_BOOL("PvpDeckPrepopulation", "MaterializeInsertedCardsPvp", false)' in main
assert 'pvpInsertRequested=false' in pvp
assert 'BindPvpSessionAfterVerifiedFullDeck(model,system)' in pvp
assert 'if (exact && Utilities::DeckMaterializer::pvpInsertRequested' in pvp
assert 'PvpRootedModel()!=model' in pvp
assert 'pvpCreateCardIfMissing[system](entry,elementType)' in mat
assert 'pvpGetElementType[system](after[target].entity)' in mat
assert 'if (before.pvpLive)' in mat
assert 'model!=PvpRootedModel()' in mat
assert 'pvpSessionFailed=true' in mat
assert 'pvpSessionFailed=false' in mat
assert 'newIndices=std::move(delta.newIndices)' in mat
assert 'PVP_INSERT_RESULT exact=1' in mat
assert 'PVP_INSERT_SKIP' in mat
assert 'CreateCardInDeckHook' not in mat
assert hook.count('originalCreateDeckProcessEffect(self, effect, method);')==1
assert 'BeforeStockCreateCardInDeck(model)' in hook
assert 'AfterStockCreateCardInDeck(model, observation, guid, amount)' in hook
assert 'createDeckRuntimeAmountToCreate = createDeckRuntime.GetField("AmountToCreate")' in transfer
assert '#include "diagnostics/PvpInsertionDelta.hpp"' in mat
print('Step2B match-bound stock insertion integration checks: PASS')
