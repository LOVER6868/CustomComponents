"""Validate generic bridge wiring and guard diagnostic getter regressions."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
q=(root/'inc/queries/PickedFromThreeQuery.hpp').read_text()
s=(root/'inc/pick3/PickFromThreeSubmission.hpp').read_text()
t=(root/'inc/triggers/detail/CustomTriggerDispatch.inl').read_text()
h=(root/'inc/pick3/PickedFromThreeSelection.hpp').read_text()
assert 'RegisterInFlightSelectionProof(&ProveInFlightSelectedTarget)' in s
assert 'std::count(proof.ids.begin(), proof.ids.end(), targetId) != 1' in s
assert 'proof.model != model' in s and 'proof.effect != effectId' in s
assert 'InFlight::Scoped selectionScope(selection)' in t
assert 'ValidateWithStockTriggerValidator(' in t
assert 'const bool bound = hasProof && sameSource;' in t
assert 'const bool hasProof = proofAccepted && proofGuid > 0;' in t
assert t.count('inFlightSelectionProof(model, proofSource, proofEffect, proofTarget)') == 1
assert t.index('inFlightSelectionProof(model, proofSource, proofEffect, proofTarget)') < t.index('for (int i = 0; i < entities->size; ++i)')
assert 'context == active.context' in h and 'effect == active.effect' in h
assert 'InFlight::Matches(context,effectId,guid)' in q
assert 'get_SourceId' not in q[q.index('bool Matches(Obj* entity, Obj* context)'):q.index('BNM_CustomClass(PickedFromThreeQuery')]
assert 'get_Model' not in q[q.index('bool Matches(Obj* entity, Obj* context)'):q.index('BNM_CustomClass(PickedFromThreeQuery')]
assert 'MATCHES_LATEST' not in t
print('2.10 source guards PASS')
