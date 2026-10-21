# PickFromThree: metadata/UI crash isolation

Observed on device: startup Stage 9, combat mode 1. Pick3 hook modes 1 and 3 crash when card GUID 704 triggers the popup, while hook mode 0 does not. Mode 1 logs stop around `CLASS_SHELL` during `UpdateCardMetadata()`. This does **not** prove the rim call is the fault; later stock icon and stat label calls are unobserved.

This patch adds a per-popup selector independent of `customcomponents_pick3_hooks.txt` and `pick3_diag_mode.txt`:

`/sdcard/Android/data/com.ea.gp.pvzheroes/files/pick3_metadata_isolation.txt`

| Value | Behavior |
|---|---|
| 0 (default) | Original behavior; extra before/after trace only |
| 1 | Skip `ApplyClassBackground()`; leave stock icons and stat labels active |
| 2 | Leave class rim enabled; skip game-owned attack/health icon replacement |
| 3 | Skip both class rim updates and stock icon replacement |
| 4 | Bypass `UpdateCardMetadata()` entirely, preserving popup shell and other systems; names/cost/stats remain placeholders |

The selector is read on each new popup, not at startup. These are **diagnostic modes**, not repairs. Avoid mode 0 with CandidateBridge enabled until crashes are understood. Keep Stage 9 and Combat mode 1 unchanged during comparisons. Begin with Pick3 hook mode 1 and metadata isolation 3; if stable, compare 1 vs 2, then 4 if needed. Only alter one file value between runs. A crash in mode 3 may still be in cost badge, text formatting or stat label creation.

PowerShell example (after rebuilding/installing the patched source):

```powershell
$remote="/sdcard/Android/data/com.ea.gp.pvzheroes/files/pick3_metadata_isolation.txt"
$local="$env:TEMP\pick3_metadata_isolation.txt"
[System.IO.File]::WriteAllText($local,"3",[System.Text.Encoding]::ASCII)
adb push $local $remote
adb shell cat $remote
adb shell am force-stop com.ea.gp.pvzheroes
```

`adb logcat -d -s Sprig:* | Select-String "META_ISO|CLASS_SHELL|Pick3"`

If you just want stability, keep Pick3 hook mode 0. This patch does not claim to solve source-entity lifetime (`played=stale`) or symbolicate the native `BNM::TryInvoke` SIGSEGV. No Android compilation or device test was performed.
