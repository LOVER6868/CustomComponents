# Stage D: stock materialization of the PickFromThree-targeted main deck (pool 6)

## Why this change

In both Stage C peers, initial stock `PrepopulateCardPoolSystem.ProcessEffect`
materialized two cards from **pool 1**. Both ended with 2 new card entities,
2 linked effect entities, 86 remaining virtual entries, and matching pool and
entity hashes. In the following match activity, the actual PickFromThree
`DECK_TARGET_POOL` selected **pool 6**, not pool 1. Its virtual entries
remained nonselectable under the existing stock-entity-only target protocol.

Stage D makes ONLY the existing two-card experiment's target configurable.
`TwoCardTargetPoolId=6` makes the **one** stock ProcessEffect operate on the
first two virtual entries of pool 6, before normal game initialization. A
successful native card creation is not yet evidence that those cards become
stock-eligible PickFromThree targets.

## Configuration — matched private clients only

```ini
[PvpDeckPrepopulation]
Mode=2
BothModdedClientsLab=true
ConstructStockSystemLab=true
ConstructStockEffectLab=true
ExecuteOneCardLab=false
ExecuteTwoCardLab=true
TwoCardTargetPoolId=6
ExpectedDeckFingerprint=dea298e889fb3a3c
```

The initial deck fingerprint has to match **on both phones**. A matching
fingerprint isn't a protocol handshake or simulation proof. Do NOT enable in
public/ranked PvP or for any unmodded/unmatched peer. The operation cannot
rollback partially applied native writes. At most one mutation per process.
Restart both apps to repeat. After the match set `ExecuteTwoCardLab=false`.
Defaults remain read-only (`Mode=1`, all lab gates disabled, target 1).

## Read-only verification following native execution

1. Confirm logs show `labGates ... twoCardExecute=1 twoCardTargetPool=6` and
   `TWO_CARD_BEGIN pool=6 count=2` on both clients, with matching initial hash.
2. Compare `TARGET_ENTRY`, `MATERIALIZE_INSPECT POOL_LINK pool=6`, `CARD_REL`,
   `OWNER_BASELINE`, `ENTITY_END` and `SUMMARY pool=6 exact=1` across clients.
   Supporting effect entities may be different for different GUIDs. Do not
   assume two effects or a fixed `NextEntityId` for this test.
3. Compare `TWO_CARD_RESULT pool=6 exact=1 ...` between phones. `exact=0`,
   exception, unexpected owner, unexpected entity, or crash = stop and examine.
4. **Most important PickFromThree check:** the later *existing* logs
   `Pick3/DECK_TARGET_POOL matchedDeck=6`, `selectedPoolLive`,
   `selectedPoolStockTargets`, `Pick3/POOL_BOUNDARY ... returned`, and
   `Pick3/PHYSICAL_SUMMARY stockLive` must be evaluated AFTER the Stage D
   prepopulation. Expectation is an increase over baseline in the same pool,
   subject to normal draws/turn changes and ability targeting restrictions;
   no increase means prepopulation has not solved stock eligibility.
5. Collect two-phone logs in one ZIP for side-by-side match-state inspection.

No PickFromThree hook or Relic Hunter code, card definition, custom ability,
transport packet format, or live deck resolver was changed. This is an
experimental pool-target pivot only, NOT a completed PvP deck fix.
