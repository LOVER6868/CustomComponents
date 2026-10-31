# PvP query-time materialization safety rollback

The Oct 23 log showed `PVP_FACTORY_OBSERVED`, followed by `MODEL_REPAIR ... created=64` and `LIVE_DECK_REPAIRED ... remote=0 created=64`. The subsequent client-authoritative PvP selection submitted entity ID 189. The session flag printed remote=0 at finder time but remote=1 at submission time. The opponent then showed a generic game problem, consistent with simulation mismatch; there is no authoritative server rejection recorded in the log.

This patch removes bulk deck repair from the target-finder hook unconditionally. It preserves the read-only DeckEntryResolver, InDeckQuery, existing exact entity targeting, and all CardDatabase/InDeckGuid paths. The UniqueGuidTargets picker may return fewer than 3 options again until a validated deterministic virtual-entry execution protocol is implemented.

Log expected: `LIVE_DECK_REPAIR_DISABLED`. Log not expected from finder: `MODEL_REPAIR` or `LIVE_DECK_REPAIRED`. If those still occur, identify another call site. Other pre-existing DeckInitializer materialization and stock RemoveExtraCards suppression are not changed by this targeted regression rollback; audit those before declaring PvP fully safe.
