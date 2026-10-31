# PickFromThree ADB network and effect-lifecycle tracing (2026-10-23)

## Scope

Read-only diagnostics added to the October 23 **live-effect-preflight** source baseline. **No targeting rules, `Play` fields, network traffic, service dispatches, queue retirements, animations or UI behavior are changed.** This is a diagnostic build, not a PvP gameplay fix. It instruments the known, adequately sized ARM64 method bodies; the adjacent 8-byte `RemotePlayService.HandlePlaySuccess` and `HandlePlayFailure` methods are deliberately NOT detoured.

Switch file (external app-specific storage, like the existing art diagnostic switches):

`/sdcard/Android/data/com.ea.gp.pvzheroes/files/pick3_net_trace.txt`

Values:
- `0` or missing: no detailed tracing (default). Hooks remain installed, but are passive.
- `1`: trace only the outgoing `Play` objects created by this PickFromThree implementation and their associated simulator callbacks. Recommended first.
- `2`: include ordinary stock `SelectTarget` plays (type 6) so they can be compared to Pick3. Recommended for command-lifecycle comparisons.
- `3`: include all play types and callbacks. Very noisy; use briefly.

### PowerShell + ADB

Enable recommended first-pass level without rebuilding or restarting the app:

```powershell
adb shell "echo 1 > /sdcard/Android/data/com.ea.gp.pvzheroes/files/pick3_net_trace.txt"
```

Compare against ordinary stock targeting:

```powershell
adb shell "echo 2 > /sdcard/Android/data/com.ea.gp.pvzheroes/files/pick3_net_trace.txt"
```

Turn off tracing immediately:

```powershell
adb shell "echo 0 > /sdcard/Android/data/com.ea.gp.pvzheroes/files/pick3_net_trace.txt"
```

If shell redirection is denied on your Android release, put `1` in a local file and try:

```powershell
Set-Content -Path .\pick3_net_trace.txt -Value 1 -NoNewline
adb push .\pick3_net_trace.txt /sdcard/Android/data/com.ea.gp.pvzheroes/files/pick3_net_trace.txt
```

Log to a file while reproducing PvP Saucer Squash then PvE Fisherman:

```powershell
adb logcat -c
adb logcat -v time -s Sprig:V > .\pick3_trace_log.txt
```

The switch is checked on the next instrumented call (at most once per second). A `NETTRACE: SWITCH` log entry shows when the new value was observed; absence usually indicates a file-access issue or that no traced code has executed yet. Writing 0 also stops the new `NETTRACE` lines; existing baseline `POOL_BOUNDARY` and `HANDOFF` logs remain unchanged.

### Stage names

- `PICK3_CHOICE_ENTER`, `PICK3_PREFLIGHT_OK`, `PICK3_PREPARED`, `PICK3_DISPATCH_ENTER`, `PICK3_DISPATCH_RETURN`
- `REMOTE_PROCESS_ENTER` / `_RETURN`, `DECIDER_ENTER` / `_RETURN` (logs selected concrete simulator type)
- `CLIENT_SIMULATE_ENTER` / `_RETURN` or `SERVER_SIMULATE_ENTER` / `_RETURN`
- `PREDICTION_SUCCESS`, `PREDICTION_FAILURE`, `SERIALIZE_ENTER`, `SERIALIZE_RETURN`
- `CLIENT_SERVICE_SUCCESS`, `CLIENT_SERVICE_FAILURE`, `LOCAL_SERVICE_SUCCESS`, `LOCAL_SERVICE_FAILURE`, `SERVER_SUCCESS`, `SERVER_FAILURE`
- `PICK3_SERVICE_CALLBACK` (may arise from prediction/local service; **not** proof of server acceptance)
- `PICK3_FINISH_TRANSACTION`, `EFFECT_QUEUE_SHIFT` if the exact effect/group is shifted locally

Stages carry a numeric tracing ID and as available the effect/group, type, player, card, order field, and up to three entity IDs. No player login credentials, auth tokens, complete serialized buffers or remote content are logged.

A `SERIALIZE_RETURN` means only that a serializer returned a byte-array object; it is **not** proof of socket transmission or server acceptance. A `SERVER_SUCCESS` stage means the game's matching server-response callback ran, not that the other player's simulation agrees with all custom effects. Some branches may legitimately skip serialization or server callbacks depending on authority mode.

For long diagnostic sessions, `Flight` records are a bounded 48-item ring keyed by `Play*` / simulator identity and copied effect/group IDs. It does not root or dereference an asynchronous `Play*`; match reset clears the ring. If a network simulator clones a `Play` before tracing begins or reuses the same object for overlapping requests, its stages may appear as unmatched `id=0` (mode 2 helps identify these cases). Tracing does not attempt to repair or replay unmatched events.

## Verified by inspection

Method metadata and RVAs from this game's `dump.cs`; `libil2cpp.so` assembly inspected at the `PlaySimulatorDecider` and short callback methods. Hooks are only installed for discovered methods, and no generated 8-byte setters/callbacks are hooked. Standalone tracing header and the complete submission header passed a host Clang C++20 syntax check with the bundled BNM headers (`-DNDEBUG`, Android logging stub). Android/NDK linking and gameplay behavior **not tested** here.
