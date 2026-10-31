#pragma once
// Read-only ordered-deck snapshot and identity validation. No entity creation,
// queue movement, or PvP transport changes occur in this file.
#include <cstddef>
#include <vector>

namespace CustomComponents::Queries::DeckEntries {

// Identity is scoped to one model snapshot and its ordered pool. An index is
// NOT a globally stable identity and must be revalidated before use.
struct Selection {
    int ownerEntityId = -1;
    int poolOrdinal = -1; // OrderedCardPool component ordinal on owner
    int entryIndex = -1;
    int guid = -1;
    int entityId = -1; // -1 means virtual, not missing/corrupt
};

// Result codes are explicit: a GUID-only entry cannot be passed to a stock
// entity-ID target or modifier, even when the GUID is otherwise selectable.
enum class ResolveStatus { FoundEntity, VirtualEntry, EntryChanged, AmbiguousEntity, InvalidSelection };
struct ResolveResult {
    ResolveStatus status = ResolveStatus::InvalidSelection;
    Selection entry{};
    int entityId = -1;
};

// Read-only lookup with owner+pool+index+GUID+entity-id revalidation. No
// materialization and no fallback to matching the GUID of an entity elsewhere.
inline ResolveResult ResolveSaved(const std::vector<Selection>& current,
                                  const Selection& saved) {
    if (saved.ownerEntityId < 0 || saved.poolOrdinal < 0 ||
        saved.entryIndex < 0 || saved.guid <= 0 || saved.entityId < -1)
        return {};
    for (const auto& entry : current) {
        if (entry.ownerEntityId != saved.ownerEntityId ||
            entry.poolOrdinal != saved.poolOrdinal ||
            entry.entryIndex != saved.entryIndex) continue;
        if (entry.guid != saved.guid || entry.entityId != saved.entityId)
            return {ResolveStatus::EntryChanged, entry, -1};
        if (entry.entityId < 0)
            return {ResolveStatus::VirtualEntry, entry, -1};
        int matches = 0;
        for (const auto& other : current)
            if (other.entityId == entry.entityId) ++matches;
        if (matches != 1)
            return {ResolveStatus::AmbiguousEntity, entry, -1};
        return {ResolveStatus::FoundEntity, entry, entry.entityId};
    }
    return {ResolveStatus::EntryChanged, {}, -1};
}

// Map a stock entity target back to exactly one physical card in the selected
// player's deck. Never use GUID equality for identity. All owners are scoped
// to the same model snapshot supplied by the caller.
inline ResolveResult ResolveEntity(const std::vector<Selection>& current,
                                   int deckOwnerEntityId, int entityId) {
    if (deckOwnerEntityId < 0 || entityId < 0) return {};
    const Selection* matched = nullptr;
    for (const auto& entry : current) {
        if (entry.entityId != entityId) continue;
        if (entry.ownerEntityId != deckOwnerEntityId)
            return {ResolveStatus::AmbiguousEntity, entry, -1};
        if (matched) return {ResolveStatus::AmbiguousEntity, entry, -1};
        matched = &entry;
    }
    if (!matched) return {};
    return {ResolveStatus::FoundEntity, *matched, entityId};
}

// Model-independent physical identity for a separately authenticated deck
// command. Entity IDs MUST be resolved in the receiving simulation; they may
// differ from the sender's allocations or be virtual (-1). This operation
// never materializes cards or passes virtual IDs to stock SelectTarget.
inline ResolveResult ResolveAcrossModels(const std::vector<Selection>& current,
                                         int authenticatedOwnerId,
                                         int poolOrdinal, int entryIndex, int guid) {
    if(authenticatedOwnerId<0 || poolOrdinal<0 || entryIndex<0 || guid<=0) return {};
    const Selection* match=nullptr;
    for(const auto& entry : current) {
        if(entry.ownerEntityId!=authenticatedOwnerId ||
           entry.poolOrdinal!=poolOrdinal || entry.entryIndex!=entryIndex) continue;
        if(match) return {ResolveStatus::AmbiguousEntity,entry,-1};
        match=&entry;
    }
    if(!match || match->guid!=guid || match->entityId < -1)
        return {ResolveStatus::EntryChanged,{},-1};
    if(match->entityId==-1)
        return {ResolveStatus::VirtualEntry,*match,-1};
    const auto live=ResolveEntity(current,authenticatedOwnerId,match->entityId);
    if(live.status!=ResolveStatus::FoundEntity)
        return {ResolveStatus::AmbiguousEntity,*match,-1};
    return {ResolveStatus::FoundEntity,*match,match->entityId};
}

inline bool SamePhysicalEntry(const Selection& expected, const Selection& now) {
    return expected.ownerEntityId == now.ownerEntityId &&
           expected.poolOrdinal == now.poolOrdinal &&
           expected.entryIndex == now.entryIndex &&
           expected.guid > 0 && expected.guid == now.guid &&
           expected.entityId == now.entityId;
}

// Separate snapshot identity from matching a card definition. A pool index
// alone is insufficient after shuffles, draws, or inserts.
inline bool Validate(const std::vector<Selection>& current, const Selection& expected) {
    for (const auto& entry : current)
        if (SamePhysicalEntry(expected, entry)) return true;
    return false;
}

inline std::vector<Selection> ForOwner(const std::vector<Selection>& current, int ownerEntityId) {
    std::vector<Selection> result;
    for (const auto& entry : current)
        if (entry.ownerEntityId == ownerEntityId) result.push_back(entry);
    return result;
}

inline int CountCopies(const std::vector<Selection>& current, int ownerEntityId, int guid) {
    int count = 0;
    for (const auto& entry : current)
        if (entry.ownerEntityId == ownerEntityId && entry.guid == guid) ++count;
    return count;
}

} // namespace CustomComponents::Queries::DeckEntries
