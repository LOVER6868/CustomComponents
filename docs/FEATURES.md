# Feature status

This is a **scope summary**, not a claim of universal regression coverage. Implementation details live in [architecture](../ARCHITECTURE.md) and source under `inc/`.

| Area | Implemented behavior | Important qualification |
| --- | --- | --- |
| Abilities | Armor (19), Charge (20), Thrash (21), GrantAbility serialization and lifecycle | Ability-specific timing and Team-Up edge cases need explicit testing |
| Deck identity | Persistent `EntityId` on main-deck `CardPoolEntry` across mulligan | Preserve stock card ownership and entry order |
| Queries | `InDeckQuery`, `InDiscardPileQuery`, Guid-based zone queries, `DiscardedFromQuery` | `DiscardedFromQuery.Source` supports numeric `1`/`2` in the tested baseline; string aliases are not established |
| Ordered targeting | Custom `ListPosition = 3` sort on deck/discard | Registered by `TargetScopeSupport` through the shared enum serializer |
| Draw | `DrawCardFromSubsetInDeck` against real deck entries | Stock CardDatabase-backed subset queries are a distinct domain |
| Discard / return | Deck/hand discards, discard pile removal, returns to hand/deck/lane | Repeated transitions and effect groups remain sensitive to lifecycle timing |
| Triggers | Custom named triggers, global/targeted fire, discard and return triggers | Must preserve trigger eligibility and queue semantics |
| Inspection | Discard pile shown through stock inspection UI | No custom Unity screen needed |
| Presentation | Local/opponent hand-discard reconciliation, lane-card stat correction, off-board trigger animation guarding | Opponent stat correction worked in subsequent testing; one mid-match device crash was observed but not attributed; animation stalls remain under investigation |
| Lifecycle | Guard against duplicate CardView creation and defer until safe retirement | Production coordinator is `core/CardViewLifecycleCoordinator.hpp`; the obsolete compatibility include was retired from the cleaned source |

## Regression scenarios

When modifying code affecting a row above, test at least: match startup on physical phone, both client viewpoints, discard from hand/deck, return to hand/deck/lane, lane → discard → lane, rapid repeat returns, stock played Trick reveal, stock buffs, and animation queue advancement.

See [known issues](KNOWN_ISSUES.md) before modifying anything in presentation or lifecycle code.

## Experimental deck positioning

`MoveCardInDeckEffectDescriptor` changes the position of a targeted live deck card using `Mode` (`Add` / `Move`) and signed `Position`. See [reference](reference/move-card-in-deck.md). Runtime testing is pending.
