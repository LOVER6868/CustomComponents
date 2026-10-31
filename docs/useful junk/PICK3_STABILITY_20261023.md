# PickFromThree stabilization — October 23, 2026

## Crash mitigation (default on)

The uploaded live log shows factory probe mode **7** creating all three native card avatars during a Fisherman popup. Both crashes have a matching `libil2cpp.so +0x17C7EE0` null dereference, but unsymbolicated Sprig frames prevent absolute attribution. To reduce exposure to that unsafe IL2CPP path:

- In normal gameplay `PickFromThreePreviewSystem::Open`, disable native experimental avatar load/render. The static card-art fallback remains and gameplay submission paths are unchanged.
- Clamp the old factory probe selector to read-only modes 0–4; a stale `pick3_factory_probe.txt` containing 5/6/7 cannot force unsafe factory creation.
- Close an orphan Pick3 UI after three separate polls of the *local* SelectTargetService find that the bound blocking effect/group has disappeared. This cancels UI-side input and async tickets; it does **not** force effect retirement, mutate network commands, or change PvP ownership.
- Preserve already-working DB5 GUID transport, stock hand highlights, PvP button handoff, and phase tracing.

## Unique weighted sampler: test and diagnosis

`SelectOptions(UniqueGuidTargets)` already operates on a new local collection each call and does not modify `eligible` or the real deck. The weighted stack is erased **only from the current draw of up to three options**; when fewer than three GUIDs exist, the source uses a second pass to offer distinct physical entities.

Regression source `tests/Pick3PhysicalCopiesRegression.cpp` verifies that a 15-copy pool over GUIDs 2,3,4,5,6 yields three options on 30 repeated activations when moving a card in the deck does not consume it; and 13 activations with one removed physical entity per activation. This passes on host with C++20.

**Not proven fixed:** live game logs show the upstream `originalFindValid` can return fewer cards on later activations. The sampler cannot legally include cards excluded from the stock query. The proper remaining diagnosis is to compare current deck physical entities to the IDs admitted by `FindValidTargets` at each trigger and to verify which gameplay effect actually moves/draws/consumes them. Do not synthesize missing IDs, bypass the real query, or pretend repeated copies remain when the stock model says otherwise.

## Validation

- C++20 syntax check of `presentation/PickFromThreePreviewSystem.hpp` with the repository's BNM and Dobby headers: passed.
- Host regression test: passed.
- Android compilation and device gameplay: **not performed**.
- Crash mitigation is evidence-based, not a confirmed fix until the device is retested.
