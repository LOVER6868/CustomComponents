"""Patch 1.7.19 menu-lifecycle invariants. No claim of on-device testing."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]/'inc'
p=(root/'presentation/PickFromThreeDiscoverPresentation.hpp').read_text()
b=(root/'pick3/PickFromThreeTargetBridge.hpp').read_text()
assert 'failedSerial' not in p, 'transient missing mapper must not permanently blacklist menu'
assert 'VIEW_MAPPER_WAIT' in p and 'retryOnLoader=1' in p
assert 'if(FindOpenGroup(source,group) && group.serial==serial)' in p
assert 'activeOfferModel=Ui::Pin(model,activeOfferModelHandle);' in p
assert 'ReleaseActiveOfferModel();' in p
assert 'const bool suspended=activeOfferModelHandle && activeOfferModel &&' in p
assert 'SuspendUnsubmittedOffer(activeRequest,activeOfferModel)' in p
assert 'hasRequest && !matching' in p
assert 'if(!suspended)' in p
assert 'CompleteLocalSelection(activeRequest.effectEntityId,activeRequest.effectGroupId,false);' in p
assert 'auto status=Pick3::Submission::PollResult();' in p
assert 'if(awaitingResult)' in p
assert 'if (Ui::state.exiting || Ui::state.deferredExit) return;' in p
assert 'HasNormalizedActiveChoice(' in p and 'mode==1' in p
assert 'entry.effectId==effect && entry.groupId==group' in b
assert 'entry.filterHandle!=0' in b
assert 'ownershipPhase!=OwnershipPhase::Displayed' in b
assert 'saved.suspended=true;' in b
assert 'CompleteLocalSelection(request.effectEntityId,request.effectGroupId,true);' in b
assert 'slot.renderId = AllocateVirtualRenderId();' in p
print('Pick3 menu interrupted-offer suspension, retry, manual scope and preview-ID checks: PASS')
