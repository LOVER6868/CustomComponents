Pick3 PvP owner gate + phase-resume record tracing (2026-10-24)
Baseline: Pick3_IdentityController_full_20261024.zip (current observed PvE-working source).

CODE CHANGES:
- PickFromThreePreviewSystem.hpp: published the CURRENT holder effect in enabled remote PvP, not merely in local PvE; passed local player id as mandatory owner for PvP. Stock holder 6/7 and rooted EntityModel remain prerequisites.
- PickFromThreeTargetBridge.hpp: pending cache matching now filters by requested local owner for remote PvP (both primary-model and verified-clone lookup). Opponent effects will not open a local modal; current submission PvpOwnerAllowed still applies.
- PhaseRecordTrace.hpp: read-only detours at LocalSelectTargetService.FinishChooseTargetPhase, PhaseStartRecordProcessor.ProcessChooseTargetPhaseEndRecord, ProcessPhaseResumeRecord, and DispatchPhaseResumeSignal. Each original called exactly once. No synthetic phase resume, effects, FSM transitions or retries.
- PickFromThreeSubmission.hpp: registered PhaseRecordTrace after existing target/button hooks.

IMPORTANT: This is a confirmed PvP activation code-path correction but NOT a proven runtime fix. It does not claim Surprise/Done button is repaired. The native assembly shows the phase resume signal is queued through EffectAnimationFactory/EffectAnimationQueue on ProcessPhaseResumeRecord; the missing event can be due to record generation, enqueue, or animation dispatch. Read-only traces locate which one. Use this build to reproduce PvP and double Surprise manual effects and capture PHASE_RECORD / STATE_TRACE / RESUME_TRACE / IDENTITY logs. Leave network deck transport for the next focused phase.

The PvP card-pool limitation (virtualTransport=DISABLED, only live card entries) has NOT been modified. Avoid shipping virtual IDs in PvP without verified stock transport support on both clients.
