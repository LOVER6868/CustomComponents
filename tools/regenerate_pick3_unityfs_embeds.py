#!/usr/bin/env python3
"""Regenerate embedded original UnityFS bundles; deterministic, no external deps.

Run from anywhere: python tools/regenerate_pick3_unityfs_embeds.py
Validates expected SHA-256 from the provenance manifest before generating.
"""
import json,hashlib
from pathlib import Path
root=Path(__file__).resolve().parents[1]
assets=root/'src'/'pick3-unityfs-originals'
manifest=json.loads((assets/'manifest.json').read_text())
cpp=['#include "presentation/detail/Pick3BundledUnityFS.hpp"',
     'namespace CustomComponents::Presentation::Pick3BundledUnityFS {']
for item in manifest:
    name=item['slot'];raw=(assets/(name.lower()+'.unityfs')).read_bytes()
    assert raw.startswith(b'UnityFS\0'),name
    assert hashlib.sha256(raw).hexdigest()==item['hash_sha256'],name
    cpp.append('static const unsigned char bundle'+name+'[] = {')
    for pos in range(0,len(raw),32):
        cpp.append('    '+','.join(f'0x{x:02x}' for x in raw[pos:pos+32])+',')
    cpp.append('};')
cpp+=['BundleBytes Get(BundleSlot slot) {','    switch (slot) {']
for item in manifest:
    name=item['slot']
    cpp.append('        case BundleSlot::'+name+': return {bundle'+name+', sizeof(bundle'+name+')};')
cpp+=['        default: return {nullptr, 0};','    }','}','}']
(root/'src/Pick3BundledUnityFS.cpp').write_text('\n'.join(cpp)+'\n')
print('Generated',len(manifest),'original UnityFS bundles')
