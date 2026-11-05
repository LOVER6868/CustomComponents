from pathlib import Path
root = Path(__file__).resolve().parents[1]/'inc'
a = (root/'triggers/CustomTriggerSystem.hpp').read_text()
b = (root/'pick3/PickFromThreeSubmission.hpp').read_text()
assert 'pick3QueueBudget.store(96' in a
assert 'TakePick3QueueAudit(window, seq)' in a
assert 'pickFromThreeStockQueueSamples' not in a
assert 'ArmPick3QueueAudit("DB_SELECTION", effect, group)' in b
assert 'ArmPick3QueueAudit("SELECT_SERVICE", -1, -1)' in b
assert 'originalQueueProcessEffect(self, effect, method);' in a
assert 'originalLocalSelectProcess(self,play,method);' in b
print('Pick3 selection queue window regression OK')
