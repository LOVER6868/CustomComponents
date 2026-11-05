#!/usr/bin/env python3
"""Read logcat files (or a PvP-Test ZIP) and report key 1.7.13 model/drag/placement traces."""
import re,sys,zipfile
from collections import Counter
from pathlib import Path
KEYS=['REPAIR1713: MODEL_FIELD_BIND','REPAIR1713: GENERATION_MODEL','REPAIR1713: GENERATION_IDENTITY',
      'REPAIR1713: PROOF_ROOT','REPAIR1713: PROOF_REJECT','REPAIR1713: DRAG_MODEL',
      'REPAIR1712: STOCK_PLAY_PROOF','REPAIR1712: DROP_PREFLIGHT','REPAIR1712: DROP_ARM',
      'REPAIR1712: CAST_REWRITE','PICK3_STAGE1_PAIR','PICK3_REVIVE_COMMIT',
      'PICK3_REVIVE_PRIMARY_BIND','LANE_FLOW: POST_TRIGGER_ZONE','PVP_RETIRE: STILL_PENDING']
def analyze(name,data):
    lines=data.splitlines()
    found=[line for line in lines if any(key in line for key in KEYS)]
    print('\n==',name,'==')
    counts=Counter(key for line in found for key in KEYS if key in line)
    for key in KEYS: print(f'{counts[key]:4} {key}')
    print('\nKey excerpts:')
    for line in found[:90]: print(line)
if len(sys.argv)!=2:raise SystemExit('usage: python AnalyzeRevive1713.py <PvP-Test.zip|logcat.txt|directory>')
p=Path(sys.argv[1])
if p.suffix.lower()=='.zip':
    with zipfile.ZipFile(p) as z:
        for name in z.namelist():
            if re.search(r'Phone[12]_full_logcat\.txt$',name):analyze(name,z.read(name).decode('utf-8','replace'))
elif p.is_dir():
    for f in sorted(p.rglob('*full_logcat.txt')):analyze(str(f),f.read_text(errors='replace'))
else:analyze(str(p),p.read_text(errors='replace'))
