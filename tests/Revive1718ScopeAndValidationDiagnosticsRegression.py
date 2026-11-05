from pathlib import Path
h=Path(__file__).resolve().parents[1]/'inc/pick3/PickFromThreeSubmission.hpp'
s=h.read_text()
assert 'const bool relevant = TrickFocus(guid) || genericScope;' in s
assert 'const bool trace = relevant && ++trickEffectBurstSamples <= 160;' in s
scope=s[s.index('inline void TrickBurstTargetsTrace('):s.index('// Read-only target propagation audit.')]
assert 'if (relevant) {' in scope
assert 'if (kDeferredPick3TrialEnabled && relevant && genericScope)' in scope
assert 'if (trace) TrickEffectLogList("BURST_AFTER"' in scope
assert 'trickEffectBurstDepth = oldDepth;' in scope
assert 'trickEffectBurstGuid = oldGuid;' in scope
assert 'trickEffectBurstEntity = oldEntity;' in scope
assert 'REPAIR1718: INITIAL_PLAY_VALIDATION' in s
assert 'TrickReadConcreteTargetArgument(valid' in s
assert 'TrickReadConcreteTargetArgument(selected' in s
assert 'if (serviceScope || candidateScope)' in s
assert 'if (pair) {' in s
assert 'originalTrickIsTargetingValid(self,valid,selected,errors,method)' in s
assert 'GetEnumerator' not in s[s.index('inline bool TrickReadConcreteTargetArgument'):s.index('inline bool TrickIsTargetingValidTrace')]
print('Patch 1.7.18 scope and diagnostic source regression: PASS')
