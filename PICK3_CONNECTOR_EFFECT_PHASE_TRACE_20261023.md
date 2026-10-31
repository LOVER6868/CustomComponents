# Pick3 connector, effect retirement, and visual targeting-phase trace (2026-10-23)

## Purpose

This patch extends the existing read-only `Pick3NetworkTrace.hpp`. It does **not**
fix gameplay, alter a `Play`, force a completion, resend a packet, mutate an
effect queue, or advance a visual subturn. The latest trace already showed
`SerializePlay` returning non-null, `ClientAuthoritativePlaySimulator` and a
local success callback. The new events narrow the remaining uncertainty.

### New diagnostic stages

- `CONNECTOR_SEND_ENTER` / `CONNECTOR_SEND_RETURN`: concrete `PvPGameConnectorImpl.Send(string,byte[])` invocation and the returned request ID. This is **not** proof of delivery or server approval.
- `CONNECTOR_POST_WITH_HANDLERS_ENTER` / `_RETURN`: concrete `PostWithHandlers` (used by the four-argument `Send` overload and by the two-argument path). The four-argument generated `Send` forwarding stub is **not hooked** because of its small instruction body.
- `POST_REQUEST_SUCCESS_ENTER` / `_RETURN`: the `PvpPlayPoster.PvpPlayRequest.CompleteWithSuccess` transport callback. Correlated to the outbound request ID when possible; it is **not proof** that the opponent's engine understands or applied the ability.
- `QUEUE_DOSHIFT_RETURN`: `EffectQueue.DoShift` changed the queue; includes its live front, counts and last matched Pick3 effect/group before/after. Existing `EffectQueue.Shift` instrumentation remains, but previously reported `queueShift=0`; `DoShift` is a separate fallback.
- `COMPLETE_EFFECT_RETURN`: `EffectCollectionProcessor.CompleteEffect` read-only before/after queue snapshot. This directly observes the engine's completion boundary when the active collection is an `EffectQueue`.
- `PROCESS_EFFECTS_RETURN`: a broader engine processing boundary in case no individual completion event is visible.
- `VISUAL_SUBTURN_CHANGED`: transitions captured by the existing game-input observer (no fragile hook on an 8-byte setter or `EndSubTurn`). `current=1` is local target phase; `current=0` means no subturn. This is *presentation state*, not the engine's blocking-effect queue.

The **startup** `NETTRACE: hooks` line now includes:
`connector`, `postHandlers`, `requestOK`, `doShift`, `complete`, `process`, and legacy `queueShift`.
`1` means the hook trampoline was installed, **not** that the event actually ran.

### Switch (same path as previous build)

`/sdcard/Android/data/com.ea.gp.pvzheroes/files/pick3_net_trace.txt`

From Windows PowerShell:

```powershell
# Focus on custom Pick3 events (default suggested for gameplay tests)
adb shell "echo 1 > /sdcard/Android/data/com.ea.gp.pvzheroes/files/pick3_net_trace.txt"

# Include stock SelectTarget plays so their send/completion can be compared
adb shell "echo 2 > /sdcard/Android/data/com.ea.gp.pvzheroes/files/pick3_net_trace.txt"

# All plays/queues: verbose; only use during a short reproducible test
adb shell "echo 3 > /sdcard/Android/data/com.ea.gp.pvzheroes/files/pick3_net_trace.txt"

# Stop additional tracing without rebuilding the APK
adb shell "echo 0 > /sdcard/Android/data/com.ea.gp.pvzheroes/files/pick3_net_trace.txt"
```

The mode file is reread at most once per second during instrumented calls.
The code defaults to `0` if the file is absent or invalid. Hooks are still
installed in off mode, but no additional network/effect payload logging occurs.

To capture a test:

```powershell
adb logcat -c
adb logcat -v time -s Sprig:V > pick3_trace_log.txt
```

Perform one custom Pick3 selection in PvP; then, if possible, one **stock**
PvP target selection. Allow the game to progress to the next turn and stop
logcat with Ctrl+C. Keep the complete log. For unexpected SIGSEGV, also collect
`adb logcat -b crash -d > pvz_crash_log.txt` (not a broad unrelated Android boot log).

## Scope and technical rationale

- Read-only native hooks at `PvPGameConnectorImpl.Send(string,byte[])`
  (RVA 0x2FE2C74), `PostWithHandlers` (0x2FE2D34), nested
  `PvpPlayRequest.CompleteWithSuccess` (0x2FE52C0), `EffectQueue.DoShift`
  (0x227FD70), `EffectCollectionProcessor.CompleteEffect` (0x2280CF4) and
  `ProcessEffects` (0x2280B9C). Exact versions must match supplied dump and APK.
- Connector request IDs come from a 4-byte `PvpPlayRequestId.UniqueId`. The
  tracer records only an outgoing byte-array **length**, never packet contents.
- Thread-local callback identity associates the connector call with the actual
  currently executing simulator handler, avoiding attributing another thread's
  unrelated send to the Pick3 transaction.
- Queue snapshots read native `EffectQueue.Effects`, `ResponseEffects`,
  `GroupResponseEffects`, `CurrentGroupId`, and the live `EffectEntityId` and
  `EffectGroupId` fields without calling `Resolve` or other mutating methods.
  The `matchList` field is 0=Effects, 1=ResponseEffects, 2=GroupResponseEffects,
  -1=not found. Snapshot matching is best-effort; if no `COMPLETE_EFFECT` event is printed,
  inspect the startup hook flags and the `mode` switch before drawing conclusions.
- Visual subturn observations use the existing `GameInputStateHolder.Update`
  observer in `PickFromThreeSubmission.hpp`. The short native
  `VisualTurnPhaseModel.StartSubTurn` / `EndSubTurn` functions remain unhooked.

## Validation / limitations

- C++20 **host syntax check** for `Pick3NetworkTrace.hpp` and
  `PickFromThreeSubmission.hpp` passed against the previously bundled BNM
  headers. BNM's existing warnings about non-standard-layout custom fields
  remain.
- **Not** compiled into an Android APK and **not** tested on-device.
- Server/opponent acceptance still requires gameplay observations and, if
  available, server-side diagnostics. HTTP POST success does not imply a custom
  effect was simulated by the other side.
- All source apart from the modified trace header, its existing callsite in
  `PickFromThreeSubmission.hpp`, and this document remains unchanged.
