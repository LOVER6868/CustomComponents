#!/usr/bin/env python3
"""Compare PF3 revive casting and board-placement stages in a PvP log ZIP.
Usage: py tests/AnalyzeRevive178.py 20261103-xxxxxx_PvP-Test.zip
"""
import collections
import re
import sys
import zipfile
from pathlib import Path

TAGS = [
    'REPAIR178: CAST_PREFLIGHT',
    'REPAIR178: EARLY_HANDOFF_CONTEXT',
    'REPAIR178: SECONDARY_CREATOR_CONTEXT',
    'PICK3_STAGE1_PAIR:',
    'PICK3_SERVICE_PAIR:',
    'PICK3_REVIVE_EARLY_HANDOFF:',
    'PICK3_REVIVE_PRIMARY_BIND:',
    'PICK3_REVIVE_COMMIT:',
    'PICK3_REVIVE_SECONDARY_CREATOR:',
    'PICK3_REVIVE_PAIR_PREFLIGHT:',
    'LANE_FLOW: REMOVE_ENTER',
    'LANE_FLOW: CAPACITY',
    'LANE_FLOW: TEAMUP_CHECK',
    'STOCK_HANDOFF: COMMAND_RESULT',
    'REPAIR177: RX_MODE_NORMALIZE',
    'REPAIR177: RX_MODE_RESTORE',
]

def main(filename):
    with zipfile.ZipFile(filename) as z:
        names = [n for n in z.namelist() if n.endswith('highlights.txt') and not n.endswith('old_Phone1_highlights.txt') and not n.endswith('old_Phone2_highlights.txt')]
        if not names:
            names = [n for n in z.namelist() if n.endswith('full_logcat.txt')]
        if not names:
            raise SystemExit('No highlights/full_logcat found in ZIP')
        for name in sorted(names):
            text = z.read(name).decode('utf-8', 'replace')
            lines = text.splitlines()
            counts = collections.Counter({tag:sum(tag in l for l in lines) for tag in TAGS})
            print('\n' + '=' * 70 + '\n' + name)
            for tag,count in counts.items():
                if count: print(f'  {count:5d}  {tag}')
            for title, test in [
                ('Rejected casting targets', lambda l: 'REPAIR178: CAST_PREFLIGHT' in l and 'exact=0' in l),
                ('Unmatched early handoffs',lambda l: 'PICK3_REVIVE_EARLY_HANDOFF' in l and 'result=SKIP' in l),
                ('Uncorrected secondary creators',lambda l: 'PICK3_REVIVE_SECONDARY_CREATOR' in l and 'action=SKIP' in l),
                ('Uncommitted primary binds',lambda l: 'PICK3_REVIVE_PRIMARY_BIND' in l and ('NO_MATCH' in l or 'SKIP' in l)),
            ]:
                examples = [l for l in lines if test(l)]
                if examples:
                    print(f'\n{title}: {len(examples)}')
                    for ex in examples[:8]:print('  '+ex[:450])
            print('\nNOTE: stock callback success is NOT proof of server acceptance or actual board placement.')

if __name__ == '__main__':
    if len(sys.argv) != 2:
        raise SystemExit('Usage: py tests/AnalyzeRevive178.py PvP-Test.zip')
    main(sys.argv[1])
