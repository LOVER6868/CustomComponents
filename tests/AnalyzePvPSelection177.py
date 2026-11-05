#!/usr/bin/env python3
"""Summarize PF3 receiver-mode and PvP queue-retirement logs, from ZIP or text.

Run on Windows:
  py tests\\AnalyzePvPSelection177.py "20261103-xxxxxx_PvP-Test.zip"
No dependencies; does not mutate the game or capture.
"""
import argparse
import collections
import pathlib
import re
import zipfile

MARKERS = (
    'REPAIR177: RX_LIVE_PREFLIGHT', 'REPAIR177: RX_MODE_NORMALIZE',
    'REPAIR177: RX_MODE_RESTORE', 'REPAIR177: RETIRE_OBSERVE',
    'DB5_PVP: RX_VALID', 'DB5_PVP: RX_INVALID',
    'PVP_RETIRE: PREDICTION_SUCCEEDED', 'PVP_RETIRE: STATE_ABSENT',
    'PVP_RETIRE: STILL_PENDING', 'STOCK_HANDOFF: COMMAND_MATCH',
    'STOCK_HANDOFF: COMMAND_MISMATCH',
    'NETTRACE: stage=POST_REQUEST_SUCCESS_ENTER',
)

def sources(path):
    if zipfile.is_zipfile(path):
        with zipfile.ZipFile(path) as z:
            for name in z.namelist():
                if name.endswith('highlights.txt') or (name.endswith('full_logcat.txt') and not any(n.endswith('highlights.txt') for n in z.namelist())):
                    yield name.replace('\\', '/').split('/')[-1], z.read(name).decode('utf-8', 'replace').splitlines()
    else:
        yield path.name, path.read_text(encoding='utf-8', errors='replace').splitlines()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('path', type=pathlib.Path)
    args=parser.parse_args()
    for name,lines in sources(args.path):
        events=collections.Counter()
        normalized=collections.Counter()
        restored=collections.Counter()
        for line in lines:
            for marker in MARKERS:
                if marker in line:
                    events[marker]+=1
                    if marker in ('REPAIR177: RX_MODE_NORMALIZE','REPAIR177: RX_MODE_RESTORE'):
                        m=re.search(r'filter=(0x[0-9a-f]+)',line)
                        if m:
                            (normalized if 'NORMALIZE' in marker else restored)[m.group(1)]+=1
        print(f'\n{name}')
        for k in MARKERS:
            if events[k]: print(f'  {k}: {events[k]}')
        for key in sorted(set(normalized)|set(restored)):
            n,r=normalized[key],restored[key]
            if n!=r: print(f'  WARNING: RX_MODE filter={key} normalized={n} restored={r}')
        if not events['REPAIR177: RX_MODE_NORMALIZE']:
            print('  No receiver-side live-mode normalization observed (test a live PF3 on both peers).')
        if events['PVP_RETIRE: STILL_PENDING']:
            print('  NOTE: local retire timeout != proof the server rejected a packet; correlate peer logs and replay.')

if __name__ == '__main__': main()
