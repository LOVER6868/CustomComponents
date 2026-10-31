# Unearth secondary selection / effect queue audit — 2026-10-28

Starting point: UnearthEffectHandoffAudit playable build.

This iteration is observational, not a speculative gameplay fix.

Adds `UNEARCH_EFFECT_HANDOFF: phase=RUNTIME_CREATED` to report the actual primary/secondary IDs of the runtime effect returned by the descriptor.

Adds `UNEARCH_SECONDARY_QUEUE` snapshots at the modal completion boundaries BEFORE_DETACH, AFTER_DETACH, AFTER_COMPLETE, AFTER_MODAL_CLEANUP, listing up to five entries and whether each is a ChooseTargetBlockingEffect. No managed Peek, queue mutation, forced continuation, or lane substitution is performed.

Test only in PvE. A missing PROCESS_ENTER with a runtime effect created and no subsequent secondary blocking entry suggests a transition or effect-scheduling issue. A secondary blocking effect appearing then disappearing across snapshots indicates a cleanup interaction; compare the exact stages. These traces alone do not establish a fix.
