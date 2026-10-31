# Stage E InDeck regular-deck scope fix (2026-10-25)

## Problem
Stage E materialized 88/88 entries across four stock pools (1,2,6,7).
Before this fix, custom InDeckQuery::Matches and InDeckGuidQuery::Matches
scanned every OrderedCardPool in EntityModel, including the superpower pools.
Fisherman (Guid 709) uses Zombies + InDeckQuery, so zombie superpowers
were valid matches after full materialization.

## Scope of fix
Only inc/queries/InDeckQuery.hpp is modified. When inspecting an
OrderedCardPool, its owning entity must contain PvZCards.Engine.Components.Deck
and must NOT contain PvZCards.Engine.Components.SuperpowerPool.
This applies to both live EntityId membership and GUID-based membership.
No source-card exclusion or duplicate-GUID rule is introduced. The normal
main-deck entries are all still eligible, including selecting the source
card's own GUID if still in the main deck.

The module logs once during startup:
  CustomComponents/InDeckQuery: scope=REGULAR_DECK_ONLY requires=Deck excludes=SuperpowerPool variants=EntityId,Guid

No changes to card_data, Fisherman, PickFromThree, Relic Hunter, config.ini,
networking, or full-deck materialization.

## Test
Use the same Stage E FullDeckLab opt-in setting on both consented private
clients, then activate Fisherman. Expect regular zombie main-deck GUIDs
only; neither zombie superpower GUID 27,319,258,577 nor other superpowers
should appear. Verify full deck exact=1, virtual=0 on both devices and
compare combinedHash. New filter has not been tested on Android yet.

If a main-deck candidate disappears, inspect whether its OrderedCardPool
owner actually carries Deck (vs. SuperpowerPool). The query fails closed if
required component metadata is absent.
