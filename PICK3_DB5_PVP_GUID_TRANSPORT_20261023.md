# PickFromThree CardDatabase (DB5) — PvP GUID transport experiment

## Baseline
Built on `Pick3_full_source_stock_hand_highlights_20261023.zip`. Existing default-on PvP button/completion repair, playable-hand-highlight refresh, live-entity PickFromThree modes (3 and 4), single-physical-copy deck moves, and network/effect tracing are retained.

## Default-on behavior

- DB5 permits **exactly one** choice and requires an already validated **single real recipient** from the game's own AdditionalTargetQuery. The chosen definition GUID is not an entity ID.
- Submit **one** stock `Play.Type=6 (SelectTarget)` action, with `Play.Targets[0]` set to the real recipient and `Play.CardId` set to the chosen card definition GUID. All non-DB5 SelectTarget plays retain `CardId=-1`.
- The foreign-owner `TargetEntityFinder.FindValidTargetsFromEffect` path suppresses the popup, but temporarily sets the stock Manual selection mode and redirects the DB5 Query to AdditionalTargetQuery so the other simulation can find the same real recipient. The original database query remains rooted.
- The existing LocalSelectTargetService.Process hook checks the real blocking effect, exact owner, exact filter/mode, selected GUID's membership in the original CardDatabase query, and real recipient existence plus DB5 shape settings. It seeds `PickedFromThree` as `SelectedCard{-1, chosenGuid}` *before* delegating to the original service.
- Stock `LocalSelectTargetService.Process` still processes and validates the real target. No extra plays, spoofed match entities, forced effect retirement, or forced subturn changes.
- Receiver-side temporary filter/query overrides restore after the original Process returns; restorations are **filter-specific** to avoid changing another simulation copy's query. Existing popup ownership and lifecycle cleanup stay in place.

## ADB tracing (all gameplay changes remain enabled when trace = 0)

```powershell
adb shell "echo 2 > /sdcard/Android/data/com.ea.gp.pvzheroes/files/pick3_net_trace.txt"
adb logcat -c
adb logcat -v time -s Sprig:V > db5_pvp_trace.txt
```

Look for:

- Startup `DB5_PVP: SETUP defaultOn=1 inbound=1 cardId=1 recipientRedirect=1 dbQuery=1`.
- Sender `DB5_PVP: TX_ENCODE effect=... group=... guid=... recipient=... remote=1`.
- Serializer `SERIALIZE_*` showing `type=6 card=<same GUID> targets=1 [<real recipient>,...]`.
- Remote/client simulation `DB5_PVP: RX_VALID effect=... group=... guid=<same GUID> recipient=...` (requires the receiving simulation to load the patched mod).
- Stock service success, eventual effect retirement, hand playable highlights and normal input.

Turn **only tracing** off with:

```powershell
adb shell "echo 0 > /sdcard/Android/data/com.ea.gp.pvzheroes/files/pick3_net_trace.txt"
```

## Known uncertainties

This is an **unverified protocol experiment**, not an already proven remote gameplay fix. Host C++20 syntax was checked; no Android APK was built and no live PvP test was run. In particular:

1. `Play.CardId` is unused by the inspected stock LocalSelectTargetService, but the network serializer, server and all receiving simulations may have other constraints on this field. `Play.CompareTo` does not include CardId; prediction/reconciliation may therefore treat two choices with the same recipient but different GUIDs as equivalent.
2. A client without the new receiver hook cannot reconstruct the selected CardDatabase GUID. A server that does not preserve `CardId`, or that doesn't process the modded DB5 effect, may fail/desync. HTTP success does **not** prove accepted gameplay.
3. Only `NumTargets=1`, `AdditionalTargetType=Query`, and `OnlyApplyEffectsOnAdditionalTargets=true` are supported. The receiving simulation must have a usable game-bound CardDatabase query.
4. When a GUID cannot be validated or an effect doesn't match DB5, the hook doesn't invent selection state; it forwards to the original stock service so stock validation decides.

## Files changed

- `inc/pick3/Pick3ReleaseSafety.hpp`
- `inc/pick3/PickFromThreeTargetBridge.hpp`
- `inc/pick3/PickFromThreeSubmission.hpp`
- This readme
