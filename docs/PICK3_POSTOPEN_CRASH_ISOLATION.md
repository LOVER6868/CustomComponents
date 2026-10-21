# PickFromThree post-open crash isolation (diagnostic only)

Applies to the native crash observed ~1 second after `Pick3/PERF: OPEN` (UnityMain, null-address 0x38, BNM::TryInvoke). The uploaded libsprig.so is stripped, so the stack offsets cannot be assigned to source functions directly.

This patch creates a separate ADB selector. Leave startup stage `9`, Pick3 hooks `1`, combat hooks `1`, metadata isolation `4` while isolating. Keep `pick3_diag_mode.txt` at `0` unless intentionally testing experimental native avatars.

Write a single ASCII digit to `/sdcard/Android/data/com.ea.gp.pvzheroes/files/pick3_postopen_isolation.txt`, force-stop the game, then launch and play GUID 704:

- `0`: normal post-open behavior (expected prior crash).
- `1`: stop the *open popup's* per-frame `Tick()` path, but let async asset and inspection services continue. Popup input, animation, close handling may not work.
- `2`: stop both the open popup's per-frame `Tick()` path and its async asset/inspection servicing. This will leave a static/noninteractive popup; force-stop to close.

Logcat should show `CustomComponents/Pick3/POSTOPEN_ISO: mode=N`. If mode 1 crashes but mode 2 does not, focus on async asset callbacks; if mode 1 avoids the crash, focus on the per-frame UI path; if both crash, investigate Unity-driven component lifecycle outside this dispatcher. Do not treat a no-crash static popup as a working effect.

PowerShell:

```powershell
$base = '/sdcard/Android/data/com.ea.gp.pvzheroes/files'
$local = "$env:TEMP\pick3_postopen_isolation.txt"
[IO.File]::WriteAllText($local, '1', [Text.Encoding]::ASCII)
adb push $local "$base/pick3_postopen_isolation.txt"
adb shell cat "$base/pick3_postopen_isolation.txt"
adb shell am force-stop com.ea.gp.pvzheroes
```

Tests: mode 1 first, only try mode 2 if mode 1 still crashes. Capture full logcat and report whether the game crashed. No Android build or gameplay verification was performed here.
