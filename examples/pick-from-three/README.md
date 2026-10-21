# PickFromThree JSON guide

The three supported modes are:

| SelectionType | Value | Picks candidates from |
|--|--:|--|
| PickFromThree | 3 | Unique live entity IDs (duplicate GUIDs allowed) |
| PickFromThreeUniqueGuidTargets | 4 | Live entities with distinct GUIDs |
| PickFromThreeCardDatabase | 5 | CardDataMapper database GUIDs, **one pick in local PvE** |

The retired serialized name `PickFromThreeRandomTargets` remains an alias for **value 3**, never 5. Existing numeric-5 card data previously meaning random live mode must be migrated to 3 before installation.

## Database test

Use `effect-group-card-database-draw.json` as an example **effect entity component list** in `card_data_5`. `PrimaryTargetFilter` must have `NumTargets: 1` and **SelfQuery**. This gives the stock targeting service one valid live *proxy*, so its normal completion animation and game-state notification still run. The three displayed card GUIDs are sampled from the game's captured `CardDataMapper.GetAllCardIds()`; the chosen definition is stored as GUID-only (`EntityId=-1`) and consumed by `PickedFromThreeQuery` in the draw effect. The proxy is NOT the chosen card definition.

Requirements: a real, eligible self entity; the existing `CardAssetsTask` dependency-capture hook and sprite cache enabled; and a downstream GUID-domain payout such as `DrawCardFromSubsetEffectDescriptor`. Do not combine the database filter with a `MoveCardInDeck` payout (that would act on the proxy, not the chosen definition). This is an experimental adapter and has not been tested on-device. If `PROXY_REQUIRED` or `DATABASE_UNAVAILABLE` appears, the popup will not be offered.

For live targeting, change only `SelectionType` in your existing PrimaryTargetFilter (and `NumTargets` if needed); leave the existing working queries/effects unchanged. `PickedFromThreeQuery` checks card GUIDs in the database; `PickedFromThreeEntityQuery` checks actual chosen live entity IDs. Database-only choices have no valid entity ID for the latter query.

`NumTargets=2/3` remains supported for live modes. The database proxy path deliberately fails closed for 2/3 picks pending a separate engine-native multi-GUID payout; PvP database selections are disabled.

## Android log checks

Boot: `Pick3/DB5: CARD_DATABASE preflight mapper=1 getAll=1 int=1`.
On first target: `Pick3/STEP2: ... selectionType=5 mode=2` and three distinct sampled card GUIDs in `PRESENT`.
Click: `Pick3/DB5: GUID_SELECTED ... stockProxy=...`, `Pick3/PICKED_QUERY: STORE`, `Pick3/SYNC410: NOTIFY_RETURN`, then `Pick3/TRACE49: END_ANIM_RETURN` only if the optional tracer is enabled.

The actual Android outcome remains unverified. If it fails, send the above and subsequent `Pick3/PICKED_QUERY` lines before expanding DB candidate filters or payout behavior.
