#!/usr/bin/env python3
"""Report per-phone revive CAST_SOURCE and follow-on proof stages. Python 3, no external dependencies."""
import collections
import pathlib
import sys
import zipfile

EVENTS = (
    'REPAIR1710: CAST_SOURCE',
    'REPAIR1710: PAIR_APPROVED',
    'REPAIR1710: PAIR_REJECT',
    'REPAIR1710: CAST_COMMITTED',
    'REPAIR1710: REMOTE_CAST_UNPROVEN',
    'REPAIR1710: EARLY_HANDOFF_CONTEXT',
    'REPAIR1710: SECONDARY_CREATOR_CONTEXT',
    'PICK3_REVIVE_PRIMARY_BIND',
    'PICK3_REVIVE_COMMIT',
    'TRICK_GENERATION: phase=PLAYS',
    'PVP_RETIRE: STILL_PENDING',
)

def analyze(name, data):
    count=collections.Counter()
    samples={}
    for line in data.splitlines():
        for event in EVENTS:
            if event in line:
                count[event]+=1
                samples[event]=line.strip()
                break
    print('\n'+name)
    for event in EVENTS:
        print(f'  {count[event]:4}  {event}')
        if event in ('REPAIR1710: PAIR_REJECT', 'REPAIR1710: REMOTE_CAST_UNPROVEN',
                     'REPAIR1710: CAST_COMMITTED', 'PVP_RETIRE: STILL_PENDING') and event in samples:
            print('       latest:',samples[event][-210:])

if len(sys.argv)!=2:
    raise SystemExit('Usage: py tests/AnalyzeRevive1710.py <PvP-Test.zip or logcat.txt>')
p=pathlib.Path(sys.argv[1])
if zipfile.is_zipfile(p):
    with zipfile.ZipFile(p) as z:
        files=[n for n in z.namelist() if n.lower().endswith(('phone1_highlights.txt','phone2_highlights.txt'))]
        for n in sorted(files): analyze(n.replace('\\','/').rsplit('/',1)[-1],z.read(n).decode('utf-8','replace'))
        if not files: print('No phone highlights found in archive')
else: analyze(p.name,p.read_text(errors='replace'))
