#pragma once

// Pure, dependency-free PickFromThree candidate selection.
// Game/BNM integration supplies the candidates and the authoritative RNG.
// No Unity objects or pointers are owned or retained here.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <random>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace CustomComponents::Pick3 {

inline constexpr std::size_t kMaximumChoices = 3;
inline constexpr int kNoEntity = -1;
inline constexpr int kNoCardGuid = -1;

enum class SelectionMode : std::uint8_t {
    RandomTargets = 0,     // Distinct eligible LIVE entities; repeated card GUIDs allowed.
    UniqueGuidTargets = 1, // Weight by eligible live copies; at most one option per GUID.
    CardDatabase = 2       // Distinct card definitions; no live entity is required.
};

// Stock recipient IDs and selectable live-card IDs have different domains.
// The local Plant player can have Entity.Id 0. The UI never selects that ID.
inline constexpr bool ValidCompletionTargetId(SelectionMode mode, int entityId) {
    return mode == SelectionMode::CardDatabase ? entityId >= 0 : entityId > 0;
}

struct Candidate {
    int entityId = kNoEntity; // > 0 for live entities; -1 for database definitions.
    int cardGuid = kNoCardGuid; // > 0 for cards; -1 for a non-card live target.
};

inline bool IsUsable(const Candidate& candidate, SelectionMode mode) {
    if (mode == SelectionMode::CardDatabase)
        return candidate.cardGuid > 0;
    if (mode == SelectionMode::UniqueGuidTargets)
        return candidate.entityId > 0 && candidate.cardGuid > 0;
    return candidate.entityId > 0;
}

// The input MUST already be restricted by stock query/faction/zone/condition
// rules. Do not treat this sampler as a substitute for target eligibility.
// UniqueGuidTargets weights GUIDs by eligible live copies and removes the
// entire GUID after each draw. Other modes sample their normalized set uniformly.
// `rng` must be synchronized/authoritative in multiplayer; avoid wall-clock seeds.
template<class UniformRandomBitGenerator>
std::vector<Candidate> SelectOptions(
    SelectionMode mode,
    const std::vector<Candidate>& eligible,
    UniformRandomBitGenerator& rng)
{
    std::vector<Candidate> normalized;
    normalized.reserve(eligible.size());

    if (mode == SelectionMode::CardDatabase) {
        std::unordered_set<int> seenGuids;
        for (const auto& candidate : eligible) {
            if (!IsUsable(candidate, mode) || !seenGuids.insert(candidate.cardGuid).second)
                continue;
            normalized.push_back({kNoEntity, candidate.cardGuid});
        }
    } else if (mode == SelectionMode::UniqueGuidTargets) {
        // Aggregate eligible live copies by GUID. Each distinct entity is
        // counted once; its stack size is the sampling weight. Once selected,
        // remove the whole stack, so a GUID cannot appear twice in one offer.
        struct Stack { int guid; std::vector<Candidate> copies; };
        std::vector<Stack> stacks;
        std::unordered_map<int, std::size_t> byGuid;
        std::unordered_set<int> seenEntities;
        for (const auto& candidate : eligible) {
            if (!IsUsable(candidate, mode) || !seenEntities.insert(candidate.entityId).second)
                continue;
            auto found = byGuid.find(candidate.cardGuid);
            if (found == byGuid.end()) {
                byGuid.emplace(candidate.cardGuid, stacks.size());
                stacks.push_back({candidate.cardGuid, {}});
                found = byGuid.find(candidate.cardGuid);
            }
            stacks[found->second].copies.push_back(candidate);
        }
        std::vector<Candidate> chosen;
        chosen.reserve(kMaximumChoices);
        while (!stacks.empty() && chosen.size() < kMaximumChoices) {
            std::vector<double> weights;
            weights.reserve(stacks.size());
            for (const auto& stack : stacks)
                weights.push_back(static_cast<double>(stack.copies.size()));
            std::discrete_distribution<std::size_t> stackDraw(weights.begin(), weights.end());
            const std::size_t stackIndex = stackDraw(rng);
            const auto& copies = stacks[stackIndex].copies;
            std::uniform_int_distribution<std::size_t> copyDraw(0, copies.size() - 1);
            chosen.push_back(copies[copyDraw(rng)]);
            stacks.erase(stacks.begin() + static_cast<std::ptrdiff_t>(stackIndex));
        }
        return chosen;
    } else if (mode == SelectionMode::RandomTargets) {
        std::unordered_set<int> seenEntities;
        for (const auto& candidate : eligible) {
            if (IsUsable(candidate, mode) && seenEntities.insert(candidate.entityId).second)
                normalized.push_back(candidate);
        }
    } else {
        return {}; // Unknown mode fails closed.
    }

    // Partial Fisher-Yates shuffle: exactly 0..min(N,3)-1 slots selected.
    const std::size_t count = std::min(kMaximumChoices, normalized.size());
    for (std::size_t i = 0; i < count; ++i) {
        std::uniform_int_distribution<std::size_t> draw(i, normalized.size() - 1);
        std::swap(normalized[i], normalized[draw(rng)]);
    }
    normalized.resize(count);
    return normalized;
}

} // namespace CustomComponents::Pick3

