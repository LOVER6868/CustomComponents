"""Static guard and isolated truth-table check; not an Android/server test."""
from pathlib import Path
s=(Path(__file__).resolve().parents[1]/'inc/pick3/PickFromThreeSubmission.hpp').read_text()
g=s[s.index('inline Obj* TrickGeneratedPlays('):s.index('inline void Drop(',s.index('inline Obj* TrickGeneratedPlays('))]
e=s[s.index('static void PlayCardExecute('):s.index('inline bool Submit(',s.index('static void PlayCardExecute('))]
assert 'generatedTarget==zone &&' in g
assert 'sole.target==generatedTarget' not in g
assert 'firstProof.effectId==sole.effect' in g
assert 'sole.model==generationScope.model' in g
assert '!sole.ambiguous' in g
assert 'deferredValidatedPlay.stockSolePrimary=sole.target' in g
assert 'ids->items->m_Items[0]=zone;' in g  # Existing multi-choice normalization stays
assert 'TrickGenerationGuid(candidateSole)>0' in e
assert 'firstNow.effectId==deferredValidatedPlay.effect' in e
assert 'deferredValidatedPlay.guid==TrickGenerationGuid(src)' in e
assert 'soleStillBound ? candidateSole : -1' in e
assert 'originalPlayCardExecute(self, method)' in e

def bound(*, generated, zone, sole_card, card, sole_target, ambiguous, effect, first_effect, model_ok, target_guid):
    return generated==zone and sole_card==card and sole_target>=0 and sole_target!=card and not ambiguous and effect==first_effect and model_ok and target_guid>0
cases=[
 ('one candidate, generated cast',dict(generated=10,zone=10,sole_card=121,card=121,sole_target=103,ambiguous=False,effect=122,first_effect=122,model_ok=True,target_guid=352),True),
 ('two candidates, no proof',dict(generated=10,zone=10,sole_card=-1,card=121,sole_target=-1,ambiguous=False,effect=-1,first_effect=122,model_ok=True,target_guid=0),False),
 ('foreign model',dict(generated=10,zone=10,sole_card=121,card=121,sole_target=103,ambiguous=False,effect=122,first_effect=122,model_ok=False,target_guid=352),False),
 ('sibling trigger effect',dict(generated=10,zone=10,sole_card=121,card=121,sole_target=103,ambiguous=False,effect=123,first_effect=122,model_ok=True,target_guid=352),False),
 ('stale or contradictory primary',dict(generated=10,zone=10,sole_card=121,card=121,sole_target=103,ambiguous=True,effect=122,first_effect=122,model_ok=True,target_guid=352),False),
 ('not a casting play',dict(generated=103,zone=10,sole_card=121,card=121,sole_target=103,ambiguous=False,effect=122,first_effect=122,model_ok=True,target_guid=352),False),
 ('not a card entity',dict(generated=10,zone=10,sole_card=121,card=121,sole_target=103,ambiguous=False,effect=122,first_effect=122,model_ok=True,target_guid=-1),False),
]
for name,kwargs,expected in cases:
 assert bound(**kwargs)==expected,name
 print('PASS',name)
print('Patch 1717: static checks + 7 isolated cases passed')
