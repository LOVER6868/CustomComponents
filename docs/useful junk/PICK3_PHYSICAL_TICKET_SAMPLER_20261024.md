# PickFromThreeUnique physical-ticket sampler (2026-10-24)

SelectionType 4 samples exactly one ticket per eligible, distinct live Entity.Id. Each ticket is one physical card copy. It draws uniformly, then temporarily removes *all other tickets with that GUID* for the remainder of the same three-option offer. That temporary vector is destroyed on return. No offer- or match-global GUID exclusion is saved. There is no explicit stack weighting calculation.

This intentionally displays at most one option per GUID. With just two distinct *eligible* GUIDs, the offer has two options, even if the two GUIDs have many physical copies. It does not fabricate target IDs or mutate the deck. It leaves mode 3 and mode 5 alone.

**Outstanding separate issue:** the supplied live log has `physicalEntries=71 liveEntries=8 missingIds=63 stockEligible=4` (then 67/7/60/3). The sampler never sees those missing-ID deck entries. This patch does NOT repair DeckMaterializer or guarantee full initial deck participation. The same-model materialization repair remains present but the log does not demonstrate successful repair. Investigate where copies become virtual in the queried match model.

Preserved previous highlight crash hotfix, avatar art, CardDatabase PvP, and experimental same-model deck repair. Host unit test updated to check per-offer GUID exclusion and natural copy-ticket probability, rather than expecting 3 distinct GUIDs when only 2 exist.
