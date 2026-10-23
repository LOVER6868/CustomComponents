# PickFromThree release audit — 22 October 2026

**Source of truth:** the user-provided `CustomComponents(20261022-223255).zip`
(the working tree includes uncommitted changes), plus
`Pick3_Live_Crash(2).txt`, and the project's IL2CPP dump/headers and
unpacked APK. Public references: Sprig and `mhdo298/pvzh-server`. External
repositories were read for context; no remote server was patched.

## Status

| Area | Evidence | Change | Runtime confidence |
|---|---|---|---|
| Project layout | `mod/cmake/Bloom.cmake` copies non-src/inc directories as assets; source includes `.git`, docs, examples, tests, tools | Only `Bloom/CustomComponents` build files in deploy path; docs/tests outside | Verified layout |
| Phone/emulator crash | Twice after `UI: CHOICE_EXIT_PREVIEW`, SIGSEGV null +0x38 in `libil2cpp`; three CardView renderers were active | Hard opt-in gate for native CardView plus static-art default, conservative render budget | **Mitigation only**, not reproduced/retested |
| PvE submission | Shared stock SelectTarget DTO and callback logic in uploaded code | PvE stays on current path; added tests for safety policy | Compile and pure tests only |
| Regular PickFromThree PvP | Local per-effect nonce could diverge by process; no authoritative subset in message | Remote-mode candidates sorted and seeded without process-local nonce; remote game-state notification no longer manually forged | Necessary partial fix, NOT complete protocol |
| CardDatabase PvP | Log says `DB5: PVE_ONE_PICK_ONLY` on every submit attempt; choice is GUID, stock targets are entity IDs | Keeps failure explicit and guarded; source never pretends GUID was serialized | **Unsupported until protocol/server integration** |
| Remote default | Uploaded `SubmitInPVP=true`, unlike earlier docs; experimental despite missing ACK proof | Default false, independent `AllowUnverifiedPvp=false` | Verified fail-closed behavior |

## Crash evidence (not a proven renderer root cause)

Two crashes in the supplied log: 2026-10-22 **17:31:12.178** and
**17:39:44.970** (reported device-local time); each is UnityMain SIGSEGV
`SEGV_MAPERR` at `0x38`. The most recent choice exit at 17:39:43.567 is
followed by continuing `FRAMING_INSPECT` on animated previews, then crash.
The APK's `libil2cpp.so` Build ID
`f523655daa037ba2c70491f5347d56399160b088` matches the tombstone.
Disassembling the provided ARM64 library near offset `0x17c7ee0` reveals
an `ldr` through a null receiver plus offset `0x38`; the subsequent crash
stack crosses `BNM::TryInvoke` in `libsprig.so`. This supports a concrete
null dereference, **not** attribution to any single mod method.

The log reports an `x86_64` ABI and `houdini` Arm translation, despite
Samsung-themed build fingerprint fields. Do not treat it as a native ARM64
phone tombstone. An actual physical-phone crash log is still needed.

Mitigation: native factory isn't initialized unless **both**
`Pick3Performance.EnableNativeArtwork=true` and
`Pick3Experimental.AllowNativeCardViewFactory=true`. Safety source defaults
are false. Static card artwork remains available. Rendering at 3×60 FPS was
reduced to a conservative experimental 30/24/24 with 75 global FPS budget,
and extra load spacing.

## Network failure evidence

At 17:38:11.304, 13.034, 14.197, 15.598, 17.180 and 21.385 the log
emits `CustomComponents/Pick3/DB5: PVE_ONE_PICK_ONLY effect=37` after
repeated selections. The uploaded source explicitly rejects CardDatabase
in `PickFromThreeSubmission::Submit` whenever the service isn't local or
required pick count != 1. The modified release code now spells out the
actual mismatch in the rejection message.

`SelectionTransport::TargetSelection.entityIds` populates the stock
`PvZCards.Engine.Dtos.Play.Targets` int list; the CardDatabase picker stores
GUID(s) in `PickedFromThreeQuery` locally. Without an explicit network
representation for GUID selection (and match-side authority), sending a
proxy recipient entity ID cannot instruct the remote server which card was
chosen. This is a protocol constraint, not a timing/UI-only bug.

For ordinary live-target PvP, a sorted/canonical option pool without a
process-local nonce reduces divergent client previews. It cannot guarantee
authoritative eligible sets or server acceptance. Both client builds must
match, and stock game server custom enum behavior remains unverified.

## Changes implemented

1. Added `inc/pick3/Pick3ReleaseSafety.hpp` with dependency-free release gates.
2. Default-off independent risk gates in `main.cpp` and `config.ini`.
3. Stable deterministic option sampler for remote session tests, unchanged
   local/PvE sampler and weighted unique-GUID selection.
4. Removed unproven local `GameStateUpdatedSignal` dispatch after remote
   completion; the PvE signal path stays unchanged.
5. More precise CardDatabase network protocol rejection diagnostics.
6. Modular packaging: only actual build material under `Bloom/CustomComponents`.
7. Added policy assertions + cross-order deterministic sampler assertions to
   the 120,000-draw lifecycle/weight unit test.

## Tests performed

- `g++ -std=c++20 -O2 -Iinc tests/pick3_lifecycle_weight_tests.cpp`: PASS.
- Clang C++20 syntax-only compile of the **whole** `main.cpp` against the
  provided Sprig, BNM and Dobby includes (a temporary host signal/log shim):
  PASS. This is **not** an Android link/APK build.
- Curated module references/build layout manually inspected: PASS.
- Android APK produced: **No**; Android NDK not available here.
- Physical phone crash regression: **Not run**.
- Real server / 2-client PvP: **Not run**.

**Release verdict: candidate for private PvE/device QA only. Not ready for
public PvP or native animated-art release.** See `TEST_MATRIX.md`.
