# PickFromThree focused regression repair (2026-10-23)

Baseline: `CustomComponents(20261023-122803).zip`. Changes confined to three existing files.

1. `config.ini`: restore PvE local completion fallback (`SyncLocalCompletionSignal=true`) and native animated avatar rendering (`EnableNativeArtwork=true`). Keep PvP live-target submission enabled.
2. `inc/pick3/PickFromThreeSubmission.hpp`: restore the previously working PvE-only conditional completion notification. PvP never uses this local fallback.
3. `inc/presentation/detail/Pick3NativeAvatarRender.hpp`: guard calls during the animated-avatar Unity hierarchy traversal; return/skip if managed calls fail instead of blindly walking the returned pointers.

Validation: `clang++ -std=c++20 -fsyntax-only` on `main.cpp` passed with included BNM/Dobby/Sprig headers and Android log shim. This is NOT a real Android link/build or device test.

Remaining issues: this does not establish native lifetime safety (TryInvoke cannot catch native SIGSEGV), fix PvP authoritative GUID transport for CardDatabase, fix all input-state synchronization or correct reduced candidate snapshots. These require runtime trace evidence and appropriate server/client protocol work. No fabricated CardDatabase-PvP success is introduced. Preserve one selection per PickFromThree activation; additional targets use existing card-data query logic.
