from pathlib import Path
source = Path(__file__).resolve().parents[1] / 'inc/pick3/PickFromThreeSubmission.hpp'
s = source.read_text()
assert s.index('inline VerifiedLiveChoice verifiedLiveChoice{};') < s.index('inline void ResetMatch()')
assert s.index('const bool mismatchedLiveGuid') < s.index('verifiedLiveChoice = {request.effectEntityId') < s.index('const auto dispatch = SelectionTransport::Dispatch(svc, play);')
segment = s[s.index('inline bool EmitSelectedTrigger(' , s.index('inline bool AwaitingResult()')):s.index('inline void OnVerifiedDatabaseDraw(',s.index('inline bool AwaitingResult()'))]
assert 'verifiedLiveChoice.entityId == boundLiveEntity' in segment
assert 'verifiedLiveChoice.cardGuid == chosenGuid' in segment
assert 'TargetBridge::StillEligibleWithGuid' not in segment
assert 'verifiedLiveChoice = {};' in s[s.index('inline void FinishTransaction'):]
assert 'databaseEarlyArmed = request.mode == SelectionMode::CardDatabase' in s
print('PASS: one verified live entity/GUID captured before dispatch, matched after retirement; DB path unchanged')
