# Unearth secondary-choice native branch audit (2026-10-28)

Baseline: SourceGuard + QueueFull, no SequentialPrimary redirect.

Read-only hooks added to PvZCards.Core.Systems.Utilities:
- ChooseTargetEffectCreator.ShouldAddChooseTargetEffects(int, TargetEntityFilter, List<int>)
- ChooseTargetEffectCreator.ShouldAddChooseTargetEffects(TargetEntityFilter, ICollection<int>)
- SecondaryTargetEffectCreator.AddChooseSecondaryTargetEffects(Entity, List<Effect>, EntityQueryContext)
- ChooseTargetEffectCreator.CreateChooseTargetEffects(Entity, TargetingPhase, EntityQueryContext)

All original methods are called exactly once, arguments unchanged, results unchanged. Counts are read from List/PoolableList managed layout with basic bounds checks. No queue mutations or manual UI.

Expected decision point: UNEARCH_CHOOSE_DECISION EXIT overload=3 choose=1/0. If true, look for UNEARCH_CHOOSE_BUILD ADD_ENTER, CREATE_ENTER phase=1, CREATE_EXIT count and ADD_EXIT. Compare traces before and after PickFromThree acceptance.

Limitations: decision entry is correlated with existing trickSecondaryFocusGuid (from dragged card) and call-depth, not proven card ownership. An absent trace may mean the relevant hook did not resolve or the method was bypassed/inlined. The game's actual behavior must be tested on Android; archive check does not establish compilation.
