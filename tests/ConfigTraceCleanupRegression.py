#!/usr/bin/env python3
"""Source-level invariant check for the config / trace cleanup (not Android runtime)."""
from pathlib import Path
import configparser
import re

ROOT = Path(__file__).resolve().parents[1]
CONFIG = ROOT / 'config.ini'
MAIN = (ROOT / 'main.cpp').read_text()
TGT = (ROOT / 'inc/pick3/TargetButtonTrace.hpp').read_text()
PHASE = (ROOT / 'inc/pick3/PhaseRecordTrace.hpp').read_text()
TRACE = (ROOT / 'inc/diagnostics/TraceVerbosity.hpp').read_text()

cfg = configparser.ConfigParser(interpolation=None)
cfg.optionxform = str
assert cfg.read(CONFIG)
all_source = '\n'.join(p.read_text(errors='ignore') for p in ROOT.rglob('*.hpp')) + '\n' + MAIN
read_pairs = set(re.findall(
    r'BLOOM_CONFIG(?:_INT|_BOOL|_FLOAT|_STRING)?\s*\(\s*"([^"]+)"\s*,\s*"([^"]+)"',
    all_source))
unused = [(section, key) for section in cfg.sections() for key in cfg[section]
          if (section, key) not in read_pairs]
assert not unused, f'ignored config keys: {unused}'
assert sum(len(cfg[s]) for s in cfg.sections()) == 147
assert cfg['Pick3Diagnostics']['PredictionCloneAudit'] == 'false'
assert cfg['PvpDeckPrepopulation']['ExecuteFullDeckLab'] == 'true'
assert cfg['PvpDeckPrepopulation']['ExpectedDeckFingerprint'] == 'auto'
assert cfg['Pick3LifecycleRepair']['GuardedStockButtonRecovery'] == 'true'
assert cfg['Pick3LifecycleRepair']['SuspendInterruptedPick3'] == 'true'
assert cfg['PickFromThree']['SubmitInPVP'] == 'true'
assert 'Pick3Experimental' not in cfg
assert 'CUSTOMCOMPONENTS_VERBOSE_HOOK_TRACE 0' in TRACE
assert 'CC_HOOK_TRACE' in TGT and 'CC_HOOK_TRACE' in PHASE
assert 'if constexpr (CUSTOMCOMPONENTS_VERBOSE_HOOK_TRACE == 0) return;' in TGT
# The stock calls and functional side effects are NOT disabled with tracing.
for token in ('oldChoose(s,m)', 'oldResume(s,m)', 'oldInput(s,v,m)',
              'oldBind(s,v,m)', 'oldCare(s,m)', 'oldStateFromEffect(s,e,m)',
              'RememberChoose(s)', 'handoffAwaitingDiscovery=false',
              'oldFirst(s,m)', 'TryRecoverAfterCompletedLocalPhase('):
    assert token in TGT, token
for counter in ('endCount', 'resumeRecordCount', 'dispatchCount', 'finishCount'):
    assert re.search(r'\b' + counter + r'\.fetch_add\(1\)', PHASE), counter
for stock_call in ('originalEnd(processor, record, method)',
                   'originalResumeRecord(processor, record, method)',
                   'originalDispatch(processor, method)',
                   'originalFinish(service, blockingEffect, method)'):
    assert stock_call in PHASE, stock_call
assert 'LOG_INFO("CustomComponents/PHASE_RECORD: SETUP' in PHASE
assert 'LOG_INFO("CustomComponents/BUTTON_RECOVERY:' in TGT
print('Config/trace cleanup checks: PASS (147 active keys, functional counters, hooks, safe debug switch)')
