#!/usr/bin/env python3
"""Check entity-draw native bindings against the matching game's dump.cs.
Usage: python3 tests/EntityDeck223BindingsRegression.py /path/to/dump.cs
This checks metadata contracts; it does not execute IL2CPP on a device.
"""
from pathlib import Path
import re, sys
root = Path(__file__).resolve().parents[1]
dump = Path(sys.argv[1]).read_text()
source = (root / 'inc/effects/detail/SubsetDrawExecution.inl').read_text()
def body(name):
    # Match class header without assuming inheritance.
    match = re.search(r'^public class ' + re.escape(name) + r'(?=\s|:)[^\n]*', dump, re.M)
    assert match, name
    return dump[match.start():].split('// Namespace:', 1)[0]
contracts = {
 'InstanceManager': ['Acquire(Type type, bool shouldInject = True)', 'Return(Poolable instance)'],
 'EntityFinder': ['.ctor(EntityModel model)', 'FindEntities(EntityQuery query, EntityQueryContext context)'],
 'EffectData': ['CopyFrom(EffectData data)'],
 'EntityQueryContext': ['<Model>k__BackingField', 'SelfId;', 'EffectContext;'],
 'CardFromSubsetFinder': ['<Instances>k__BackingField'],
}
for klass, signatures in contracts.items():
    for signature in signatures:
        assert signature in body(klass), (klass, signature)
assert 'GetMethod("Return",1)' in source
assert 'GetMethod("ReturnToPool",1)' not in source
assert 'std::string_view("query"),std::string_view("context")' in source
assert 'return=%d' in source
print('PASS: entity draw bindings match game metadata; pooled context uses InstanceManager.Return')
