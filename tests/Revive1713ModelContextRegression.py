from pathlib import Path
p=Path(__file__).resolve().parents[1]/'inc/pick3/PickFromThreeSubmission.hpp'
s=p.read_text(encoding='utf-8')
checks={
 'stock decider model field':'gameStateUtilsEntityModelField = decider.GetField("<EntityModel>k__BackingField")' in s,
 'generation model scoped':'ScopedGenerationReviveRecognition generationScope(self);' in s,
 'proof model exact':'generationScope.model == generationModel' in s,
 'proof rooted':'CaptureRoot(reviveLegalPlayModelRoot,generationModel);' in s,
 'proof fallback guard':'!Target(reviveLegalPlayModelRoot)' in s,
 'drag model scoped':'ScopedReviveDragLookup dragLookup(proofCard);' in s,
 'no dynamic Entity Components lookup in GUID helper': 'BNM::Class(entity).GetField("Components")' not in s[s.index('inline int TrickGenerationGuid(int id) {'):s.index('// Strict generic entry-point predicate')],
 'binder cleanup':'Drop(reviveLegalPlayModelRoot);\n            deferredValidatedPlay={};' in s,
 'match cleanup':'Drop(reviveLegalPlayModelRoot);\n    deferredValidatedPlay={};' in s,
 'reason-specific traces':'REPAIR1713: DRAG_MODEL' in s and 'REPAIR1713: GENERATION_IDENTITY' in s,
}
for name,success in checks.items():print(('PASS' if success else 'FAIL')+': '+name)
assert all(checks.values())
