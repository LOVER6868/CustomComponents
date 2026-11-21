# November 18 one-pass diagnostic patch — acceptance procedure

## Baseline and non-goals

This patch is based on the **uncommitted contents** of `CustomComponents(20261118-191331).zip`, branch `unstable` at source HEAD `b5c2da518d7ae9f6b8f45947c49ab9facce704da`. Existing fixes are preserved. The November 18 phone/emulator logs are *historical*, not an instrumented run of this patch. There is no actual rebuilt APK or game run bundled with this source patch. Host Clang C++20 syntax checking and source-regression checks do **not** establish runtime hook availability, behavior, or stability.

**Redirection and PF3/AI are still diagnostic-first**: native candidate/target-selection behavior has not been reproduced or fixed conclusively. Do not hide that uncertainty. **Small-shield binding intentionally fails closed** if no unique indicator sprite matches the named prefab candidates. This prevents the health artwork/health number from being recolored, but requires one real prefab inspection before confirming golden mode. No Halfbread listener suppression or forced merged snapshots were added.

## Install/build/identity

1. Diff/apply this patch *only* against the uncommitted November 18 module snapshot; do not apply against bare `git HEAD`, which omits hundreds of unrelated working-tree changes. Or use the patched source ZIP and merge into the installed `CustomComponents` module.
2. Build with the installed Sprig workflow (read `AGENTS.md`/`bloom.ini` for module placement): from Sprig root in Windows PowerShell run `./work/scripts/all.ps1` (or `./work/scripts/build_mod.ps1` if your installed workflow uses module-only builds), keeping its configured `ANDROID_NDK_ROOT`. The output of a full build is `out/Sprig.apk`. The current sandbox has no Android NDK and did not produce this APK.
3. In this source snapshot, `config.ini` already has `[Performance] Enabled=true`. Keep it for the short profiling capture, then disable to remove overhead. Critical discard, shield, redirection and trigger milestones log without `CUSTOMCOMPONENTS_VERBOSE_DIAGNOSTICS` enabled. If you need additional `CC_DIAG` or `CC_SELECTION_DIAG` contextual details, rebuild with `-DCUSTOMCOMPONENTS_VERBOSE_DIAGNOSTICS=1` temporarily; that can itself increase lag.
4. At startup record the game's version, module version/hash and `CustomComponents/AI/PROFILE_INIT` / `Superpowers/LIFECYCLE_HOOKS` statuses. All required hooks must report success for their respective feature. Preserve `adb logcat` timestamp/device identity, and record whether it was an emulator or real device.

## One gameplay capture, in this order

| Scene/scenario | Required observation / trace | Acceptance condition |
|---|---|---|
| Opening mulligan | `MULLIGAN_END`, `OPENING_GROUP_CREATED`, `OPENING_POST_PROCESS`, `OPENING_DRAW_START`, `OPENING_DRAW_COMPLETE`, `INITIAL_POOL_GUARD` | Temporary initial four does not trigger golden. Native groups complete before golden. Preserve native health artwork and text |
| Power counts | Observe remaining powers **3 → 4 → 3 → 0 → 1** and capture screenshots/video | Golden only at 4+ after native opening settled; ordinary little shields and health display correct at 3, 0 and 1; `PREFAB_BIND` selects only actual indicators |
| Hand discard, both sides | `TICKET`, `BOUNDARY`, `RETIRE_READY`, `LIFT_BEGIN/LIFT_COMPLETE`, `SHRINK_BEGIN/SHRINK_COMPLETE`, `CLEANUP: DESTROY_REQUESTED` | One ticket/view/scene, hand departure confirmed, native tween completion precedes hand-view destruction; no disappear/return/disappear |
| Withering, both sides | `Withering/QUEUE`, `ORDINARY_EFFECT`, native stock zone movement + same presentation trail | End-turn ordinary effects used, no duplicate destroy or presentation owner |
| Halfbread after one prior plant play | `Variables/EVENT_CAPTURE`, comparisons, `TriggerSnapshots/QUEUED`, `EFFECT_BOUND`, `RESOLVE`, `DRAW_CREATED`, `Draw/DELIVERY` | Distinct listeners (including stacked 167/168 case) share event snapshot sequence but retain separate chain state; each eligible actual draw delivered |
| Level playability | `Performance: area=level-quote`, `level-highlight`, `Level/HIGHLIGHT` | Sum calls/total/max and compare to unpatched same scene; no repeated change to already-updated view |
| Opponent deck-buff frame | `Stats/RECONCILE_BATCH` + `Stats/RECONCILE` | Correct visible opponent attack/health/buff, without revealing hidden opponent deck/hand state |
| AI turn and PF3 | `AI/PROFILE_INIT`, `PF3_TARGET`, `PF3_QUERY`, `Level/AI/`, per-three-second `ai-candidate-step`, `ai-scorer-poll`, `ai-model-copy`, `ai-query`, `ai-custom-hook` | If PF3 has no effective AI target/commit, preserve descriptors, origin contexts, candidate/query results and scheduler order; don't infer success from `SelectTarget` return alone |
| Damage redirection | `DamageRedirect/PROCESS`, `VALID_TARGETS`, `FRONT_ACCEPT/REJECT`, `SELECT`, `CREATED`, `RESULT` | Confirm actual context self, candidate filter, final redirected effect target and resulting health delta; retest normal and environment/lane-front redirectors |

Do not use opponent face-up card data in diagnostics for hidden hand cards: only log view identity, hand ID and public animation geometry. All native object pointers in captures are process-lifetime identities only, not stable across matches/processes.

## Collection and analysis

From PowerShell, record phone and emulator independently (replace `DEVICE_SERIAL` with `adb devices` output):

```powershell
adb -s DEVICE_SERIAL logcat -c
# Launch the game; run the above scenarios. Then:
adb -s DEVICE_SERIAL logcat -v threadtime -b all -d > capture_phone_new.txt
python tools/OnePassCaptureAudit.py capture_phone_new.txt --out acceptance_audit.md --crashes acceptance_crashes.md
```

Keep both the raw files and their SHA-256 digests. The analyzer gives bounded per-area counts, summed totals and maxima from the log (historical reports are per-three-second windows; the analyzer adds them across recorded windows). Do not directly compare aggregate sums between runs of different duration or scene content; normalize to same scenario, same hardware and measured work. AI scorer polling includes idle work, so the dedicated `ai-scorer-poll` area is separate from candidate/model-copy/query/custom-hook processing.

## Native crashes: separate from presentation acceptance

Phone log contains **five native SIGSEGV crashes** around Nov 17 23:42:49–23:43:22, before the gameplay performance traces. The emulator file has no fatal-signal reports. Retain the tombstone's process/thread/fault address, build ID and module-relative PCs. Native symbolization should run with the *same Build ID* `.so` and its debugging symbols; the uploaded APK's `libNimbleCInterface.so` (Build ID `caa73707e786fbadee073c1745dea067d3bafaf7`), `libNimbleCInterfaceNexus2.so` (`0aae891cb7ecb65eada19c7a29e82ca06fc3adae`) and `libNimbleCInterfaceBase2.so` (`c9c1e1f7b49d8737c34c801fc293582e93bb2ddb`) match the phone stack. Example from a native-symbol-enabled machine:

```text
llvm-addr2line -f -C -e libNimbleCInterface.so 0x42e24
```

The tombstones include ART/JNI GlobalRef stacks, Nimble libraries, and UnityMain threads. Their **root cause is unknown**; passing shield/discard tests does not establish their resolution. Compare a known stock game run and the same mod build with instrumentation disabled before attempting an attribution.