// ---- merged PickFromThreeEnumModes.hpp ----
// Public TargetSelectionType extensions. Values 0-2 belong to stock PvZ Heroes.
namespace CustomComponents::Pick3::EnumModes {
inline constexpr int kStockAll = 0, kStockManual = 1, kStockRandom = 2;
inline constexpr int kPickFromThree = 3;
inline constexpr int kPickFromThreeUniqueGuidTargets = 4;
inline constexpr int kPickFromThreeCardDatabase = 5;
inline constexpr bool IsLivePickFromThree(int value) {
    return value == kPickFromThree || value == kPickFromThreeUniqueGuidTargets;
}
inline constexpr bool IsDatabasePick(int value) { return value == kPickFromThreeCardDatabase; }
inline constexpr bool IsPickFromThree(int value) {
    return IsLivePickFromThree(value) || IsDatabasePick(value);
}
inline constexpr SelectionMode Resolve(int value, SelectionMode /*legacy*/) {
    return value == kPickFromThreeUniqueGuidTargets ? SelectionMode::UniqueGuidTargets
         : value == kPickFromThreeCardDatabase ? SelectionMode::CardDatabase
         : SelectionMode::RandomTargets;
}
}

// ---- merged PickFromThreeCandidateSources.hpp ----
// The per-mode sampler from Step 1 is the sole owner of the three-option rule.
// This adapter turns already-validated game targets into a stable, inspectable
// preview request. It intentionally does NOT return three chosen effect targets.

namespace CustomComponents::Pick3 {
struct PreviewRequest {
    int effectEntityId = -1;
    int effectGroupId = -1;
    SelectionMode mode = SelectionMode::RandomTargets;
    int requiredTargets = 1;
    std::vector<Candidate> options;
    // The activating card, NEVER one of the three candidates. Used only for
    // the prefab-specific localized UI prompt.
    int sourceCardGuid = -1;
    // Effect.PlayerId: the selecting player, not the side receiving the Play.
    // -1 means unknown. This is kept with the request through the UI delay.
    int ownerPlayerId = -1;
};

// Only used for the READ-ONLY Step 2 device probe. Real gameplay must use the
// engine's replicated MtRandom/authoritative selection, not this deterministic
// local preview seed. A nonce makes successive effects differ even with the
// same eligible input; the contents of a request are stable once sampled.
inline PreviewRequest MakePreviewRequest(int effectEntityId,
                                         int effectGroupId,
                                         SelectionMode mode,
                                         const std::vector<Candidate>& eligible,
                                         std::uint32_t probeNonce,
                                         int requiredTargets = 1) {
    std::uint32_t seed = 2166136261u;
    auto add = [&](std::uint32_t word) { seed = (seed ^ word) * 16777619u; };
    add(static_cast<std::uint32_t>(effectEntityId));
    add(static_cast<std::uint32_t>(effectGroupId));
    add(probeNonce);
    for (const auto& candidate : eligible) {
        add(static_cast<std::uint32_t>(candidate.entityId));
        add(static_cast<std::uint32_t>(candidate.cardGuid));
    }
    std::mt19937 probeRng(seed);
    return PreviewRequest{effectEntityId, effectGroupId, mode, requiredTargets,
                          SelectOptions(mode, eligible, probeRng)};
}
// Canonicalize candidates and remove the local nonce for experimental
// cross-client tests. This removes a known source of client-to-client choice
// divergence when both have identical authoritative eligible entity IDs.
// IT IS NOT A NETWORK PROTOCOL: the server still cannot verify the offered
// three options, and CardDatabase GUIDs cannot be carried by stock Play.
inline PreviewRequest MakeStableNetworkPreviewRequest(int effectEntityId,
                                                      int effectGroupId,
                                                      SelectionMode mode,
                                                      const std::vector<Candidate>& eligible,
                                                      int requiredTargets = 1) {
    auto canonical = eligible;
    std::sort(canonical.begin(), canonical.end(), [](const Candidate& a, const Candidate& b) {
        if (a.entityId != b.entityId) return a.entityId < b.entityId;
        return a.cardGuid < b.cardGuid;
    });
    return MakePreviewRequest(effectEntityId, effectGroupId, mode, canonical, 0u, requiredTargets);
}
} // namespace CustomComponents::Pick3

// ---- merged PickFromThreeAccumulation.hpp ----
// Three visible options, but 1..3 required distinct runtime entity targets.
// This pure state helper does not mutate the game or dispatch a Play.

