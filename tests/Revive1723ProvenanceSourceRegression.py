from pathlib import Path
s = (Path(__file__).resolve().parents[1] / 'inc/pick3/PickFromThreeSubmission.hpp').read_text()
a = s.index('inline void Pick3AddEffectsWithSecondary(')
b = s.index('inline void UnearthSecondaryContextAudit(',a)
body = s[a:b]
assert 'PREPLAY1723: stage=SECONDARY_PRIMARY_PROVENANCE' in body
assert 'RevivePairContract1723::Classify' in body
assert 'provenanceSamples < 48' in body
assert 'const bool canCorrect = verified && zoneVerified && laneIds &&' in body
assert 'if (canCorrect) data->primaryId = selected;' in body
assert 'originalPick3AddEffectsWithSecondary(self, data, effects, method);' in body
print('1723 provenance source regression passed')
