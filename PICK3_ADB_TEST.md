# PickFromThree ADB native factory diagnostic

Install this source change once and rebuild the APK once. Thereafter modify the control file with adb, then trigger a new PickFromThree popup (force-stop/restart for clean runs).

`pick3_native_test.txt` accepts four space-separated integers:

`overload avatar inHand inspection`

- overload 0: original four-argument CreateCardAsync(config, explicit custom parent, delegate)
- overload 1: three-argument CreateCardAsync(config, delegate), which uses the factory's injected RootView internally. The renderer still uses its separate staging object.
- avatar 0/1: request card avatar assets
- inHand 0/1: request in-hand card sprite assets
- inspection 0/1: call ShowForInspection after callback is processed

Default if file absent: `0 1 1 1` (legacy behavior). Suggested first test: `1 1 0 0`.

The file is read on each new card request. `pick3_factory_probe.txt=6` still limits to one native avatar; `7` requests three. `pick3_factory_di_stage.txt=5` allows creation.

PowerShell example (no Out-File):

```powershell
$base='/sdcard/Android/data/com.ea.gp.pvzheroes/files'
adb shell am force-stop com.ea.gp.pvzheroes
adb shell "echo '1 1 0 0' > $base/pick3_native_test.txt"
adb shell "echo 6 > $base/pick3_factory_probe.txt"
adb shell "echo 5 > $base/pick3_factory_di_stage.txt"
adb shell "cat $base/pick3_native_test.txt"
# Start game, trigger PickFromThree, then:
adb logcat -d -s Sprig:I '*:S'
```

To test inspection afterward: `1 1 0 1`. To test sprite fallback: `1 1 1 0`. To return to static-only mode: set factory probe to `4`.

This is an experimental diagnostic, not a verified crash fix. If the crash occurs before CALLBACK_ENTRY, changing the inspection flag cannot prevent it. The factory three-arg overload shares an internal implementation with the four-arg overload, so it may still crash. Never assume successful factory return means successful async loading.
