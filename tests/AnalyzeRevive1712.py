#!/usr/bin/env python3
"""Read-only summary of 1.7.12 revive pipeline events in a directory or test zip."""
import argparse
import collections
import pathlib
import re
import zipfile

KEYS = [
    'REPAIR1712: STOCK_PLAY_PROOF', 'REPAIR1712: STOCK_PLAY_UNPROVEN',
    'REPAIR1712: PRIMARY_MISSING', 'REPAIR1712: DROP_PREFLIGHT',
    'REPAIR1712: DROP_ARM', 'REPAIR1712: COMMAND_PREFLIGHT',
    'REPAIR1712: CAST_REWRITE', 'PICK3_STAGE1_PAIR',
    'PICK3_REVIVE_COMMIT', 'PICK3_REVIVE_PRIMARY_BIND',
    'LANE_FLOW: POST_TRIGGER_ZONE', 'PVP_RETIRE: STILL_PENDING',
]

def analyze(name, text):
    counts = collections.Counter()
    rejected=[]
    for line in text.splitlines():
        if 'Sprig' not in line and 'CustomComponents/' not in line:
            continue
        for key in KEYS:
            if key in line:
                counts[key]+=1
        if 'REPAIR1712: DROP_PREFLIGHT' in line and 'arm=0' in line:
            if len(rejected)<12: rejected.append(line.strip()[-300:])
    print('\n'+name)
    for key in KEYS: print(f'{counts[key]:4}  {key}')
    for item in rejected: print('  skipped:',item)

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('path',help='PvP capture ZIP, directory, or a log file')
    a=ap.parse_args();p=pathlib.Path(a.path)
    if zipfile.is_zipfile(p):
        with zipfile.ZipFile(p) as z:
            for n in z.namelist():
                if n.lower().endswith(('_highlights.txt','_full_logcat.txt')) and 'phone' in n.lower():
                    analyze(n,z.read(n).decode('utf8','replace'))
    elif p.is_dir():
        for f in sorted(p.rglob('*.txt')):
            if 'phone' in f.name.lower() and ('highlights' in f.name.lower() or 'full_logcat' in f.name.lower()):
                analyze(str(f),f.read_text(errors='replace'))
    else: analyze(p.name,p.read_text(errors='replace'))
if __name__=='__main__':main()
