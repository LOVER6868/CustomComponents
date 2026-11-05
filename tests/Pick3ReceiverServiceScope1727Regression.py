"""1.7.27: receiver-side initial Type-3 scope and removal of two-gesture code.
Source contract check only; does not prove native PvP acceptance.
"""
from pathlib import Path
p=Path(__file__).resolve().parents[1]
s=(p/'inc/pick3/PickFromThreeSubmission.hpp').read_text()
u=(p/'inc/presentation/PickFromThreeDiscoverPresentation.hpp').read_text()
m=(p/'main.cpp').read_text()
c=(p/'config.ini').read_text()
assert 'trickServiceActivePlay=play;' in s
assert 'if(play) trickServiceActivePlay=play;' in s
assert 'if(play) trickServiceActivePlay=prior;' in s
assert 'trickServiceActivePlay == play &&' in s
assert 'trickPlayTypeField[play].Get() == 3' in s
assert 'const bool receiverService = trickServiceActivePlay &&' in s
assert 'const bool nestedDetermine = receiverService &&' in s
assert 'Obj* activePlay = serviceScope ? trickServiceActivePlay :' in s
assert 'const bool pairScope = (candidateScope || serviceScope)' in s
assert 'const bool pair = exactZone && Pick3VerifyCapturedRevivePair(' in s
assert 'const bool fromProvenLegacyReceiver =' in s
assert 'pick3PrimaryCaptureModel == pick3CurrentValidationModel' in s
assert 'const bool result=originalTrickIsTargetingValid ? originalTrickIsTargetingValid' in s
assert 'if (serviceScope && trickServiceActivePlay==activePlay)' in s
assert 'const int outboundTarget=provenSole>=0 ? provenSole : deferredDropZone;' in s
assert 'for(const auto& item:activeRequest.options){group.ids.push_back(item.entityId);group.guids.push_back(item.cardGuid);}' in u
for banned in ('PrePlayRevive1725', 'prePlayUiActive','committedUiOptionIndex',
               'InstallPrePlayReturnObserver','ShouldForwardPrePlayTap', 'selectedPreplay'):
    assert banned not in s+u+m, banned
assert 'Pick3PrePlaySelection' not in c+m
assert not (p/'inc/pick3/PrePlayRevive1725.hpp').exists()
assert 'Pick3::Submission::isStockChooseBoardTap=&ShouldForwardStockChooseTap;' in u
print('PASS: receiver scoped to real Type-3 service and complete legacy preplay removal')
