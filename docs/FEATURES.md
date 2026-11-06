# Feature catalog

These are public card-authoring features. Internal runtime carriers are implementation
details. The [example catalog](../examples/README.md) links recipes for every area.

## Abilities

| Feature | Purpose |
| --- | --- |
| Armor, grantable ID `19` | Grant or modify stock numeric Armor |
| Charge, ID `20` | Control block-meter charge produced by a fighter's damage |
| Thrash, ID `21` | Deal excess fighter damage to the hero opposing the damage source |
| RandomKeyword, ID `22` | Grant an eligible keyword from a configurable pool |
| Destroyable BoardAbility | Spend a hidden board ability through destruction targeting |
| Grant/remove lifecycle | Add/Set operations, duration expiry and ability history |

See [abilities](ABILITIES.md) and its linked references.

## Selection and queries

| Feature | Purpose |
| --- | --- |
| `PickFromThreeRandomTargets` | Offer up to three weighted physical candidates |
| `PickFromThreeUniqueGuidTargets` | Offer distinct definitions while retaining a physical representative |
| `PickFromThreeCardDatabase` | Choose a definition for a subset-based payout |
| `AdditionalTargetSelectionType` | Override additional selection independently |
| `AdditionalTargetFirst` | Choose an interactive additional target before primary PF3 |
| `PickedFromThreeQuery` | Match the chosen GUID |
| `PickedFromThreeEntityQuery` | Match the exact selected or produced physical card |
| `InDeckQuery`, `InDiscardPileQuery` | Test physical zone membership |
| `InDeckGuidQuery`, `InDiscardPileGuidQuery` | Match definitions whose GUID occurs in a zone |
| `DiscardedFromQuery` | Distinguish custom hand and deck discards |
| `ListPositionQuery` | Compare zero-based deck/discard position from either end |
| `OpenLaneForCardQuery` | Find lanes for cards returned by a nested card query |
| `CanBePlacedQuery` | Match cards that fit the current board |
| `ListPosition` target sorting | Order physical targets by pool position |

See [PickFromThree](PICKFROMTHREE.md) and [queries](QUERIES.md).

## Card and zone effects

| Descriptor | Purpose |
| --- | --- |
| `DiscardFromDeckEffectDescriptor` | Discard a physical deck card |
| `DiscardFromHandEffectDescriptor` | Discard a physical hand card |
| `RemoveFromDiscardPileEffectDescriptor` | Remove a current discard member |
| `ReturnFromDiscardPileEffectDescriptor` | Return the original discarded card to hand |
| `ReturnFromDiscardPileToDeckEffectDescriptor` | Return it to a deck position |
| `ReturnFromDiscardPileToLaneEffectDescriptor` | Return a fighter or environment to a legal lane |
| `MoveCardInDeckEffectDescriptor` | Reorder a deck card by offset or absolute position |
| `DrawCardFromSubsetInDeckEffectDescriptor` | Tutor a deck card, with optional fallback and HeraldEntities |
| `CreateCardInDeckFromSubsetEffectDescriptor` | Create copies of one sampled definition in a deck |
| `CreateCardFromSubsetEffectDescriptor` extensions | Grant Team-Up/Aquatic and override front/back ordering |

See [zone effects](ZONE_EFFECTS.md).

## Triggers

`CustomTrigger` listens for a named event. `FireCustomTriggerEffectDescriptor`
broadcasts it; `FireTriggerEffectDescriptor` invokes a selected built-in or named
trigger. `DiscardOutsidePlayTrigger`, `ReturnedFromDiscardPileTrigger` and
`PickFromThreeTrigger` connect discard, return and selection to card effects.
See [trigger examples](../examples/triggers/README.md).

## Presentation and shared services

- Discard inspection presents the physical card's current stats.
- Custom discards receive the module's `discarded` herald origin.
- Optional HeraldModifiers can append its `Gained …` summary.
- Card-frame reconciliation updates views after animation/history work.
- StatIcons provides configurable attack/health icons for registered abilities.
- FrameIconRegistry provides a C++ extension point for other icon providers.
- DeckMaterializer maintains physical identities for main-deck entries.
- Shared hook, queue, model, pool and subset services support new components.

See [presentation](PRESENTATION.md) and [architecture](ARCHITECTURE.md). Tradeable
and StartingDeck are not in the current public feature set.
