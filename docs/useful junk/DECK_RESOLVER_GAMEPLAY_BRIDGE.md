# Shared deck resolver: gameplay bridge

`DeckEntries::ResolveSaved` revalidates owner, pool ordinal, pool position, GUID and entity ID against a fresh snapshot. `ResolveEntity` maps an existing stock target ID to **exactly one** card-pool entry in the requested deck; duplicate or cross-owner entity IDs are rejected. Virtual entries return `VirtualEntry`, not a fabricated entity ID.

`Queries::ResolveDeckEntityTarget` and `Queries::RevalidateDeckSelection` work against the executing system's exact `EntityModel`. `DiscardFromDeckSystem` now uses the shared identity resolver before delegating to stock deck/discard modifiers. No deck mutation or network serializer change occurs in this patch.

**Remaining multiplayer boundary:** `EntityFinder` only enumerates real `Entity` objects. GUID-only deck entries still cannot be stock `SelectTarget` targets. A multiplayer implementation requires an explicit deterministic choice transport and resolution on the server-side simulation; this patch intentionally does not encode a pool index as an entity ID.
