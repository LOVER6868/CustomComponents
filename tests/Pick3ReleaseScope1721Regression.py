"""Patch 1.7.21 read-only synchronous release/Play scope invariants."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
pre=(root/'inc/pick3/PrePlayLifecycleTrace.hpp').read_text()
sub=(root/'inc/pick3/PickFromThreeSubmission.hpp').read_text()
btn=(root/'inc/pick3/TargetButtonTrace.hpp').read_text()
assert 'thread_local unsigned releaseDepth = 0' in pre
assert 'thread_local unsigned releaseSerial = 0' in pre
assert 'thread_local unsigned releasePlayCount = 0' in pre
assert 'kMaxReleaseDepth = 16' in pre
assert 'if (releaseDepth < kMaxReleaseDepth) ++releaseDepth;' in pre
assert 'if (after && releaseDepth) --releaseDepth;' in pre
assert 'insideStopDrag=%d readOnly=1' in pre
assert 'if (!after && releaseDepth) ++releasePlayCount;' in pre
assert 'releaseDepth = releaseSerial = releasePlayCount = 0;' in pre
assert 'PrePlayLifecycleTrace::OnStopDrag(cardId,false);' in sub
assert 'if (originalTrickStopDrag) originalTrickStopDrag(self, method);' in sub
assert 'PrePlayLifecycleTrace::OnStopDrag(cardId,true);' in sub
assert sub.index('PrePlayLifecycleTrace::OnStopDrag(cardId,false);') < sub.index('if (originalTrickStopDrag) originalTrickStopDrag(self, method);') < sub.index('PrePlayLifecycleTrace::OnStopDrag(cardId,true);')
assert sub.count('if (originalTrickStopDrag) originalTrickStopDrag(self, method);') == 1
assert sub.count('if (originalPlayCardExecute) originalPlayCardExecute(self, method);') == 1
assert pre.count('BNM::BasicHook(') == 1
assert btn.count('hook(holder.GetMethod("BindCommandsForState",1),Bind,oldBind);') == 1
assert 'const int outboundTarget=provenSole>=0 ? provenSole :' in sub
assert 'selectedPreplay>=0 ? selectedPreplay : deferredDropZone;' in sub
assert 'PrePlayRevive1725::enabled' in sub
assert 'slot.renderId = AllocateVirtualRenderId();' in (root/'inc/presentation/PickFromThreeDiscoverPresentation.hpp').read_text()
print('Pick3 1.7.21 release-depth trace and unchanged stock forwarding: PASS')
