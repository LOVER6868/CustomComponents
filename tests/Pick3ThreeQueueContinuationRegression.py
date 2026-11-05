from pathlib import Path
source = (Path(__file__).resolve().parents[1] / 'inc/pick3/PickFromThreeSubmission.hpp').read_text()
start = source.index('inline void BindCommittedPick3RevivePrimary(Obj* service, const TargetBridge::ScopedValidationRecipient& recipient) {')
end = source.index('inline void TraceUnearthQueueAtBoundary(', start)
body = source[start:end]
for phrase in ['boundQueueEffects[queue].Get()', 'boundQueueResponses[queue].Get()',
               'boundQueueGroupResponses[queue].Get()', 'phase[e].Get() != 1',
               'matching != 1', 'SKIP_BOUND', 'collection=%s']:
    if phrase == 'phase[e].Get() != 1':
        assert 'stage == 1' in body
    else:
        assert phrase in body, phrase
assert 'primary[match].Set(recipient.chosen);' in body
assert 'secondary[match].Set(' not in body
print('Pick3 three-queue continuation structural regression passed')
