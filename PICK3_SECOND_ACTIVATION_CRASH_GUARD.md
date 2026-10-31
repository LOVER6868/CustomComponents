# Second-activation submission guard (2026-10-24)

Observed 2026-10-23 21:07:24: second PvE popup 35 eligible physical entities, 12 GUIDs, offers 3; target entity=102 GUID=704. Supplied Sprig-only log ends immediately after TARGET_TRACE. It does not contain native crash frames, so root cause is not established.

Change: in LocalBlockingEffectMatches, validate the front managed object's BlockingEffect type *before* reading entity/group fields; catch managed exceptions around those field reads. Added SUBMIT_CHECKPOINT events after target selection, before local-effect verification, and after verification. No changes to native art, highlight handling, sampler, materializer, DB5 PvP, or card transport.

Validation: textual diff and archive integrity only. This is an instrumented defensive test, not a confirmed crash fix. To capture the next native failure: adb logcat -v threadtime -b main -b system -b crash > pick3_full_live.txt; stop with Ctrl+C after the crash.
