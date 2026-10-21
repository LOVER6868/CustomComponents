# Pick3 callback/invocation isolation

Install this patch once, then use ADB to choose one of four phases with the **fifth** integer in `pick3_native_test.txt`.

The first four values retain their former meaning: overload, avatar, in-hand sprite, and inspection. The fifth is a diagnostic phase:

- `1`: create/root and configure CardViewConfiguration; stop before constructing callback.
- `2`: create/root receiver and Action<CardView> delegate; stop before calling CardViewFactory.
- `3`: call the receiver's *native C++ OnLoaded implementation* with a null card, without dispatching through the managed delegate; stop before factory. This only checks the C++ callback body, **not** IL2CPP delegate dispatch.
- `0` (or omitted): run the existing factory call (crash risk).

Use **Mode 6**, one card. Do not jump to phase 0 until phases 1–3 have been observed. Neither the patch nor its package proves that phase 0 is safe.

PowerShell (prints directly, no Out-File):

```powershell
$base = '/sdcard/Android/data/com.ea.gp.pvzheroes/files'
adb shell am force-stop com.ea.gp.pvzheroes
adb shell "echo '0 1 0 0 1' > $base/pick3_native_test.txt"
adb shell "echo 6 > $base/pick3_factory_probe.txt"
adb shell "echo 5 > $base/pick3_factory_di_stage.txt"
adb shell "cat $base/pick3_native_test.txt"
```

Trigger PickFromThree and read `adb logcat -d -s Sprig:I '*:S'`. Repeat with final digit `2` and `3`, restarting each time. Phase 1/2/3 deliberately do **not** show a native card; they isolate the preparation path. To restore the normal non-native UI, set `pick3_factory_probe.txt` to `4`.
