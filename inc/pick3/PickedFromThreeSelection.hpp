#pragma once
// Scoped selection results shared by PickFromThree submission and downstream
// EntityQuery.Matches. CardDatabase GUIDs are NOT live Entity.Id values.
// No winner is exposed until an actual UI selection has been validated.
// Matches are keyed by originating effect entity, not a global last pick.
// The latest pick per effect persists within the current match. Match end clears it.

#include <algorithm>
#include <cstdint>
#include <mutex>
#include <vector>

namespace CustomComponents::Pick3::Picked {

struct SelectedCard {
    int entityId = -1;
    int cardGuid = -1;
    int boundEntityId = -1; // Verified stock-produced card for a database selection.
};
struct Selection {
    int effectEntityId = -1;
    int effectGroupId = -1;
    std::vector<SelectedCard> cards;
};
inline std::mutex mutex;
inline std::vector<Selection> selections;

inline bool Store(int effectId, int groupId, const std::vector<SelectedCard>& cards) {
    if (effectId <= 0 || groupId < 0 || cards.empty() || cards.size() > 3)
        return false;
    for (const auto& card : cards) {
        if (card.entityId == 0 || card.entityId < -1 || card.cardGuid <= 0) return false;
    }
    std::lock_guard<std::mutex> guard(mutex);
    // Exactly one latest result per source effect. Repeated activations may
    // allocate new group IDs, so matching both effect and group would leave
    // stale entries and eventually hit the old 64-entry rejection limit.
    // Keep the result in process memory; downstream queries still resolve by
    // effect ID so an unrelated effect cannot consume another one's choice.
    selections.erase(std::remove_if(selections.begin(), selections.end(),
        [&](const Selection& old) { return old.effectEntityId == effectId; }),
        selections.end());
    // Bound the cache if many distinct source effects fire within one match.
    // Replace the oldest result rather than failing a valid new selection.
    constexpr std::size_t kMaxRememberedEffects = 64;
    if (selections.size() >= kMaxRememberedEffects)
        selections.erase(selections.begin());
    selections.push_back({effectId, groupId, cards});
    return true;
}

inline void Erase(int effectId, int groupId) {
    std::lock_guard<std::mutex> guard(mutex);
    selections.erase(std::remove_if(selections.begin(), selections.end(),
        [&](const Selection& old) {
            return old.effectEntityId == effectId && old.effectGroupId == groupId;
        }), selections.end());
}

// Cross-effect lookup for post-pick effects such as Captain Cucumber.
// The newest successful selection is authoritative until replaced or match end.
// Database choices do not have a live Entity.Id, so those compare card GUIDs.
inline bool MatchesLatestLiveCard(int entityId, int cardGuid) {
    if (entityId <= 0 && cardGuid <= 0) return false;
    std::lock_guard<std::mutex> guard(mutex);
    if (selections.empty()) return false;
    const Selection& latest = selections.back();
    for (const auto& card : latest.cards) {
        if (card.entityId > 0 && entityId > 0 && card.entityId == entityId) return true;
        if (card.entityId == -1 && card.boundEntityId > 0 &&
            entityId > 0 && card.boundEntityId == entityId) return true;
    }
    return false;
}

// During stock GUID lookup, the query supplies the originating effect ID.
// This must be observed inside the finder's call stack, not inferred from
// a matching GUID or the newest selection.
inline thread_local int finderSelectionEffect = -1;
inline thread_local int finderSelectionGuid = -1;
inline thread_local unsigned finderSelectionMatches = 0;
inline thread_local bool finderObserving = false;

inline void ObserveGuidMatch(int effectId, int guid) {
    if (!finderObserving || effectId <= 0 || guid <= 0) return;
    ++finderSelectionMatches;
    if (finderSelectionMatches == 1) {
        finderSelectionEffect = effectId;
        finderSelectionGuid = guid;
    } else if (finderSelectionEffect != effectId || finderSelectionGuid != guid) {
        finderSelectionEffect = -1; // Ambiguous provenance, fail closed.
    }
}

inline bool BindCreated(int sourceEffect, int guid, int createdId) {
    if (sourceEffect <= 0 || guid <= 0 || createdId <= 0) return false;
    std::lock_guard<std::mutex> guard(mutex);
    for (auto& entry : selections) {
        if (entry.effectEntityId != sourceEffect) continue;
        for (auto& card : entry.cards) {
            if (card.cardGuid == guid && card.entityId == -1) {
                if (card.boundEntityId > 0 && card.boundEntityId != createdId)
                    return false; // Do not overwrite a distinct produced instance.
                card.boundEntityId = createdId;
                return true;
            }
        }
    }
    return false;
}

// Resolve the single, actual live entity from this exact transaction.
// This never returns a CardDatabase definition GUID or synthesized entity.
inline bool TryGetSingleLiveEntity(int effectId, int groupId, int& entityId, int& cardGuid) {
    entityId = -1;
    cardGuid = -1;
    if (effectId <= 0 || groupId < 0) return false;
    std::lock_guard<std::mutex> guard(mutex);
    for (const auto& entry : selections) {
        if (entry.effectEntityId != effectId || entry.effectGroupId != groupId) continue;
        if (entry.cards.size() != 1) return false;
        const auto& card = entry.cards.front();
        if (card.entityId <= 0 || card.cardGuid <= 0 || card.boundEntityId != -1)
            return false;
        entityId = card.entityId;
        cardGuid = card.cardGuid;
        return true;
    }
    return false;
}

// Resolve a single database-created entity for this exact completed selection.
// Never substitute a regular PickFromThree target or a CardDatabase GUID.
// Multiple cards, missing binding, or mismatched effect/group fail closed.
inline bool TryGetSingleBoundDatabaseEntity(int effectId, int groupId, int& entityId) {
    entityId = -1;
    if (effectId <= 0 || groupId < 0) return false;
    std::lock_guard<std::mutex> guard(mutex);
    for (const auto& entry : selections) {
        if (entry.effectEntityId != effectId || entry.effectGroupId != groupId) continue;
        if (entry.cards.size() != 1) return false;
        const SelectedCard& card = entry.cards.front();
        if (card.entityId != -1 || card.cardGuid <= 0 || card.boundEntityId <= 0)
            return false;
        entityId = card.boundEntityId;
        return true;
    }
    return false;
}

inline void ResetMatch() {
    // Never carry a result into another match, where entity IDs can be reused.
    // During a match results are only replaced by Store(), not consumed
    // or cleared when the preview closes.
    std::lock_guard<std::mutex> guard(mutex);
    selections.clear();
}

// Query evaluation can know its originating effect ID, but some stock
// CardFromSubsetFinder contexts do not expose an effect group. If there are
// multiple groups for an effect, NEVER guess which group was picked.
inline bool Matches(int effectId, int id, bool guidDomain) {
    if (effectId <= 0 || id <= 0) return false;
    std::lock_guard<std::mutex> guard(mutex);
    const Selection* selected = nullptr;
    for (const auto& entry : selections) {
        if (entry.effectEntityId != effectId) continue;
        if (selected) return false; // ambiguous: fail closed
        selected = &entry;
    }
    if (!selected) return false;
    for (const auto& card : selected->cards)
        if ((guidDomain ? card.cardGuid : card.entityId) == id) return true;
    return false;
}

} // namespace CustomComponents::Pick3::Picked
