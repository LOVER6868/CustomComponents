# PickFromThree crash-isolation diagnostic build

The same APK supports five modes. It reads a single integer from
`/sdcard/Android/data/com.ea.gp.pvzheroes/files/pick3_diag_mode.txt`
when `Open()` starts. The default when the file does not exist is **0**.

- **0:** static popup, no native CardView factory probing/loading
- **1:** static popup plus native factory metadata probing; no avatar factory request
- **2:** one native card factory request, no RenderTexture stage
- **3:** one native card factory request with RT stage
- **4:** sequential native card requests and RTs for all three options

The native subsystem is initialized at module startup even in mode 0 so the
mode may change without APK rebuild. This means mode 0 is *not* an entirely
native-code-free baseline; startup initialization still happens.

## PowerShell / ADB

```powershell
adb shell mkdir -p /sdcard/Android/data/com.ea.gp.pvzheroes/files
"0" | Set-Content -NoNewline -Encoding ascii "$env:TEMP\pick3_diag_mode.txt"
adb push "$env:TEMP\pick3_diag_mode.txt" /sdcard/Android/data/com.ea.gp.pvzheroes/files/pick3_diag_mode.txt
```

To test next mode, change `"0"` to `"1"` (through `"4"`), push, then open
a **new popup**. If Android restricts access to that directory, use your
device's file manager or game debug build. Not all devices expose scoped
storage through ADB.

A marker is written with `PENDING` and fsynced before synchronous artwork
preload and before the native `createCard` invoke. The marker changes to `OK`
after the call returns. On startup, a `PENDING` marker forces mode 0 for the
lifetime of the game process. After collecting your crash log, remove the
marker using `adb shell rm /sdcard/Android/data/com.ea.gp.pvzheroes/files/pick3_diag_marker.txt`, then restart the game to re-enable diagnostics.

This marker is only a diagnostic clue. Abrupt termination anywhere between
writes, including power loss, can leave a false positive. A fully asynchronous
native callback may crash later *after* the marker was cleared. Logcat remains
necessary. Modes 2-4 are unsafe experimental operations and must be tested
one at a time, with no automatic escalation after a crash.

Logcat checkpoints: `CustomComponents/Pick3/DIAG: ENTER`, `EXIT`, `MODE`,
`POPUP_START`, `RECOVERY`. If app crashes inside the `createCard` managed
invocation, the last line is likely `ENTER phase=factory-createCard`.

The existing animationExperiment code still coordinates loads and rendering;
this diagnostic build does not rewrite its lifecycle. No Android compilation
or on-device verification has been performed.
