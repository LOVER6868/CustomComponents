# CustomComponents

Bloom PnP module for extending PvZ Heroes managed components, grantable abilities, and effect behavior.

Current custom grantable abilities:

- `Armor` = 19 — stock Armor component through the generic numeric grant pipeline; stock Armor presentation.
- `Charge` = 20 — custom managed Charge component using stock `GrantableAbility.Counters`, with block-meter gameplay integration.
- `Thrash` = 21 — custom flag-style `GrantableAbility`; grant/remove/duration and final overflow-damage gameplay are verified working.

Current framework areas:

- custom managed component construction and serialized type resolution,
- registry-driven GrantAbility serialization/lifecycle/presentation,
- custom gameplay systems such as Charge and Thrash,
- persistent entity-backed main decks,
- real JSON-visible `InDeckQuery` targeting,
- stock-compatible `DiscardFromDeck` using real deck entity IDs,
- real-deck `DrawCardFromSubsetInDeck` behavior,
- custom discard/return triggers and `Discarded` provenance tracking,
- selective off-board presentation synchronization for attack, health, and cost,
- `DiscardedFromQuery` with verified numeric `Source` and custom `DeepCopyFrom` dispatch,
- investigation/design work for first-class custom runtime `Effect` support.

See `ARCHITECTURE.md` for the module layout and `docs/` for the component/effect investigation record. Important engine findings are intentionally documented so completed systems do not need to be reverse-engineered again.

## Deck / discard roadmap

Current status (the original 0–7 outline was expanded during implementation; earlier documentation may use different Phase 4/5 labels):

- **Phase 0 — persistent entity-backed decks: complete**
- **Phase 1 — live `InDeckQuery` + `DiscardFromDeck`: complete**
- **Phase 2 — `ListPosition` targeting: complete**
- **Phase 3 — discard membership, Guid variants and viewer: complete**
- **Phase 4 — deck/discard effects, discard origin, return triggers and off-board presentation: complete for tested scope**
- **Phase 5 — next development phase: ready to begin; concrete feature scope to be defined before implementation**

Phase 4 ships with three **deferred issues**, not resolved bugs: intermittent targeting UI text/hand dimming, inconsistent attack-threshold trigger timing, and `DiscardedFromQuery.Source` string values causing loading failures. Numeric `Source` values (`1` = Hand, `2` = Deck) are the verified format. See `docs/PHASE4_OFFBOARD_ANIMATION_HANDOFF.md` and `docs/DECK_DISCARD_STATUS.md` before adding new hooks.

See `docs/PvZHeroes_Deck_Discard_Trigger_Architecture_Notes.md` for the original engine contracts and investigation history.
