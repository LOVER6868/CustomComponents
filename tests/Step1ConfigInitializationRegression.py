#!/usr/bin/env python3
"""Static regression guard for Step 1 config and initialization wiring.

Run: python tests/Step1ConfigInitializationRegression.py
Not an Android/IL2CPP runtime test. Checks all named config keys and the
safety ordering of the patch without depending on an Android toolchain.
"""
from __future__ import annotations

import configparser
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
MAIN = (ROOT / 'main.cpp').read_text(encoding='utf-8')
BRIDGE = (ROOT / 'inc/core/CustomFieldBridgeSystem.hpp').read_text(encoding='utf-8')
CONF = configparser.ConfigParser(interpolation=None)
CONF.optionxform = str
assert CONF.read(ROOT / 'config.ini'), 'missing config.ini'

READS = {(section, key): int(default) for section, key, default in re.findall(
    r'BLOOM_CONFIG_INT\("([^"]+)",\s*"([^"]+)",\s*(-?\d+)\)', MAIN
)}

for section, expected_count in [('Pick3RewardLayout', 21), ('Pick3StatText', 17)]:
    assert section in CONF
    settings = dict(CONF[section]); assert len(settings) == expected_count, (section, len(settings))
    for key, value in settings.items():
        assert (section, key) in READS, f'config value ignored: [{section}] {key}'
        assert READS[section, key] == int(value), (
            f'{section}.{key}: compiled fallback {READS[section, key]} '
            f'differs from packaged baseline {value}'
        )

assert '"Pick3Layout"' not in MAIN
assert '"Pick3Text"' not in MAIN
assert 'FontSizePercent' not in CONF['Pick3Description']
assert READS['Pick3StatText', 'DescriptionSizePercent'] == 160

for key, desired in [('StartupCutoff', -1), ('CombatHookMode', 3), ('Pick3HookMode', -1)]:
    assert CONF.getint('StartupDiagnostics', key) == desired
    assert READS['StartupDiagnostics', key] == desired
for filename in ('customcomponents_startup_stage.txt',
                 'customcomponents_combat_hooks_mode.txt',
                 'customcomponents_pick3_hooks_mode.txt'):
    assert filename in MAIN, filename
assert 'ReadControlFile(' in MAIN and 'std::strtol' in MAIN
assert 'const bool stage8CandidateHooks = pick3HookMode < 0' in MAIN
assert 'BLOOM_CONFIG_BOOL("Pick3Stage8Isolation", "CandidateBridge", true)' in MAIN
assert 'BLOOM_CONFIG_BOOL("Pick3Stage8Isolation", "SubmissionHooks", true)' in MAIN

init = MAIN.index('CustomComponents::Pick3::TargetBridge::Initialize(true, installSubmission)')
submission = MAIN.index('CustomComponents::Pick3::Submission::Initialize(')
assert init < submission
assert 'installSubmission && candidateReady && !CustomComponents::Pick3::Submission::Initialize(' in MAIN
assert 'CustomComponents::Pick3::TargetBridge::enabled' in MAIN
assert '                candidateReady,\n                installSubmission && candidateReady && ' in MAIN

preflight = BRIDGE.index('auto entityDeepCopy = entityClass.GetMethod("DeepCopyFrom", 2);')
hook = BRIDGE.index('BNM::BasicHook(setProperties, SetObjectPropertiesHook')
assert preflight < hook, 'DeepCopy metadata must be checked before installing first hook'
assert BRIDGE.index('auto deserializeNew = referenceDeserializer.GetMethod("Deserialize", 1);') < hook

print('Step1 config / initialization source checks: PASS (38 UI keys, modes, startup dependencies, field bridge)')
