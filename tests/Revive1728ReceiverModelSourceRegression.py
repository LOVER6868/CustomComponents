#!/usr/bin/env python3
"""1.7.28: ensure receiver recognition uses the validator's own model."""
from pathlib import Path
p=Path(__file__).resolve().parents[1]/'inc/pick3/PickFromThreeSubmission.hpp'
s=p.read_text()
start=s.index('inline bool TrickValidateObserved(')
end=s.index('inline bool TrickReadConcreteTargetArgument',start) if 'inline bool TrickReadConcreteTargetArgument' in s[start:] else s.index('inline bool TrickIsTargetingValidTrace',start)
body=s[start:end]
assert 'ScopedType3ValidatorRecognition' in body
assert 'trickServiceActivePlay != candidate' in body
assert 'trickPlayTypeField[candidate].Get() != 3' in body
assert 'std::strcmp(kind, "OneTimeEffect")' in body
assert 'pick3OneTimeValidatorModelField[validator].Get()' in body
assert 'receiveReviveRecognitionModel = found;' in body
assert '~ScopedType3ValidatorRecognition() { receiveReviveRecognitionModel = previous; }' in body
assert body.index('receiveType3Model(type, self, play)') < body.index('IsGenericPick3ReviveBurst(trickPlayCardIdField[play].Get())')
assert 'RECEIVER1728: phase=RECOGNITION_MISS' in s
assert 'ScopedReceiveReviveRecognition receiveRecognitionScope(self, play)' in s
assert 'playType[play].Get() != 6' in s
assert 'PrePlayRevive1725' not in s
print('PASS: 1.7.28 exact Type-3 validator-model recognition, restore, Type-6 selection retained')
