#!/usr/bin/env python3
"""Generate embedded Pick3 static sprites from the reviewed PNG list."""
from pathlib import Path
import struct
root=Path(__file__).resolve().parents[1]
assets=root/'assets/pick3-static'
names=['description_plate','cost_sun','cost_brain','attack_leaf','health_heart',
       'inspection_grass','inspection_reference_rim','inline_tooltip_plate',
       'inspection_rim_left','inspection_rim_right','choose_button']
cpp=['#include "presentation/detail/Pick3RewardArt.hpp"',
     'namespace CustomComponents::Presentation::Pick3RewardArt {']
for name in names:
    raw=(assets/(name+'.png')).read_bytes()
    assert raw[:8]==b'\x89PNG\r\n\x1a\n',name
    width,height=struct.unpack_from('>II',raw,16)
    assert 0<width<=4096 and 0<height<=4096,name
    cpp.append('static const std::uint8_t k'+name+'[] = {')
    for i in range(0,len(raw),24):
        cpp.append('  '+', '.join('0x%02x'%b for b in raw[i:i+24])+',')
    cpp.append('};')
cpp.append('Blob Get(Kind kind) { switch(kind) {')
for name in names:
    cpp.append('  case '+''.join(part.capitalize() for part in name.split('_'))+
               ': return {k'+name+', sizeof(k'+name+'), '+
               '0, 0};')
cpp.append('  default: return {nullptr,0,0,0};')
cpp.append('} }')
cpp.append('} // namespace CustomComponents::Presentation::Pick3RewardArt')
# Rewrite width/height constants deterministically.
for i,name in enumerate(names):
    raw=(assets/(name+'.png')).read_bytes()
    w,h=struct.unpack_from('>II',raw,16)
    case='  case '+''.join(part.capitalize() for part in name.split('_'))+': return {k'+name+', sizeof(k'+name+'), 0, 0};'
    cpp[cpp.index(case)]=case.replace(' 0, 0}', f' {w}, {h}' + '}' )
(root/'src/Pick3RewardArt.cpp').write_text('\n'.join(cpp)+'\n')
print('Regenerated',len(names),'embedded resources')
