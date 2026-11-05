"""Static sanity guard for the submission crash containment patch."""
from pathlib import Path
p = Path(__file__).resolve().parents[1] / 'inc/pick3/PickFromThreeSubmission.hpp'
s = p.read_text()
area = s[s.index('inline long long TimerRemaining() {'):s.index('inline int RandomIndex(', s.index('inline long long TimerRemaining() {'))]
assert 'GetTimeRemaining' not in area
assert 'BNM::TryInvoke' not in area
assert 'return -1;' in area
assert 'inline bool DeadlineExpired()' in area
assert 'return false;' in area
assert 'secondaryLaneVerified=0' in s
print('timer crash containment: OK; revive viability remains unverified')
