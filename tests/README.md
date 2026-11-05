# Native checks

On Windows, install the Android NDK and Visual Studio C++ tools, then run:

```powershell
./tools/Test-Native.ps1 -SprigRoot 'C:/path/to/Sprig-main'
```

The script uses `ANDROID_NDK_ROOT` (or `-NdkRoot`) and locates Visual Studio with
`vswhere` (or accepts `-VcVars`). It checks the full module with Android ARM64
headers in default and rollback configurations, then compiles and executes:

- `ReturnToLanePolicyRegression`: card types, full lanes, Team-Up, environment slots.
- `ReturnToLaneMoveFilterRegression`: return-only suppression, ordinary moves,
  preserved play/return events, absent identities, and simulation-model isolation.
- `Patch3SubmissionLifetimeRegression`: deferred PvP intent, duplicates, reset.
- `SelectionTransaction216Regression`: existing transaction ownership/routing.
- `PhysicalDeckSelection222Regression`: exact physical-card identity.
- `Pick3VerifiedBindingRegression`: selected GUID to created-entity binding.

Output stays in `.build/tests/`. These checks do not link an APK/library or run
Android gameplay. Follow the device matrix in [release notes](../CHANGELOG.md).

The other versioned tests are retained to preserve regression history. Many Python
files assert earlier source shapes and diagnostic strings; they are not a current
all-tests acceptance suite. `run_selection_transaction216.py` remains available for
its original host/compiler setup and broader BNM-dependent runtime tests.
