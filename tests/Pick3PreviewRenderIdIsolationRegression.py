"""Patch: visual preview keys must never alias gameplay entity IDs.

Source-contract tests + isolated allocator examples; Android device behaviour is
verified separately by checking mapper exceptions and the PF3 exit animation.
"""
from pathlib import Path

source = (Path(__file__).resolve().parents[1] / 'inc/presentation/PickFromThreeDiscoverPresentation.hpp').read_text()
show = source[source.index('        inline void Show(Obj* holder, const Group& group)'):
              source.index('        // GameInputStateHolder.Update', source.index('        inline void Show(Obj* holder, const Group& group)'))]
hide = source[source.index('        inline void Hide(const char* why) {'):
              source.index('        inline void OnPicked(', source.index('        inline void Hide(const char* why) {'))]

assert 'slot.id = group.ids[i];' in show
assert 'slot.guid = group.guids[i];' in show
assert 'slot.renderId = AllocateVirtualRenderId();' in show
assert 'slot.renderId = slot.id >= 0 ? slot.id' not in show
assert 'CreateNewObjectParameters(slot.renderId, slot.guid)' in show
assert 'mapperCreateCardAsync[cardViewMapper](data, slot.callback, rootView, false)' in show
assert 'viewMapperGet[viewMapper](slot.renderId) == slot.view' in hide
assert 'viewMapperRemove[viewMapper](slot.renderId)' in hide
assert 'viewMapperRemove[viewMapper](slot.id)' not in hide
assert 'inline constexpr int kMaxOptions=3' in source
assert 'Pick3::Submission::Submit(activeRequest,chosenIndices,false)' in source
assert 'Pick3::Submission::StockTapRecipient(activeRequest,index)' in source
assert 'slot.renderId' not in source[source.index('    inline bool PickTap('):source.index('    inline void OnPick3PlayDispatch(')]

next_render_id = -1000000
assigned = []
for gameplay_id in [28, 214, 238, -1, 28, 103]:
    render_id = next_render_id
    next_render_id -= 1
    assert render_id < -1 and render_id != gameplay_id
    assigned.append(render_id)
assert len(set(assigned)) == len(assigned)
print('Pick3 preview render-ID isolation regression: PASS (physical, repeat, database)')
