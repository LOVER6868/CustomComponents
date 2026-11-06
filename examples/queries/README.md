# Queries

Physical identity, GUID membership, discard provenance, ordered pools, and placement.

See the [insertion guide](../README.md) before copying a fragment.

| Example | Shape | Purpose |
| --- | --- | --- |
| [bottom-five-deck.json](bottom-five-deck.json) | Query | Deck: position LessOrEqual 4, counted from the end. |
| [can-be-placed-any-lane.json](can-be-placed-any-lane.json) | Query | Can be placed any lane (`CanBePlacedQuery`). |
| [can-be-placed-non-water-lane.json](can-be-placed-non-water-lane.json) | Query | Can be placed non water lane (`CanBePlacedQuery`). |
| [deck-except-top.json](deck-except-top.json) | Query | Deck except top (`CompositeAllQuery`). |
| [destroyed-in-play-in-discard.json](destroyed-in-play-in-discard.json) | Query | Destroyed in play in discard (`CompositeAllQuery`). |
| [discarded-from-deck.json](discarded-from-deck.json) | Query | Discarded from deck (`DiscardedFromQuery`). |
| [discarded-from-hand.json](discarded-from-hand.json) | Query | Discarded from hand (`DiscardedFromQuery`). |
| [discarded-from-query.json](discarded-from-query.json) | Query | Discarded from query (`DiscardedFromQuery`). |
| [in-deck-guid.json](in-deck-guid.json) | Query | In deck guid (`InDeckGuidQuery`). |
| [in-deck.json](in-deck.json) | Query | In deck (`InDeckQuery`). |
| [in-discard-pile-guid-query.json](in-discard-pile-guid-query.json) | Component | In discard pile guid query (`DrawCardFromSubsetEffectDescriptor`). |
| [in-discard-pile-guid.json](in-discard-pile-guid.json) | Query | In discard pile guid (`InDiscardPileGuidQuery`). |
| [in-discard-pile.json](in-discard-pile.json) | Query | In discard pile (`InDiscardPileQuery`). |
| [list-position-compare-equal.json](list-position-compare-equal.json) | Query | Deck: position Equal 2, counted from the start. |
| [list-position-compare-greater.json](list-position-compare-greater.json) | Query | Deck: position Greater 2, counted from the start. |
| [list-position-compare-greaterorequal.json](list-position-compare-greaterorequal.json) | Query | Deck: position GreaterOrEqual 2, counted from the start. |
| [list-position-compare-less.json](list-position-compare-less.json) | Query | Deck: position Less 2, counted from the start. |
| [list-position-compare-lessorequal.json](list-position-compare-lessorequal.json) | Query | Deck: position LessOrEqual 2, counted from the start. |
| [list-position-compare-notequal.json](list-position-compare-notequal.json) | Query | Deck: position NotEqual 2, counted from the start. |
| [list-position-deck-bottom-five.json](list-position-deck-bottom-five.json) | Query | Deck: position LessOrEqual 4, counted from the end. |
| [list-position-deck-first.json](list-position-deck-first.json) | Query | Deck: position Equal 0, counted from the start. |
| [list-position-deck-last.json](list-position-deck-last.json) | Query | Deck: position Equal 0, counted from the end. |
| [list-position-defaults.json](list-position-defaults.json) | Query | Deck: position Equal 0, counted from the start. |
| [list-position-discard-bottom-five.json](list-position-discard-bottom-five.json) | Query | DiscardPile: position LessOrEqual 4, counted from the end. |
| [list-position-discard-first.json](list-position-discard-first.json) | Query | DiscardPile: position Equal 0, counted from the start. |
| [list-position-discard-last.json](list-position-discard-last.json) | Query | DiscardPile: position Equal 0, counted from the end. |
| [open-lane-for-composite-live-query.json](open-lane-for-composite-live-query.json) | Query | Open lane for composite live query (`OpenLaneForCardQuery`). |
| [open-lane-for-picked-definition.json](open-lane-for-picked-definition.json) | Query | Open lane for picked definition (`OpenLaneForCardQuery`). |
| [open-lane-for-picked-entity.json](open-lane-for-picked-entity.json) | Query | Open lane for picked entity (`OpenLaneForCardQuery`). |
| [picked-from-three-entity.json](picked-from-three-entity.json) | Query | Picked from three entity (`PickedFromThreeEntityQuery`). |
| [picked-from-three-guid.json](picked-from-three-guid.json) | Query | Picked from three guid (`PickedFromThreeQuery`). |
