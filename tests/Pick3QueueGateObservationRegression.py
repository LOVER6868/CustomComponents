from pathlib import Path
root = Path(__file__).resolve().parents[1]
system = (root/'inc/triggers/CustomTriggerSystem.hpp').read_text()
dispatch = (root/'inc/triggers/detail/CustomTriggerDispatch.inl').read_text()
submission = (root/'inc/pick3/PickFromThreeSubmission.hpp').read_text()
assert 'gateHasTriggers = triggeredCollection.GetMethod("HasTriggers", 0)' in system
assert 'gateHasNext = concreteQueue.GetMethod("HasNext", 0)' in system
assert 'gateShiftWillChangeGroup = concreteQueue.GetMethod("ShiftWillChangeGroup", 0)' in system
assert 'GateField(candidate, 0x18) != expectedTriggers' in system
for phase in ['BEFORE_CUSTOM_FIRE','AFTER_CUSTOM_FIRE','STOCK_PROCESS_ENTER','STOCK_PROCESS_EXIT']:
    assert phase in system+dispatch
assert 'originalQueueProcessEffect(self, effect, method);' in system
assert 'originalQueueRegisterHandlers(self, router, method);' in system
assert 'QueueTriggeredEffects[' not in system
assert 'ProcessEffects[' not in system
assert 'const bool databaseSelection = request.mode == SelectionMode::CardDatabase;' in submission
assert 'if (databaseSelection && dispatchIdentity.valid && exactDatabaseBinding)' in submission
print('PASS: observation-only queue gates, exact collection identity, stock calls preserved, database-only target retained')
