#!/usr/bin/env python3
"""Source guards for one-option PF3 stock handoff and scene-safe presentation."""
from pathlib import Path
root = Path(__file__).resolve().parents[1]
ui = (root/'inc/presentation/PickFromThreeDiscoverPresentation.hpp').read_text()
submission = (root/'inc/pick3/PickFromThreeSubmission.hpp').read_text()
metadata = (root/'inc/presentation/detail/Pick3CardMetadata.hpp').read_text()
assert 'SOLE_STOCK_CHOICE' in ui
assert 'AUTO_SOLE_ARM' not in ui
assert 'AUTO_SOLE_WAIT' not in ui
assert 'if(autoSolePending) {' not in ui
assert 'StageStockHandoff(activeRequest,chosenIndices)' in ui
assert 'Ui::BeginExit(chosenIndices.back())' in ui
assert 'RowY(state.count,i)' in ui
assert 'RowY(int count,int i)' in ui
assert 'STOCK_HANDOFF: COMMAND_MATCH' in submission
assert 'overlayCapturesInput = false;' in submission
assert 'return localizationReady && capturedLocalizationHandle ? capturedLocalizationMapper : nullptr;' in metadata
print('PASS: one-option PF3 now uses stock-owned Choose; shared visual/hitbox row coordinates; no live scene scan')