namespace CustomComponents::Pick3 {
class PickAccumulator {
public:
    bool Begin(int available, int required) {
        Reset();
        if (available < 1 || available > 3 || required < 1 || required > available) return false;
        available_ = available; required_ = required;
        return true;
    }
    void Reset() { available_ = required_ = 0; chosen_.clear(); sealed_ = false; }
    bool Add(int index) {
        if (sealed_ || required_ < 1 || index < 0 || index >= available_ ||
            static_cast<int>(chosen_.size()) >= required_ || Has(index)) return false;
        chosen_.push_back(index);
        return true;
    }
    bool Has(int index) const {
        return std::find(chosen_.begin(), chosen_.end(), index) != chosen_.end();
    }
    int Count() const { return static_cast<int>(chosen_.size()); }
    int Required() const { return required_; }
    bool Complete() const { return required_ > 0 && Count() == required_; }
    bool Sealed() const { return sealed_; }
    bool Seal() { if (sealed_ || !Complete()) return false; sealed_ = true; return true; }
    const std::vector<int>& Indices() const { return chosen_; }
    bool FillUnchosen() {
        if (sealed_ || !required_) return false;
        for (int i = 0; i < available_ && !Complete(); ++i)
            if (!Has(i)) chosen_.push_back(i);
        return Complete();
    }
private:
    int available_ = 0, required_ = 0;
    bool sealed_ = false;
    std::vector<int> chosen_;
};
}

// ---- merged PickFromThreeTransaction.hpp ----

// Gameplay-independent state contract for a SINGLE PickFromThree request.
// This is not an effect hook, trigger producer, or gameplay query yet.
// A future per-effect manager will own one transaction for each pending effect.



namespace CustomComponents::Pick3 {

enum class Phase : std::uint8_t {
    Idle, AwaitingSelection, Selected, ResolvingPayout,
    ReadyToDispatchTrigger, Finished, Cancelled
};

// A unique gameplay-defined identity is mandatory when integrating multiple
// pending effects. Never key this by a global 'last picked GUID' variable.
struct TransactionKey {
    std::uint64_t matchGeneration = 0;
    std::uint64_t effectToken = 0;
};

class SelectionTransaction {
public:
    Phase State() const { return phase_; }
    const TransactionKey& Key() const { return key_; }
    const std::vector<Candidate>& Options() const { return options_; }
    std::size_t CompletedPayouts() const { return completedPayouts_; }
    std::size_t ExpectedPayouts() const { return expectedPayouts_; }

    // Only legal before any live selection. A transaction with zero available
    // choices is cancelled, not made into a three-option duplicate.
    bool Begin(TransactionKey key, SelectionMode mode,
               std::vector<Candidate> options, std::size_t expectedPayouts = 1) {
        if (phase_ != Phase::Idle || key.effectToken == 0 ||
            options.empty() || options.size() > kMaximumChoices || expectedPayouts == 0)
            return false;
        for (const Candidate& option : options)
            if (!IsUsable(option, mode)) return false;
        key_ = key;
        mode_ = mode;
        options_ = std::move(options);
        expectedPayouts_ = expectedPayouts;
        phase_ = Phase::AwaitingSelection;
        return true;
    }

    // Choice is committed only by the authoritative gameplay selection.
    bool Choose(std::size_t optionIndex) {
        if (phase_ != Phase::AwaitingSelection || optionIndex >= options_.size())
            return false;
        chosen_ = options_[optionIndex];
        phase_ = Phase::Selected;
        return true;
    }

    // Queries are unavailable before Choose, and after final release.
    // An auto-targeting effect may consume SelectedEntityId without a query;
    // a subset/draw effect can consume SelectedCardGuid through a future query.
    bool SelectedEntityId(int& outEntityId) const {
        if (!HasCommittedChoice() || chosen_.entityId <= 0) return false;
        outEntityId = chosen_.entityId;
        return true;
    }
    bool SelectedCardGuid(int& outCardGuid) const {
        if (!HasCommittedChoice() || chosen_.cardGuid <= 0) return false;
        outCardGuid = chosen_.cardGuid;
        return true;
    }

    bool StartPayout() {
        if (phase_ != Phase::Selected) return false;
        phase_ = Phase::ResolvingPayout;
        return true;
    }

    // CRITICAL: call only at the VERIFIED queue/group completion boundary,
    // AFTER ordinary child effects and responses have settled. Never from
    // the UI click handler, and never just because an effect returned early.
    bool PayoutSettled(bool successful) {
        if (phase_ != Phase::ResolvingPayout) return false;
        if (!successful) { phase_ = Phase::Cancelled; return true; }
        ++completedPayouts_;
        phase_ = completedPayouts_ >= expectedPayouts_
            ? Phase::ReadyToDispatchTrigger : Phase::Selected;
        return true;
    }

    bool MarkTriggerDispatched() {
        if (phase_ != Phase::ReadyToDispatchTrigger) return false;
        phase_ = Phase::Finished;
        return true;
    }
    void Cancel() {
        if (phase_ != Phase::Finished) phase_ = Phase::Cancelled;
    }
    void Reset() { *this = SelectionTransaction{}; }

private:
    bool HasCommittedChoice() const {
        return phase_ == Phase::Selected || phase_ == Phase::ResolvingPayout ||
               phase_ == Phase::ReadyToDispatchTrigger;
    }
    TransactionKey key_{};
    SelectionMode mode_ = SelectionMode::RandomTargets;
    Phase phase_ = Phase::Idle;
    std::vector<Candidate> options_;
    Candidate chosen_{};
    std::size_t expectedPayouts_ = 0;
    std::size_t completedPayouts_ = 0;
};

} // namespace CustomComponents::Pick3
