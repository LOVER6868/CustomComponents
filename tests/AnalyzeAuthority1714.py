#!/usr/bin/env python3
"""Read logcat(s) or a PvP test ZIP and summarize 1.7.14 boundary evidence."""
import sys,zipfile,re,collections,pathlib
patterns=('REPAIR1714:', 'HORRIBLE FAILURE', 'NETTRACE: stage=REMOTE_PROCESS_ENTER',
          'NETTRACE: stage=CONNECTOR_POST_WITH_HANDLERS_RETURN', 'TRICK_SERVICE: phase=DETERMINE_EXIT')

def scan(name, lines):
    counts=collections.Counter()
    selected=[]
    for line in lines:
        if 'REPAIR1714:' in line:
            marker=line.split('REPAIR1714:',1)[1].split(' ',1)[0]
            counts[marker]+=1
        if any(p in line for p in patterns):
            selected.append(line.rstrip())
    print('\n###',name, 'events:',dict(counts))
    for line in selected[-220:]: print(line)

for name in sys.argv[1:]:
    p=pathlib.Path(name)
    if zipfile.is_zipfile(p):
        with zipfile.ZipFile(p) as z:
            for n in z.namelist():
                if n.endswith('highlights.txt'):
                    scan(n, z.read(n).decode('utf-8','replace').splitlines())
    else: scan(name,p.read_text(errors='replace').splitlines())
