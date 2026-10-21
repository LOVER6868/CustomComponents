CustomComponents Charge/Thrash Hook Isolation (2026-10-20)

This is a diagnostic patch, not a crash fix. It changes ONLY CustomComponents/main.cpp.
Both custom managed components are still registered. Only gameplay hooks are skipped.

Copy main.cpp into the root of your CustomComponents project and rebuild/reinstall ONE TIME.
From then on, control gameplay hook installation at next process start using:
 /sdcard/Android/data/com.ea.gp.pvzheroes/files/customcomponents_combat_hooks.txt

Modes:
 0 = Charge OFF, Thrash OFF
 1 = Charge ON,  Thrash OFF
 2 = Charge OFF, Thrash ON
 3 = Charge ON,  Thrash ON (original/default)

PowerShell:
 $mode=0
 $local=Join-Path $env:TEMP 'customcomponents_combat_hooks.txt'
 [System.IO.File]::WriteAllText($local, [string]$mode, [Text.Encoding]::ASCII)
 adb push $local '/sdcard/Android/data/com.ea.gp.pvzheroes/files/customcomponents_combat_hooks.txt'
 adb shell am force-stop com.ea.gp.pvzheroes

Check logcat: `CustomComponents/COMBAT_DIAG: mode=0 ChargeHooks=0 ThrashHooks=0`
Keep startup cutoff >=2 (recommend 7 for these tests). Disable any other changing variables.

Testing: compare Charge-use then leave the match versus no-Charge then leave;
wait the usual several minutes for each trial. Use multiple runs per mode.
WARNING: In mode 0/1/2, Charge/Thrash gameplay may not work. These modes
are solely to identify cause. Missing selector defaults to original behavior.

Source audit observations:
 - ChargeSystem: thread_local activeCharge and activeChargeAmount are restored
   after ProcessBlockableDamage returns; no managed Charge pointer is retained there.
 - ThrashSystem: thread_local pendingGroups stores IDs (not managed pointers),
   and can retain unmatched records across matches. activeEffectRouter is a
   raw thread_local pointer restored after ProcessEffect returns. Teardown
   hazards and improper hook signatures still need further assessment.
 - A disabled hook is not uninstalled from a running process: force-stop first.
 - If mode 0 still crashes, the other Stage 2 hooks (Armor, deck subset) and
   all preceding stages remain active. That does not exonerate all components.
