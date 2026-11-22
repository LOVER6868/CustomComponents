#pragma once
// Deterministic, engine-independent insertion guard used by the shared stock
// CreateCardInDeck hook. All pre-existing entry objects must survive unchanged;
// only genuinely new, virtual entries may be materialized.
#include <cstddef>
#include <cstdint>
#include <vector>

namespace CustomComponents::Diagnostics::PvpInsertionDelta {
struct Entry {
    const void* identity = nullptr;
    int guid = 0;
    int entityId = -1;
};
struct Delta {
    bool valid = false;
    const char* reason = "invalid";
    std::vector<std::size_t> newIndices;
};
inline Delta Compare(const std::vector<Entry>& before,
                     const std::vector<Entry>& after,
                     int expectedGuid,
                     int expectedAmount) {
    Delta result{};
    if (expectedGuid <= 0 || expectedAmount < 1 || expectedAmount > 64 ||
        before.size() > 1024 || after.size() > 1024) {
        result.reason = "invalid-input";
        return result;
    }
    if (after.size() != before.size() + static_cast<std::size_t>(expectedAmount)) {
        result.reason = "unexpected-count";
        return result;
    }
    std::vector<bool> seen(before.size(), false);
    for (std::size_t i = 0; i < after.size(); ++i) {
        const auto& candidate = after[i];
        if (!candidate.identity) {result.reason = "null-entry";return result;}
        std::size_t found = before.size();
        for (std::size_t j = 0; j < before.size(); ++j) {
            if (before[j].identity == candidate.identity) {
                if (found != before.size()) {result.reason = "ambiguous-identity";return result;}
                found = j;
            }
        }
        if (found == before.size()) {
            // Prevent a new object from appearing twice in the same pool.
            for (auto index : result.newIndices)
                if (after[index].identity == candidate.identity) {
                    result.reason = "duplicate-new-entry";
                    return result;
                }
            if (candidate.guid != expectedGuid || candidate.entityId != -1) {
                result.reason = "new-entry-not-expected-virtual";
                return result;
            }
            result.newIndices.push_back(i);
        } else {
            if (seen[found] || candidate.guid != before[found].guid ||
                candidate.entityId != before[found].entityId) {
                result.reason = "existing-entry-changed";
                return result;
            }
            seen[found] = true;
        }
    }
    for (bool present : seen)
        if (!present) {result.reason = "existing-entry-missing";return result;}
    if (result.newIndices.size() != static_cast<std::size_t>(expectedAmount)) {
        result.reason = "insertion-count-mismatch";
        return result;
    }
    result.valid = true;
    result.reason = "ready";
    return result;
}
inline void Mix32(std::uint64_t& h, std::uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) {
        h ^= static_cast<std::uint8_t>((value >> (i * 8)) & 0xffu);
        h *= 1099511628211ull;
    }
}
} // namespace CustomComponents::Diagnostics::PvpInsertionDelta
