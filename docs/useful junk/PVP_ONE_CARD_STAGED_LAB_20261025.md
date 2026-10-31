# PvP stock prepopulation — two-stage / one-card laboratory prototype

**Baseline:** `CustomComponents_PvP_Crash_Isolation_NoEffects_20261025.zip`, tested on the crashing phone in `PvZHeroes_logswewant(20261025-193214).txt`. That log reached `system-root-ok`, `ctor-skipped`, `dependency-fields-validated`, and then the forced no-effects skip. This update preserves the verified system-constructor bypass.

## What changes

Only `main.cpp`, `config.ini`, and `inc/diagnostics/PvpDeckPrepopulatePrototype.hpp` change in the C++ module; this file is new. No PvE deck, FirstPickFromThree, UI, submission, or other gameplay module code is edited.

Stock assembly references for the **exact supplied** `libil2cpp.so`:

- `PvZCards.Core.Systems.PrepopulateCardPoolSystem::ProcessEffect(PrepopulateCardPoolEffect)` RVA `0x21EB834`: uses `Effect.TargetId` to locate a pool, then uses `NumCardsToCreate` to limit its loop and calls stock creation for virtual entries.
- `PrepopulateCardPoolSystem::.ctor` RVA `0x21EBAAC`: no-op in this binary; **never called** by the fallback.
- `PrepopulateCardPoolEffect::.ctor` RVA `0x2291274`: delegates to `Effect::.ctor` RVA `0x227F564`, which initializes stock `Effect.Data`, `Effect.Context`, and default IDs. **Stage A invokes this constructor**, unlike the disabled system constructor.

No custom `Play` payload, direct `EntityModel.CreateEntity`, pool-list edits, or full-deck loop is used.

## Default config: strictly read-only

```ini
[PvpDeckPrepopulation]
Mode=1
BothModdedClientsLab=false
ConstructStockSystemLab=false
ConstructStockEffectLab=false
ExecuteOneCardLab=false
ExpectedDeckFingerprint=
```

`Mode=0`: no hook; `Mode=1`: initial-model shape/fingerprint only. `Mode=2` plus explicit flags enters the laboratory path. Changing `config.ini` requires a rebuild/reinstall; these are Bloom's compile-packaged module settings.

## Stage A — effect-construction isolation (NO deck materialization)

**Do this stage first on the phone that crashed.** Only test on a controlled private match. Set:

```ini
[PvpDeckPrepopulation]
Mode=2
BothModdedClientsLab=true
ConstructStockSystemLab=true
ConstructStockEffectLab=true
ExecuteOneCardLab=false
ExpectedDeckFingerprint=
```

Build and install your Sprig APK using your normal PowerShell workflow. Enter one PvP match and capture `CustomComponents/PVP_PREPOP`.

Expected diagnostic sequence (not an observed result):

```text
PVP_PREPOP: ... eligible=1 hash=<16 hexadecimal characters> nextId=16 ...
PVP_PREPOP: FACTORY_PROBE result=ready modelMatch=1 instanceMatch=1 writes=0
PVP_PREPOP: CRASH_PROBE stage=system-allocate-ok ... writes=0
PVP_PREPOP: CRASH_PROBE stage=ctor-skipped nativeNoop=1 writes=0
PVP_PREPOP: CRASH_PROBE stage=dependency-fields-validated ... writes=0
PVP_PREPOP: EFFECT_PROBE stage=allocate-enter writes=0
PVP_PREPOP: EFFECT_PROBE stage=allocate-ok ... writes=0
PVP_PREPOP: EFFECT_PROBE stage=ctor-enter writes=0
PVP_PREPOP: EFFECT_PROBE stage=ctor-ok dataReady=1 contextReady=1 writes=0
PVP_PREPOP: EFFECT_PROBE stage=ready target=1 count=1 fingerprint=<same hash> writes=0
PVP_PREPOP: SKIP reason=one-card-execution-disabled writes=0
```

An error or crash here isolates effect construction. **Do not proceed to Stage B** until Stage A works on both devices and both devices report the same 16-character deck fingerprint. Merely seeing `FACTORY_PROBE result=ready` is insufficient.

## Stage B — ONE card on exactly ONE pool (experimental / can crash or desync)

This stage is suitable **only for two devices you control, each running identical mod builds, card data and `config.ini`, using a private controlled matchmaking environment.** There is **no remote mod/version handshake**, and a hash is **not** peer authentication. If the second player is unmodded, public, unknown, or not reporting the same hash, **do not enable Stage B**.

With both Stage A logs in hand, and only if both have the same hash, set **on both devices**:

```ini
[PvpDeckPrepopulation]
Mode=2
BothModdedClientsLab=true
ConstructStockSystemLab=true
ConstructStockEffectLab=true
ExecuteOneCardLab=true
ExpectedDeckFingerprint=PASTE_THE_SHARED_16_CHARACTER_HASH_HERE
```

The `ExpectedDeckFingerprint` must be 16 lowercase hexadecimal characters, with **no `0x` prefix**. The earlier log from one phone showed `dea298e889fb3a3c`, but **do not blindly copy it**: verify both devices report the same hash for your intended private test.

Expected logs if the stock single-card call succeeds (not observed yet):

```text
PVP_PREPOP: ONE_CARD_BEGIN pool=1 count=1 nextId=16 initialHash=... privateLab=1
PVP_PREPOP: ONE_CARD_RESULT exact=1 live=1 virtual=87 others=0 firstId=16 nextId=17 entities=17 exception=0 partialStatePossible=0
```

`exact=1` checks *local model structure only*. It does NOT demonstrate remote replay, an authoritative server command, compatibility with an unmodified opponent, or that gameplay effects remain synchronized. Compare the `ONE_CARD_RESULT`, initial fingerprints, and later match behavior on **both** devices. If an exception or `exact=0` appears, stop that trial. The code does not attempt unsafe rollback after a partial stock mutation.

If the human-reviewed hash is missing or mismatched, it logs `SKIP reason=expected-fingerprint-not-configured` or `SKIP reason=expected-fingerprint-mismatch` with `writes=0`.

## Rollback and build

To return to proven non-mutating behavior, set `ExecuteOneCardLab=false` and rebuild; to disable all experimental construction set `Mode=1`. Restore the earlier crash-isolation ZIP if troubleshooting unrelated modifications. Close PvZ Heroes and rebuild Sprig using your existing Windows PowerShell build workflow (`.\\work\\scripts\\all.ps1` from your usual Sprig repo root, if that is your version's script path). Install the resulting `out/Sprig.apk` as usual.

## Artifacts

- Full ZIP: all `CustomComponents/` contents, including unchanged files.
- Updated-files-only ZIP: relative overlay paths `CustomComponents/main.cpp`, `CustomComponents/config.ini`, `CustomComponents/inc/diagnostics/PvpDeckPrepopulatePrototype.hpp`, plus this README. Extract it in the same parent directory containing your existing `CustomComponents` folder and allow replacing the matching files.

## Validation and limitations

- C++20 host **syntax-only** check of the full `main.cpp` passes with supplied Sprig/BNM/Dobby include trees and host Android compatibility shims.
- Previous deterministic-plan regression suite passes. ZIP structure, replacement overlay equality, and diff/patch application are checked.
- **Not built using an Android NDK and not run on a phone in this revision.** The Stage A effect constructor may still crash, and Stage B may still create divergent states despite both clients using the same build. Test one stage at a time.
