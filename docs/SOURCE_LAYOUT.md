# Source layout

The three public umbrella headers live at `inc/CustomComponents.hpp`, `inc/DiscardPileZoneEffects.hpp`, and `inc/DrawCardFromSubsetInDeck.hpp`. `main.cpp` initializes modules. `inc/abilities/`, `inc/effects/`, `inc/queries/`, `inc/zones/`, `inc/triggers/`, and `inc/presentation/` retain their original responsibilities and gameplay hooks.

`inc/pick3/PickFromThreeCore.hpp` contains the pure sampler, enum definitions, transaction and accumulator, previously spread across five small headers. The other `inc/pick3/` files own the game bridge, selected GUID snapshots, stock service submission and optional history traces. The popup and card-art cache remain in `inc/presentation/`.

This cleanup does not remove other abilities or alter their startup order. Long investigation documents and obsolete JSON examples were intentionally omitted; keep the original dated CustomComponents ZIP for complete history. Never copy old `inc/pick3/` headers into this clean distribution.
