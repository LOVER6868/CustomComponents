from pathlib import Path
s = (Path(__file__).resolve().parents[1] / 'inc/pick3/PickFromThreeSubmission.hpp').read_text()
a=s.index('inline void Pick3AddEffectsWithSecondary(')
b=s.index('inline void UnearthSecondaryContextAudit(', a)
hook=s[a:b]
assert 'stage=HOOK_ENTER' in hook
assert hook.index('stage=HOOK_ENTER') < hook.index('if (scope.effect > 0 && data && data->effectEntity)')
for marker in ('stage=HOOK_CLASSIFY', 'stage=CREATOR_RETURN', 'NO_COMMIT_SCOPE', 'NO_EFFECT_DATA', 'EFFECT_MISMATCH', 'NON_REVIVE_EFFECT', 'MATCHED'):
    assert marker in hook, marker
assert 'creatorSamples1724 < 64' in hook
assert hook.count('originalPick3AddEffectsWithSecondary(self, data, effects, method)') == 1
assert hook.index('stage=CREATOR_RETURN') > hook.index('originalPick3AddEffectsWithSecondary(self, data, effects, method)')
assert 'const bool canCorrect = verified && zoneVerified && laneIds &&' in hook
assert 'if (canCorrect) data->primaryId = selected;' in hook
start = s.index('originalLocalSelectProcess(self,play,method);')
end = s.index('if (traceSelection) --CustomComponents::Systems::CustomTriggerSystem::pickFromThreeServiceDepth;', start)
post=s[start:end]
for marker in ('stage=POST_PROCESS_EFFECT','stage=POST_PROCESS_SUMMARY','GroupResponseEffects','ResponseEffects','matched=%d','postSamples1724'):
    assert marker in post, marker
assert 'data->primaryId =' not in post
print('1.7.24 diagnostic boundary source regression passed')
