#pragma once

#include <algorithm>
#include <cstddef>
#include <vector>
#include <utility>

namespace CustomComponents::Queries::PrimaryTargetsSelectionPolicy {

// Never identify selected copies by Card.Guid: Entity.Id is the primary
// selection identity.  Compare complete donor lists, not just whether an
// individual candidate appears, so an ambiguous donor cannot leak targets.
inline constexpr std::size_t kMaxTargets = 128;

struct Selection {
    std::vector<int> ids;
    bool present = false;
    bool ambiguous = false;

    bool Add(const int* values, std::size_t count) {
        if (count == 0 || count > kMaxTargets || !values) return false;
        std::vector<int> incoming(values, values + count);
        if (std::any_of(incoming.begin(), incoming.end(), [](int id) { return id <= 0; }))
            return false;
        std::sort(incoming.begin(), incoming.end());
        incoming.erase(std::unique(incoming.begin(), incoming.end()), incoming.end());
        if (!present) {
            ids = std::move(incoming);
            present = true;
        } else if (ids != incoming) {
            ambiguous = true;
        }
        return true;
    }

    bool Matches(int id) const {
        return id > 0 && present && !ambiguous &&
               std::binary_search(ids.begin(), ids.end(), id);
    }
};

}  // namespace CustomComponents::Queries::PrimaryTargetsSelectionPolicy
