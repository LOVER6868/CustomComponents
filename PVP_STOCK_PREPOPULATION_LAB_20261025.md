# PvP initial-model stock prepopulation prototype (experimental)

## Why this route

The October 25, 13:46:57 `PVP_INIT_AUDIT` captured the live initial `EntityModel` with `Entities=16`, `NextEntityId=16`, and four GUID-only deck pools: entities 1/2/6/7 containing 40/4/40/4 entries, **all with `EntityId=-1`**. The later Pick3 deck target audit uses that same `EntityModel`. The public Python relay server initializes those virtual entries and forwards play updates without running Unity's card factory.

The bundled ARM64 `PvpInitResponseExecutable.InitializeGame` performs `SetEntityModel` (RVA 0x2FE4074), then `SetRngSeedData` (RVA 0x2FE4190), **then** virtual calls to the injected `GamePredictor` and `GameStateUpdateHandler`. The prototype hooks **the latter setup method after stock returns**, so its prepopulation attempt is in the initial base match model before the predictor is initialized. It never runs from target enumeration or modifies `Play.Targets`.

`PrepopulateCardPoolSystem.ProcessEffect` (RVA 0x21EB834) and its injected `Model`/`CardFactory` are stock. If a same-model system is available, the lab path invokes that stock method on four stock `PrepopulateCardPoolEffect` instances targeting pools 1, 2, 6, 7 in this fixed order. It does **not** invent a custom card constructor or network packet.

## Exactly what changed

- **Added** `inc/diagnostics/PvpDeckPrepopulatePlan.hpp`: pure, deterministic validation and fingerprinting of ordered initial deck entries.
- **Added** `inc/diagnostics/PvpDeckPrepopulatePrototype.hpp`: one opt-in `SetRngSeedData` native hook, preflight, same-model DI lookup, optional stock prepopulation.
- **Added** one optional installer call in `main.cpp` stage 3 and a config section in `config.ini`.
- **Added** one host regression for deterministic fingerprints, duplicates, invalid GUIDs, stale IDs, wrong IDs/counts and incomplete plans.
- **Unchanged** existing PvE `DeckMaterializer`, global mulligan cleanup hook, FirstPickFromThree UI, targeting, Play submission, DEK1 protocol, card data, and game/server.

## Config and safety modes

The bundled config uses read-only Mode 1:

```ini
[PvpDeckPrepopulation]
Mode=1
BothModdedClientsLab=false
```

- **0**: No prototype hook installed (preferred production state).
- **1**: Read-only initial model preflight; logs `PVP_PREPOP` eligible, 88 virtual cards, fingerprint; **no writes**.
- **2**: Attempt stock deck creation **only** if `BothModdedClientsLab=true`, all four initial pools have the exact expected shape, every entry is `-1`, a stock `PrepopulateCardPoolSystem` is resolved through the *current* game's existing injection binder, and both system model and card factory model equal the actual PvP base `EntityModel`. Missing/mismatching dependencies skip without writes. The consent flag is **not a two-peer handshake**.

> **IMPORTANT: Mode 2 is an unsafe laboratory experiment, not a proven PvP solution.** It cannot verify the other peer's module/asset versions or the server's compatibility. It invokes stock methods outside their usual queued-effect replay and may produce unrecorded local mutations, divergence, hangs or crashes. Do **not** use it in ranked, public, or mismatched-client games. Test only a controlled private match with two equally modded clients and a backup of the original APK. Even two identically built clients are not guaranteed to converge.

The mode-2 design is not an authoritative new protocol, lacks peer acknowledgment and rollback, and can partly mutate the state before the stock method throws. **A successful `STOCK_RESULT` is local only.** It is never a claim of synchronized authoritative success.

## Test procedure

1. Back up the working `Bloom/CustomComponents` directory. Apply just the patch if your source has newer changes, or replace that module with the supplied ZIP.
2. Start with the packaged `Mode=1`, `BothModdedClientsLab=false`, and optionally `[PvpDeckDiagnostics] InitSnapshot=true` for the existing post-initialization snapshot.
3. Rebuild Sprig using the existing build procedure, reinstall, launch and enter a controlled PvP match.
4. Collect log lines containing `CustomComponents/PVP_PREPOP` and `CustomComponents/PVP_INIT_AUDIT`. Expected Mode 1 lines are `eligible=1`, matching fingerprint on both peers when initial ordered deck contents are identical, `PREFLIGHT_OK virtual=88 writes=0`. Log `initial-model-shape-mismatch` should never trigger mutation.
5. **Only after successful Mode 1 on both identically modded test clients**, optionally test Mode 2 with both booleans explicitly set on **both** clients, in a controlled non-ranked match. Don't test with an unmodified opponent. The prepopulation hook may still skip with `di-not-ready`, `stock-system-not-bound` or `factory-model-mismatch`. These are intentional fail-closed outcomes, not reasons to bypass safeguards.
6. If stock materialization did run, `STOCK_RESULT pools=4 remainingVirtual=0` is expected locally. Compare actual card/entity counts, matching deck fingerprints and successful following plays on **both** devices, including predicted vs observed remote choices and disconnect/desync behavior. If anything differs, revert to Mode 0 immediately.

## What the logs mean

- `PVP_PREPOP: install=1`: 1 new hook installed.
- `PVP_PREPOP: seq=... mode=1 model=... eligible=1 hash=...`: matching exact initial model shape, before any materialization.
- `PVP_PREPOP: PREFLIGHT_OK virtual=88 writes=0`: observation succeeded; no cards created.
- `PVP_PREPOP: SKIP reason=di-not-ready`: stock game-context CommandBinder wasn't yet available. **Do not force a different model's factory.**
- `PVP_PREPOP: SKIP reason=stock-system-not-bound`: current game DI doesn't bind stock `PrepopulateCardPoolSystem` through that key. Needs a different lifecycle resolution strategy.
- `PVP_PREPOP: SKIP reason=factory-model-mismatch`: injecting the candidate would create entities in the wrong model, correctly refused.
- `PVP_PREPOP: STOCK_RESULT ...`: a *local experimental* operation; a network ACK and two-peer validation are **not** implied.

## Validation performed here

- Host-side `clang++ -std=c++20 -fsyntax-only` passed for the **entire modified `CustomComponents/main.cpp`** with the uploaded Sprig and BNM headers and temporary Android compatibility build headers.
- Five host-side C++ regression executables passed, including the new prepopulation planner and existing physical deck/wire receiver tests.
- The native assembly addresses were checked against the uploaded `libil2cpp.so` and `dump.cs`.
- **No Android NDK APK, online peer convergence, real stock DI resolution, stock effect execution or runtime game stability test has been performed.**
