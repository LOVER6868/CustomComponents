#!/usr/bin/env python3
"""Non-runtime guardrails for targeted revive drag patch."""
from pathlib import Path
h=(Path(__file__).resolve().parents[1]/'inc/pick3/PickFromThreeSubmission.hpp').read_text()
assert 'IsRecentValidatedInitialPlay(src,deferredDropZone)' in h
assert 'IsReviveDropLaneOnCurrentModel(target)' in h
assert 'inHand==0' in h
assert 'REPAIR1712: DROP_PREFLIGHT' in h
assert 'REPAIR1712: COMMAND_PREFLIGHT' in h
assert 'REPAIR1712: PRIMARY_MISSING' in h
assert 'stockFallback_noSyntheticPair' in h
assert 'ids && ids->size==1 && ids->items && zone>=0' not in h[h.index('inline void TrickStopDragTrace'):h.index('inline void TrickLogGeneratedTargets')]
print('Patch 1.7.12 focused source regression: PASS')
