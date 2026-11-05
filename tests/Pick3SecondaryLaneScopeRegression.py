from pathlib import Path
root=Path(__file__).resolve().parents[1]
s=(root/'inc/pick3/PickFromThreeSubmission.hpp').read_text()
z=(root/'inc/zones/DiscardPileTransferSystem.hpp').read_text()
m=(root/'main.cpp').read_text()
assert 'Pick3VerifyCapturedRevivePair' in s
assert 'Pick3SecondaryIsLaneOnly' in s
assert 'pick3PhysicalPairCheck(pick3CurrentValidationModel' in s
assert 'if (candidateScope && !serviceScope' in s
assert 'if (pair) return true;' in s
assert 'const bool result=originalTrickIsTargetingValid' in s
assert s.index('if (pair) return true;') < s.index('const bool result=originalTrickIsTargetingValid')
assert 'CheckDiscardRevivePair(model, card, lane, player).legal' in z
assert 'RegisterPick3PhysicalPairCheck(' in m
print('Pick3 secondary-lane scope source assertions passed')
