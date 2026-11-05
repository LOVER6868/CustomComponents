from pathlib import Path

root = Path(__file__).resolve().parents[1] / 'inc' / 'pick3'
submission = (root / 'PickFromThreeSubmission.hpp').read_text()
bridge = (root / 'PickFromThreeTargetBridge.hpp').read_text()
assert 'pick3AcceptedCastPrimaries.effect == effectId' in submission
assert 'pick3AcceptedCastPrimaries.model == model' in submission
assert 'pick3AcceptedCastPrimaries.card == card' in submission
assert 'std::chrono::seconds(30)' in submission
assert 'phase=STORE' in submission and 'phase=READ' in submission and 'phase=CLEAR' in submission
assert 'resolveValidatedInitialCardTargets(model, sourceCard, effectId)' in bridge
assert 'resolveValidatedInitialCardTargets(model, source, effect)' in submission
print('Pick3 accepted cast / blocked selection source regression passed')
