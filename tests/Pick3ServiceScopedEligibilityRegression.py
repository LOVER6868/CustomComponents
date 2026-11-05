from pathlib import Path
p = Path(__file__).resolve().parents[1] / 'inc/pick3/PickFromThreeSubmission.hpp'
s = p.read_text()
assert 'trickServiceActivePlay == play &&' in s
assert 'trickPlayTypeField[play].Get() == 3' in s
assert 'const bool receiverService = trickServiceActivePlay &&' in s
assert 'const bool oneTimeScope = (scopedCandidate || scopedService)' in s
assert 'pick3CurrentValidationModel = pick3OneTimeValidatorModelField[self].Get()' in s
assert 'const bool pairScope = (candidateScope || serviceScope)' in s
assert 'const bool pair = exactZone && Pick3VerifyCapturedRevivePair(' in s
assert 'phase=SERVICE_CAPTURE_SKIPPED' not in s
assert 'PICK3_SERVICE_PAIR: phase=VALIDATE' in s
print('Pick3 service-scoped eligibility source assertions passed')
