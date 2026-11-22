#pragma once
#include <cstdint>
#include <string_view>

// Comparison names are authored strings. Parse once at the boundary and use
// a closed, strongly-typed set inside the engine/queries/variables modules.
namespace CustomComponents::Core::Comparisons {
enum class Operator : std::int32_t {
    Invalid = -1, Equal = 0, NotEqual = 1, Greater = 2,
    GreaterOrEqual = 3, Less = 4, LessOrEqual = 5,
    Contains = 6, NotContains = 7
};
inline constexpr Operator Parse(std::string_view text) {
    if (text == "Equal") return Operator::Equal;
    if (text == "NotEqual") return Operator::NotEqual;
    if (text == "Greater") return Operator::Greater;
    if (text == "GreaterOrEqual") return Operator::GreaterOrEqual;
    if (text == "Less") return Operator::Less;
    if (text == "LessOrEqual") return Operator::LessOrEqual;
    if (text == "Contains") return Operator::Contains;
    if (text == "NotContains") return Operator::NotContains;
    return Operator::Invalid;
}
inline constexpr bool Test(int left, int right, Operator operation) {
    switch (operation) {
    case Operator::Equal: return left == right;
    case Operator::NotEqual: return left != right;
    case Operator::Greater: return left > right;
    case Operator::GreaterOrEqual: return left >= right;
    case Operator::Less: return left < right;
    case Operator::LessOrEqual: return left <= right;
    default: return false;
    }
}
inline constexpr bool Test(int left, int right, std::string_view text) {
    return Test(left, right, Parse(text));
}
} // namespace CustomComponents::Core::Comparisons
