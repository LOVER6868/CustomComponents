#!/usr/bin/env python3
"""Step 2A wiring checks. Android/private two-client runtime testing still required."""
from pathlib import Path
import configparser
p=Path(__file__).resolve().parents[1]
config=configparser.ConfigParser(interpolation=None)
config.read(p/'config.ini')
assert config['PvpDeckPrepopulation']['ExpectedDeckFingerprint']=='auto'
pvp=(p/'inc/diagnostics/PvpDeckPrepopulatePrototype.hpp').read_text()
pve=(p/'inc/zones/DeckMaterializer.hpp').read_text()
assert 'getBackingEntityByGuid[database](guid)' in pvp
assert 'DECK_DEFINITIONS_OK' in pvp
assert 'PvpDeckPrepopulatePlan::AcceptFingerprint(requiredFingerprint,hash)' in pvp
assert 'requiredFingerprint=="auto"' in pvp
assert 'Preflight(model,secondPools,secondHash) || secondHash!=hash' in pvp
assert 'ExecuteFullDeckMaterialization(model,secondPools,system,effect,hash)' in pvp
assert 'BeforeStockCreateCardInDeck' in pve and 'AfterStockCreateCardInDeck' in pve
assert 'CreateCardInDeckHook' not in pve
assert 'originalCreateCardInDeck' not in pve
assert 'RepairSameModelDeckEntries(model)' in pve
assert 'if (!factory || model != targetModel ||' in pve
assert 'FindComponent<Obj>(entity,"SuperpowerPool")' in pve
assert 'RepairSameModelDeckEntries(model)' in pve and 'PvpRootedModel()' in pve
hook=(p/'inc/zones/detail/DiscardToDeck.inl').read_text()
assert hook.index('BeforeStockCreateCardInDeck(model)') < hook.index('originalCreateDeckProcessEffect(self, effect, method);') < hook.index('AfterStockCreateCardInDeck(model, observation, guid, amount)')
assert 'kExistingEntityDeckSentinel' in hook
transfer=(p/'inc/zones/DiscardPileTransferSystem.hpp').read_text()
assert 'CreateDeckProcessEffectHook,' in transfer
assert 'originalRemoveExtraCardsProcess' in pve  # unchanged stock lifetime hook
assert 'PickFromThree' not in hook # separate mechanism
print('Step2 dynamic PvP / PvE insert integration checks: PASS')
