# Configuration and trace cleanup — October 26, 2026

Based on the game-tested **Review Step 1** build. This is a maintenance patch, not a gameplay feature change.

## config.ini

- Reorganized comments: one concise, purpose-driven comment per section, retaining important coordinate, timing, gameplay, and debugging notes. The file is **266 → 221 lines**.
- Removed **10 obsolete, unread keys**, including the **whole `[Pick3Experimental]` section**. They had no C++ configuration readers and already had no runtime effect.
- Kept **all 146 actively read config keys**, including all UI tuning, full-deck materialization flags/fingerprint, native rendering controls, Stage-8 isolation, and startup diagnostics. No active value changes except below.
- Changed `[Pick3Diagnostics] PredictionCloneAudit` from `true` to `false`. It installs only a passive diagnostic hook; developers can set it back to `true` when investigating prediction/clone behavior.
- Did not remove the experimental one/two-card lab controls: they remain valuable for regression isolation. Did not generalize the full-deck fingerprint.

Removed keys (previously ignored):
`Pick3LifecycleRepair.OneShotFirstPickRebind`, `Pick3Experimental.AllowUnverifiedPvp`, `Pick3Experimental.AllowNativeCardViewFactory`, `Pick3CardViewCamera.FighterCameraYOffsetPercent`, `Pick3CardViewCamera.OtherCameraYOffsetPercent`, `Pick3CardViewCamera.AdaptiveFraming`, `Pick3CardViewCamera.AdaptiveZoomMaxPercent`, `Pick3GameOwnedAssets.LogCaptures`, `Pick3OwnedUnityFS.PrewarmStaticUI`, `Pick3OwnedUnityFS.PrewarmFallbackSprites`.

## High-frequency hook tracing

- `TargetButtonTrace.hpp`: per-frame `STATE_TRACE`, `LIFECYCLE`, button, phase-care, and handoff log lines are suppressed by default. The detours still run and forward to stock methods, and the guarded stock button recovery remains enabled.
- `PhaseRecordTrace.hpp`: per-record ENTER/EXIT logs are suppressed by default. Its **four atomic counters are explicitly incremented outside the logging code** and thus remain operational for stock completion/recovery detection.
- `TargetButtonTrace::Snapshot` skips diagnostic-only reflection work when tracing is off. The FSM state discovery still occurs on demand when actual recovery needs it.
- Startup readiness, setup diagnostics, recovery milestones, materialization results, selection results and errors remain logged.

For temporary deep lifecycle debugging, compile with `-DCUSTOMCOMPONENTS_VERBOSE_HOOK_TRACE=1`; default is `0` without adding yet another config.ini option. To reinstate read-only prediction cloning audit, set `PredictionCloneAudit=true`.

## Intentional non-changes

No changes to CardDatabase PvP, Relic Hunter, Fisherman/InDeck membership, PickFromThree candidate/weight/submission logic, full-deck stock materializer, networking, or draw/transfer behavior. Sow Magic Beans adding four virtual entries is outside this cleanup.

## Validation

Run `python tests/Step1ConfigInitializationRegression.py` and `python tests/ConfigTraceCleanupRegression.py`, build all five C++ regression tests, and compile `main.cpp` both normally and with `CUSTOMCOMPONENTS_VERBOSE_HOOK_TRACE=1`. These are host-side checks only; a device regression check remains necessary.
