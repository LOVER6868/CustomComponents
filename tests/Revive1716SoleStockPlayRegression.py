"""Continuity checks for the 1.7.16 sole-proof approach as corrected in 1.7.17."""
from pathlib import Path
s=(Path(__file__).resolve().parents[1]/'inc/pick3/PickFromThreeSubmission.hpp').read_text()
assert 'deferredPrimaryCards.ids.size()==1' in s
assert 'sole.model==generationScope.model' in s
assert 'deferredValidatedPlay.stockSolePrimary=sole.target' in s
assert 'outboundTarget=provenSole>=0 ? provenSole :' in s
assert 'selectedPreplay>=0 ? selectedPreplay : deferredDropZone;' in s
assert 'const int provenSole=soleStillBound ? candidateSole : -1;' in s
assert 'stockSoleInitialPrimary=previousSole;' in s
assert 'const bool pair = exactZone && Pick3VerifyCapturedRevivePair(' in s
print('Patch 1716/1717 scoped source continuity checks passed')
