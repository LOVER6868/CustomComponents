#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string_view>

namespace CustomComponents::Variables::MultiplierPolicy {

// Authored VariableMultiplier.Owner values. These are NOT native IL2CPP fields.
enum class OwnerMode : std::uint8_t {
    Default,
    Self,
    Query,
    PrimaryTarget,
    Invalid
};

// Authored VariableMultiplier.Aggregation values.
enum class Aggregation : std::uint8_t {
    Single,
    Sum,
    Min,
    Max,
    Invalid
};

inline constexpr OwnerMode ParseOwner(std::string_view value) {
    if (value.empty()) return OwnerMode::Default;
    if (value == "Self") return OwnerMode::Self;
    if (value == "Query") return OwnerMode::Query;
    if (value == "PrimaryTarget") return OwnerMode::PrimaryTarget;
    return OwnerMode::Invalid;
}

inline constexpr Aggregation ParseAggregation(std::string_view value) {
    if (value == "Single") return Aggregation::Single;
    if (value == "Sum") return Aggregation::Sum;
    if (value == "Min") return Aggregation::Min;
    if (value == "Max") return Aggregation::Max;
    return Aggregation::Invalid;
}

// Validate combinations before resolving any managed game entities.
// The active effect creates one multiplier evaluation per primary recipient.
inline constexpr bool ValidSelection(OwnerMode owner, Aggregation aggregation,
                                     bool hasQuery, bool hasPrimaryTarget) {
    if (owner == OwnerMode::Invalid || aggregation == Aggregation::Invalid) return false;
    switch (owner) {
        case OwnerMode::PrimaryTarget:
            return !hasQuery && hasPrimaryTarget && aggregation == Aggregation::Single;
        case OwnerMode::Self:
            return !hasQuery;
        case OwnerMode::Query:
            return hasQuery;
        case OwnerMode::Default:
            return true;
        case OwnerMode::Invalid:
            return false;
    }
    return false;
}

inline constexpr bool ValidOwnerCount(Aggregation aggregation, std::size_t count) {
    return aggregation != Aggregation::Invalid && count > 0 &&
           (aggregation != Aggregation::Single || count == 1);
}

struct Accumulator {
    Aggregation mode;
    std::int64_t value = 0;
    bool first = true;

    explicit Accumulator(Aggregation aggregation) : mode(aggregation) {}

    void Add(int next) {
        if (mode == Aggregation::Min) {
            value = first ? next : (value < next ? value : next);
        } else if (mode == Aggregation::Max) {
            value = first ? next : (value > next ? value : next);
        } else {
            // Single and Sum have identical arithmetic; count is checked first.
            value += next;
        }
        first = false;
    }

    std::optional<int> Result() const {
        if (first || mode == Aggregation::Invalid ||
            value < std::numeric_limits<int>::min() ||
            value > std::numeric_limits<int>::max()) return {};
        return static_cast<int>(value);
    }
};

}  // namespace CustomComponents::Variables::MultiplierPolicy
