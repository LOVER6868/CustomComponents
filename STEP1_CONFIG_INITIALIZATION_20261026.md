# Step 1 — Configuration and initialization reliability (2026-10-26)

## Scope

Built on the user's `CustomComponents(20261026-000019).zip` baseline (SHA-256: `1f33d27fe9299d063c93f720219796e372b3032c60bcfd748d1c3f0f66893466`). The 88-card
prepopulator, model fingerprint gate, stock SelectTarget transport, PickFromThree
sampling, CardDatabase PvP implementation, and Relic Hunter were **not changed**.

### Config wiring restored

- All 21 `[Pick3RewardLayout]` entries now connect to ConfigureLayout and badge offsets.
- All 17 `[Pick3StatText]` entries now connect to ConfigureStatText.
- DescriptionSizePercent now lives in `[Pick3StatText]`, with packaged value 160:
  this reproduces the **previous effective** value read from the now-removed
  `[Pick3Description] FontSizePercent=160`. It does not revert to the formerly
  ignored 120.
- Other known unused or deliberately disabled/legacy settings were not silently
  activated: the retired FirstPick rebind, Pick3Experimental switches, four
  unsupported camera knobs, LogCaptures, and intentionally disabled prewarm flags.
  They require separate feature decisions rather than arbitrary reactivation.

### Startup isolation controls

New packaged `[StartupDiagnostics]` settings, with normal-operation defaults:

```ini
[StartupDiagnostics]
StartupCutoff=-1
CombatHookMode=3
Pick3HookMode=-1
```

- `StartupCutoff=-1` = normal full startup; `0`..`9` = monotonic diagnostic cutoff.
- `CombatHookMode`: `0` neither Charge/Thrash hooks, `1` Charge, `2` Thrash, `3` both.
- `Pick3HookMode=-1` honors `[Pick3Stage8Isolation]`. Override `0` neither,
  `1` candidate only, `2` unsupported (no Stage 8 hooks), `3` both.

Optional game-external one-line decimal override files (check external
`/sdcard/Android/data/com.ea.gp.pvzheroes/files/`, then internal
`/data/user/0/com.ea.gp.pvzheroes/files/`):

- `customcomponents_startup_stage.txt`: -1 through 9
- `customcomponents_combat_hooks_mode.txt`: 0 through 3
- `customcomponents_pick3_hooks_mode.txt`: -1 through 3

An absent or invalid external file falls back to the packaged value. Valid
external files take precedence. Force-stop and relaunch the game to apply
these values. **Remove any old override files left from previous debugging** before match testing; previously ignored files may now be honored. Do **not** leave an isolation override in place for match testing.

### Hook reliability

- Submission hooks are installed only after TargetBridge initialization reports
  success and its enabled flag is set. A requested setting alone is not enough.
- The UI is told that live candidates are available only if TargetBridge is ready.
- All required metadata for `CustomFieldBridgeSystem`'s `Entity.DeepCopyFrom`
  hook is validated before installing *any* deserializer hooks. Failure while
  installing an individual native hook still cannot be rolled back here.

### Validation

- `python tests/Step1ConfigInitializationRegression.py`: checks 38 config keys,
  packaged/effective defaults, all three diagnostic selectors and hook-order invariants.
- Existing C++ regression suite: five tests.
- Host Clang C++20 syntax-only compile against included Sprig/BNM/Dobby headers.
- **Still required:** real Android build and two-phone match regression. Syntax
  and static tests do not verify behavior of installed native hooks.
