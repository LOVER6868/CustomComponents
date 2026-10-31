from pathlib import Path
root = Path(__file__).resolve().parents[1]
system = (root/'inc/triggers/CustomTriggerSystem.hpp').read_text()
submission = (root/'inc/pick3/PickFromThreeSubmission.hpp').read_text()
assert 'BNM::Class("PvZCards.Engine", "EffectCollectionProcessor")' in system
assert 'effectProcessor.GetMethod("ProcessEffects", 1)' in system
assert 'BNM::BasicHook(processEffects, EffectCollectionProcessEffectsTrace' in system
assert 'originalEffectCollectionProcessEffects(self, effects, method);' in system
assert 'effects == resumeObservedEffects' in system
assert 'LogPick3ServiceProcess("SERVICE_PROCESS_ENTER", self)' in submission
assert 'LogPick3ServiceProcess("SERVICE_PROCESS_EXIT", self)' in submission
assert submission.count('originalLocalSelectProcess(self,play,method);') == 2
assert 'request.mode == SelectionMode::CardDatabase && Local()' in submission
assert submission.index('WatchPick3ResumeEffects(') < submission.index('SelectionTransport::Dispatch(svc, play)')
assert 'QueueTriggeredEffects[' not in system
assert 'ProcessEffects[' not in system
print('PASS read-only resume boundary scope and original-call preservation')
