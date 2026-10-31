#!/usr/bin/env python3
"""Regression for 2026-10-26 startup lock (static checks, not Android runtime)."""
from pathlib import Path
import re
p = Path(__file__).resolve().parents[1]
mat = (p/'inc/zones/DeckMaterializer.hpp').read_text()
transfer = (p/'inc/zones/DiscardPileTransferSystem.hpp').read_text()
routing = (p/'inc/zones/detail/DiscardToDeck.inl').read_text()
main = (p/'main.cpp').read_text()
# The stock method is hooked ONLY in DiscardPileZoneEffects.
assert re.search(r'BNM::BasicHook\s*\(\s*createDeckProcess\s*,\s*CreateDeckProcessEffectHook', transfer)
assert not re.search(r'BNM::BasicHook\s*\(\s*createCardInDeckProcess', mat)
assert 'originalCreateCardInDeck' not in mat
assert '"zones/DeckMaterializer.hpp"' in transfer
# Original method runs exactly once on the stock branch; custom sentinel path stays.
assert routing.count('originalCreateDeckProcessEffect(self, effect, method);') == 1
assert routing.index('BeforeStockCreateCardInDeck(model)') < routing.index('originalCreateDeckProcessEffect(self, effect, method);') < routing.index('AfterStockCreateCardInDeck(model, observation, guid, amount)')
assert 'kExistingEntityDeckSentinel' in routing
# Query registration follows successful discard hook initialization.
assert main.index('DiscardPileZoneEffectsSystem::Initialize()') < main.index('InitializeInDeckQuery()')
assert 'if (!CustomComponents::Systems::DiscardPileZoneEffectsSystem::Initialize())' in main
assert 'if (!CustomComponents::Queries::InitializeInDeckQuery())' in main
print('Step2 single-owner CreateCardInDeck hook and query-startup checks: PASS')
