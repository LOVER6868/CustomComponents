from pathlib import Path
p = Path(__file__).resolve().parents[1] / 'inc/pick3/PickFromThreeTargetBridge.hpp'
s = p.read_text(encoding='utf8')
start = s.index('static bool ShouldPrimaryHook(')
end = s.index('static bool ShouldSecondaryHook(', start)
h = s[start:end]
assert 'const bool value = originalShouldPrimary(self, filter, candidates, method);' in h
assert 'selectionType[filter].Set(originalValue);' in h
assert 'originalValue != EnumModes::kPickFromThreeCardDatabase' in h
assert 'numberOfTargets[filter].Get() == 1' in h
assert 'count == 1 && ids->items && ids->items->m_Items[0] > 0' in h
assert 'return false;' in h[h.index('PICK3_AUTO_PRIMARY'):]
assert 'return valid;' in h
assert 'return value;' in h
print('PASS: live-card single-primary auto branch, DB5 and multi-card unchanged')
