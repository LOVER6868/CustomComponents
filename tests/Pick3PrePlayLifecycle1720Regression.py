"""Patch 1.7.20 pre-Play read-only probe invariants.

Source-level guards only: does NOT establish an Android runtime or PvP fix.
"""
from pathlib import Path
root = Path(__file__).resolve().parents[1]
pre = (root/'inc/pick3/PrePlayLifecycleTrace.hpp').read_text()
btn = (root/'inc/pick3/TargetButtonTrace.hpp').read_text()
sub = (root/'inc/pick3/PickFromThreeSubmission.hpp').read_text()
main = (root/'main.cpp').read_text()
cfg = (root/'config.ini').read_text()

assert 'Pick3PrePlayTrace' in cfg and 'Enabled=true' in cfg
assert 'BLOOM_CONFIG_BOOL("Pick3PrePlayTrace", "Enabled", false)' in main
assert 'bool prePlayLifecycleTrace = false' in sub
assert 'PrePlayLifecycleTrace::Configure(prePlayLifecycleTrace);' in sub
# The original game holder's Update/Determine/Bind hooks already exist.
assert btn.count('hook(holder.GetMethod("DetermineCurrentInputState",0),Determine,oldDetermine);') == 1
assert btn.count('hook(holder.GetMethod("BindCommandsForState",1),Bind,oldBind);') == 1
assert 'PrePlayLifecycleTrace::OnDetermine(s,before,decided);' in btn
assert 'PrePlayLifecycleTrace::OnBind(s,previous,v,false);' in btn
assert 'PrePlayLifecycleTrace::OnBind(s,previous,v,true);' in btn
assert 'GetMethod("DetermineCurrentInputState"' not in pre
assert 'GetMethod("BindCommandsForState"' not in pre
assert 'GetMethod("Update"' not in pre
# Only the one untouched stock highlight command gains a new trampoline.
assert pre.count('BNM::BasicHook(') == 1
assert 'GetMethod("Execute",0)' in pre
assert 'HighlightPlayableLocationsOrTargetsForCardCommand' in pre
assert pre.count('if (originalHighlight) originalHighlight(self,method);') == 1
assert 'if (!originalHighlight) BNM::BasicHook(execute,Highlight,originalHighlight);' in pre
assert 'const bool valid=holderState.IsValid()' in pre
assert 'if (!valid)' in pre and 'tracing disabled; gameplay unaffected' in pre
assert 'kMaxEvents = 640' in pre
assert 'lastHighlightCount' in pre and 'HIGHLIGHT_EXIT' in pre
assert 'list->size > 128' in pre and '(list->size && !list->items)' in pre
# No new Play rewrite, selection-queue mutation or old command replay.
assert 'PrePlayLifecycleTrace::OnPlay(src,target,handled,false);' in sub
assert 'PrePlayLifecycleTrace::OnPlay(src,target,handled,true);' in sub
assert sub.count('if (originalPlayCardExecute) originalPlayCardExecute(self, method);') == 1
assert pre.count('originalPlayCardExecute(') == 0
assert 'PrePlayLifecycleTrace::OnStopDrag(cardId,false);' in sub
assert 'PrePlayLifecycleTrace::OnStopDrag(cardId,true);' in sub
assert 'const int outboundTarget=provenSole>=0 ? provenSole :' in sub
assert 'const int outboundTarget=provenSole>=0 ? provenSole : deferredDropZone;' in sub
assert 'PrePlayRevive1725' not in sub
assert 'slot.renderId = AllocateVirtualRenderId();' in (root/'inc/presentation/PickFromThreeDiscoverPresentation.hpp').read_text()
print('Pick3 1.7.20 read-only bridge, singleton hooks, bounded logging, legacy fixes: PASS')
