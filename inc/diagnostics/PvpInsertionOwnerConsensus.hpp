#pragma once
// A PvP deck-pool entity need not have an Owner component. Obtain the owner
// from physical cards already present in that pool, never from pool ID or
// the potentially different acting player's effect target.
#include <vector>

namespace CustomComponents::Diagnostics::PvpInsertionOwnerConsensus {
struct PhysicalCopy {
    int poolGuid = -1;
    int poolEntityId = -1;
    int cardGuid = -1;
    int cardEntityId = -1;
    int ownerId = -1;
    bool hasCard = false;
    bool hasOwner = false;
};
struct Result {
    bool valid = false;
    int ownerId = -1;
    int checked = 0;
    const char* reason = "no-physical-owner-evidence";
};
inline Result Resolve(const std::vector<PhysicalCopy>& copies) {
    Result result{};
    for (const auto& copy : copies) {
        if (copy.poolEntityId < 0 || copy.cardEntityId != copy.poolEntityId ||
            !copy.hasCard || !copy.hasOwner || copy.poolGuid <= 0 ||
            copy.cardGuid != copy.poolGuid || copy.ownerId < 0) {
            result.reason = "invalid-existing-card-owner";
            return result;
        }
        if (result.checked && result.ownerId != copy.ownerId) {
            result.reason = "conflicting-existing-card-owners";
            return result;
        }
        result.ownerId = copy.ownerId;
        ++result.checked;
    }
    if (!result.checked) return result;
    result.valid = true;
    result.reason = "existing-cards-agree";
    return result;
}
} // namespace CustomComponents::Diagnostics::PvpInsertionOwnerConsensus
