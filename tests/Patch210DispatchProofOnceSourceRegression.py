"""2.10: Event proof is resolved once, before the listener loop."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
t=(root/'inc/triggers/detail/CustomTriggerDispatch.inl').read_text()
q=(root/'inc/queries/PickedFromThreeQuery.hpp').read_text()
r=(root/'inc/pick3/Patch2ExactRouting.hpp').read_text()
start=t.index('const bool mayBindInFlight =')
loop=t.index('for (int i = 0; i < entities->size; ++i)', start)
end=t.index('const CustomComponents::Pick3::Picked::InFlight::Identity selection{', loop)
assert t.count('inFlightSelectionProof(model, proofSource, proofEffect, proofTarget)') == 1
assert start < t.index('inFlightSelectionProof(model, proofSource, proofEffect, proofTarget)') < loop
assert 'const bool mayBindInFlight = inFlightSelectionProof != nullptr;' in t[start:loop]
assert 'FindEntityById(model, proofTarget)' in t[start:loop]
assert 'InDeckCardGuidOf(proofEntity)' in t[start:loop]
assert 'inFlightSelectionProof(' not in t[loop:end]
assert 'FindEntityById(' not in t[loop:end]
assert 'const bool bound = hasProof && sameSource;' in t[loop:end]
assert 'traceSourceId == proofSource' in t[loop:end]
assert 'InFlight::Scoped selectionScope(selection)' in t
assert 'ValidateWithStockTriggerValidator(' in t[loop:]
assert 'InFlight::Matches(context,effectId,guid)' in q
assert 'version=2.16' in q and 'version=2.16' in r
print('2.16 dispatch proof once PASS')
