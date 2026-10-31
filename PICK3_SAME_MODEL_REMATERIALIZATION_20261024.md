# Live Deck Same-Model Rematerialization (guarded test build)

- Retains native avatar art, highlighting crash hotfix, CardDatabase PvP, and other existing code.
- Roots the actual DeckInitializer-associated CardEntityFactory and EntityModel using existing GC handle infrastructure.
- On local PvE UniqueGuidTargets query, attempts to materialize missing OrderedCardPool IDs **only** when the factory Model is pointer-identical to the queried finder Model.
- Calls stock `FindValidTargetsFromEffect` **after** any such repair so it sees new entities through normal query validation.
- Refuses all remote PvP/other-model allocation, never invents entity IDs or manufactures preview targets.
- `CustomComponents/Pick3/LIVE_DECK_REPAIRED` reports successfully created entities.
- This is not a full solution for copied simulation models. When identities differ the code intentionally declines to mutate those models.
- Not Android device tested; host C++20 header syntax checked only.
