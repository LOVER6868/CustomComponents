# PvP exact-model factory registration

This patch observes the stock CardEntityFactory.CreateCardEntityWithDefaultOwner(int) call and records its own EntityModel/factory binding when a card is created. It lets the existing Pick3 same-model repair run in PvP models that were not observed by DeckInitializer.InitializeDeck. It does not alter CardDatabase, InDeckGuid, target serialization, or the sampler. It does NOT make arbitrary virtual deck entries valid stock SelectTarget IDs.

Diagnostics: PVP_FACTORY_OBSERVED, NO_MODEL_FACTORY, MODEL_REPAIR, LIVE_DECK_SKIPPED, LIVE_DECK_REPAIRED and LIVE_DECK_AUDIT. Verify that missingIds decreases on subsequent activations. This is not proof of deterministic server synchronization; test only in a controlled PvP setup and compare both clients.
