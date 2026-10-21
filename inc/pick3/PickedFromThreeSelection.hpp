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
        if (card.entityId == -1 && cardGuid > 0 && card.cardGuid == cardGuid) return true;
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
