#!/usr/bin/env python3
"""Source guard: query hot-path diagnostics must never invoke managed getters."""
from pathlib import Path
source = Path(__file__).resolve().parents[1] / 'inc/queries/PickedFromThreeQuery.hpp'
s = source.read_text()
start = s.index('inline void TracePatch2QueryContext(')
end = s.index('inline int PickedEffectId(', start)
body = s[start:end]
for forbidden in ('BNM::TryInvoke', 'GetMethod(', 'get_SourceId', 'get_TargetId',
                  'get_SpringboardId', 'get_Model', 'ReadPatch2ContextInt',
                  'pickedFromThreeContextSelfId[', 'pickedFromThreeContextModel['):
    assert forbidden not in body, f'Unsafe diagnostic operation in query trace: {forbidden}'
assert 'nativeOnly=1 diagnosticGetters=0' in s
assert 'PickedEffectId(context)' in s
assert 'return match;' in s
assert '#define CUSTOMCOMPONENTS_PF3_EXACT_ROUTING 1' in (source.parents[1] / 'pick3/Patch2ExactRouting.hpp').read_text()
print('PASS: 2.6.2 query diagnostic has no managed getter calls; guarded exact routing enabled')
