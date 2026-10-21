# CustomComponents startup isolation (Android / PowerShell)

This patch retains the baseline source and normal behavior when no cutoff file exists. It adds a monotonic startup cutoff to `main.cpp` (NOT a collection of independent feature toggles; those would violate startup dependencies).

Place a decimal number `0` through `9` or `-1` in:

`/sdcard/Android/data/com.ea.gp.pvzheroes/files/customcomponents_startup_stage.txt`

The module also tries the equivalent `/data/user/0/.../files/` path if external access is unavailable. **Changing the file requires a force-stop/restart.** It is read once when CustomComponents initializes. `-1` or missing file runs the original startup unaltered.

Stages run cumulatively:

| Stage | Most recent initialization region included |
| --- | --- |
| 0 | none (general Enabled check only) |
| 1 | numeric ability and grantable runtime |
| 2 | combat, zone-trigger and presentation systems before deck utilities |
| 3 | deck/discard utilities and lifecycle systems |
| 4 | shared custom field bridge and discard-pile effects |
| 5 | target queries and targeting scope |
| 6 | type and component resolver/constructor |
| 7 | ability registry, enums, serialization, GrantAbility |
| 8 | Pick3 live candidates and submission |
| 9 | Pick3 UI, asset registries and native avatar initialization |

A stage is a **diagnostic early-return**, not a supported playable subset of the mod. Some lower stages will break custom cards or matches by design. Test for getting past the game screen ONLY, and restore stage 9/-1 before regular play.

PowerShell:

```powershell
$stage = 0 # change to 1..9 or -1
$dest = '/sdcard/Android/data/com.ea.gp.pvzheroes/files/customcomponents_startup_stage.txt'
"$stage" | Set-Content -Encoding ascii -NoNewline "$env:TEMP\customcomponents_startup_stage.txt"
adb shell mkdir -p /sdcard/Android/data/com.ea.gp.pvzheroes/files
adb push "$env:TEMP\customcomponents_startup_stage.txt" $dest
adb shell am force-stop com.ea.gp.pvzheroes
adb shell cat $dest
# Open the game manually; then:
adb logcat -d -v threadtime -s Sprig:* | Select-String 'STARTUP_DIAG'
```

If `adb push` fails with permission denied, this path is not usable on that device. Do not assume settings took effect unless `STARTUP_DIAG: using ... cutoff=N` appears in Logcat. Use a permitted game-owned file installation route instead.

Suggested sequence: test 0, 4, 7, 9 to find the failing half, then bisect between the last stable and first unstable stage. Capture multiple launches per stage because startup failure is intermittent. If **stage 0** still crashes frequently, CustomComponents' module initializer functions are not the crash cause on that attempt; it does **not** exclude static constructors or other native code that executes when the library loads.
