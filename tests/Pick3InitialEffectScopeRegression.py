from pathlib import Path
p = Path(__file__).resolve().parents[1] / "inc/pick3/PickFromThreeSubmission.hpp"
s = p.read_text()
start = s.index("inline bool TrickEffectIsTargeted(")
end = s.index("inline Obj* TrickEffectValidTargets(", start)
body = s[start:end]
assert "IsGenericPick3ReviveEffect(effectId)" in body
assert "return result;" in body
print("PASS: initial targetedness override limited to first revive effect")
