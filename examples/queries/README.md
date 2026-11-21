# Query examples

These JSON objects go into an existing `Query` field; they don’t do anything on their own. Use GUID queries for card definitions and entity queries for specific physical copies. See [query reference](../../docs/reference/queries.md).

## Start with these

- [Deck bottom five](list-position-deck-bottom-five.json)
- [Placeable card](can-be-placed-any-lane.json)
- [PF3 exact card](picked-from-three-entity.json)
- [Level three or higher](leveled-greater-or-equal.json)
- [Card set metadata (Set3)](has-set.json)
- [Rarity component (R3)](has-rarity.json)

## Other variants

- [can-be-placed-non-water-lane.json](can-be-placed-non-water-lane.json)
- [deck-except-top.json](deck-except-top.json)
- [destroyed-in-play-in-discard.json](destroyed-in-play-in-discard.json)
- [discarded-from-deck.json](discarded-from-deck.json)
- [discarded-from-hand.json](discarded-from-hand.json)
- [in-deck-guid.json](in-deck-guid.json)
- [in-deck.json](in-deck.json)
- [in-discard-pile-guid-query.json](in-discard-pile-guid-query.json)
- [in-discard-pile-guid.json](in-discard-pile-guid.json)
- [in-discard-pile.json](in-discard-pile.json)
- [leveled-equal.json](leveled-equal.json)
- [leveled-greater.json](leveled-greater.json)
- [leveled-less-or-equal.json](leveled-less-or-equal.json)
- [leveled-less.json](leveled-less.json)
- [leveled-not-equal.json](leveled-not-equal.json)
- [list-position-compare-equal.json](list-position-compare-equal.json)
- [list-position-compare-greater.json](list-position-compare-greater.json)
- [list-position-compare-greaterorequal.json](list-position-compare-greaterorequal.json)
- [list-position-compare-less.json](list-position-compare-less.json)
- [list-position-compare-lessorequal.json](list-position-compare-lessorequal.json)
- [list-position-compare-notequal.json](list-position-compare-notequal.json)
- [list-position-deck-first.json](list-position-deck-first.json)
- [list-position-deck-last.json](list-position-deck-last.json)
- [list-position-defaults.json](list-position-defaults.json)
- [list-position-discard-bottom-five.json](list-position-discard-bottom-five.json)
- [list-position-discard-first.json](list-position-discard-first.json)
- [list-position-discard-last.json](list-position-discard-last.json)
- [open-lane-for-composite-live-query.json](open-lane-for-composite-live-query.json)
- [open-lane-for-picked-definition.json](open-lane-for-picked-definition.json)
- [open-lane-for-picked-entity.json](open-lane-for-picked-entity.json)
- [picked-from-three-guid.json](picked-from-three-guid.json)
- [self-leveled-three.json](self-leveled-three.json)

## Set vs. rarity

`HasSetQuery` uses the card_data **top-level `set` metadata**, not the card's Tags/Subtypes or a fictional Set component. The comparison is case-sensitive: `"Set3"`, `"Token"`, etc. The metadata is captured from the game's `CardDataMapper.SetCardData` using the existing shared detour; if a GUID has not been loaded, it does **not** match.

`HasRarityQuery` reads the **live `PvZCards.Engine.Components.Rarity.Value` component** from the candidate card. Supported string names are `R1`, `R2`, `R3`, `R4`, `R0`, and `Event` (these are not the integer values in `CardData.Rarity`). If the component is missing or the value is unknown, it does **not** match. Both queries work as normal `EntityQuery` objects in target filters.
