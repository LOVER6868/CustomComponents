#!/usr/bin/env python3
"""1.7.15 static invariants and isolated gate truth table (not an Android game test)."""
from pathlib import Path
s=(Path(__file__).resolve().parents[1]/'inc/pick3/PickFromThreeSubmission.hpp').read_text()
start=s.index('inline bool TrickIsTargetingValidTrace(')
end=s.index('inline bool ',start+8)
sub=s[start:end]
assert 'remoteMissingGenerated = serviceScope && !candidateScope' in sub
assert 'trickPlayTypeField[activePlay].Get() == 3' in sub
assert 'kObservedLegacyInitialCastEntity = 10' in sub
assert 'pick3PrimaryCaptureModel == pick3CurrentValidationModel' in sub
assert 'pick3PrimaryCaptureCard == serviceCard' in sub
assert 'pick3PrimaryCaptureEffect >= 0' in sub
assert 'incomingZone == kObservedLegacyInitialCastEntity' in sub
assert 'generatedZone < 0' in sub
assert 'exactZone && Pick3VerifyCapturedRevivePair(' in sub
assert 'if (pair) {' in sub
assert 'REPAIR1715: RECEIVER_CAST_SCOPE' in sub
assert 'REPAIR1715: RECEIVER_PAIR_COMMITTED' in sub
assert 'originalTrickIsTargetingValid(self,valid,selected,errors,method)' in sub
assert 'const bool fromStockBurst = generatedZone >= 0 && incomingZone == generatedZone;' in sub
assert 'const bool fromVerifiedRemoteHand = remoteZoneFallback && incomingZone == modelHand;' in sub

def allow(*, service, candidate, generated, incoming, hand, type_, owner, model, same_model, same_card, effect, pair):
    missing=service and not candidate and generated<0 and type_==3
    legacy=(missing and incoming==10 and owner>=0 and model and same_model and same_card and effect>=0)
    stock=generated>=0 and incoming==generated
    hand_ok=missing and hand>=0 and incoming==hand
    return (stock or hand_ok or legacy) and pair

cases=[
    ('old working peer received 10 but generated none',dict(service=True,candidate=False,generated=-1,incoming=10,hand=8,type_=3,owner=5,model=True,same_model=True,same_card=True,effect=153,pair=True),True),
    ('new model local generated 159',dict(service=False,candidate=True,generated=159,incoming=159,hand=3,type_=3,owner=0,model=True,same_model=True,same_card=True,effect=24,pair=True),True),
    ('wrong incoming target',dict(service=True,candidate=False,generated=-1,incoming=14,hand=8,type_=3,owner=5,model=True,same_model=True,same_card=True,effect=153,pair=True),False),
    ('wrong type',dict(service=True,candidate=False,generated=-1,incoming=10,hand=8,type_=6,owner=5,model=True,same_model=True,same_card=True,effect=153,pair=True),False),
    ('unknown pair',dict(service=True,candidate=False,generated=-1,incoming=10,hand=8,type_=3,owner=5,model=True,same_model=True,same_card=True,effect=153,pair=False),False),
    ('different validator model',dict(service=True,candidate=False,generated=-1,incoming=10,hand=8,type_=3,owner=5,model=True,same_model=False,same_card=True,effect=153,pair=True),False),
    ('wrong card',dict(service=True,candidate=False,generated=-1,incoming=10,hand=8,type_=3,owner=5,model=True,same_model=True,same_card=False,effect=153,pair=True),False),
    ('missing hand still old validated receiver',dict(service=True,candidate=False,generated=-1,incoming=10,hand=-1,type_=3,owner=5,model=True,same_model=True,same_card=True,effect=153,pair=True),True),
    ('not service',dict(service=False,candidate=False,generated=-1,incoming=10,hand=8,type_=3,owner=5,model=True,same_model=True,same_card=True,effect=153,pair=True),False),
    ('generated conflict overrides legacy',dict(service=True,candidate=False,generated=159,incoming=10,hand=8,type_=3,owner=5,model=True,same_model=True,same_card=True,effect=153,pair=True),False),
]
for name,kwargs,expected in cases:
    actual=bool(allow(**kwargs))
    print(('PASS' if actual==expected else 'FAIL')+': '+name)
    assert actual==expected,(name,actual,expected)
print('PASS: source guards and 10 isolated compatibility scenarios')
