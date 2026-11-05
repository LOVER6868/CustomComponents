#pragma once
// Value-only classification and exact-pair contract for PF3 1.7.23.
// Neither helper edits managed lists, game state, nor command data.
#include <cstddef>
#include <cstdint>

namespace CustomComponents::Pick3::RevivePairContract1723 {
enum class PrimaryOrigin : std::uint8_t {
    Invalid, AlreadyChosen, VerifiedCast, Other
};
inline PrimaryOrigin Classify(int original, int chosen, int verifiedCast) {
    if (original < 0 || chosen < 0) return PrimaryOrigin::Invalid;
    if (original == chosen) return PrimaryOrigin::AlreadyChosen;
    if (verifiedCast >= 0 && original == verifiedCast) return PrimaryOrigin::VerifiedCast;
    return PrimaryOrigin::Other;
}
inline const char* Name(PrimaryOrigin origin) {
    switch (origin) {
        case PrimaryOrigin::AlreadyChosen: return "ALREADY_CHOSEN";
        case PrimaryOrigin::VerifiedCast: return "VERIFIED_CAST";
        case PrimaryOrigin::Other: return "OTHER";
        default: return "INVALID";
    }
}
// The provided target set MUST come from the current exact effect/model scope.
// The two callbacks MUST independently prove live physical placement and the
// effect's exact primary-dependent secondary filter, respectively.
// No callback is run until candidate membership and ID checks pass.
template<class Physical, class Filter>
inline bool VerifyExact(int sourceCard, int selectedPrimary, int selectedLane,
                        const int* livePrimaries, std::size_t primaryCount,
                        Physical&& physical, Filter&& filter) {
    if (sourceCard < 0 || selectedPrimary < 0 || selectedLane < 0 ||
        selectedPrimary == sourceCard || selectedLane == sourceCard ||
        selectedPrimary == selectedLane || !livePrimaries ||
        primaryCount == 0 || primaryCount > 256) return false;
    bool found = false;
    for (std::size_t i = 0; i < primaryCount; ++i) {
        if (livePrimaries[i] == selectedPrimary) { found = true; break; }
    }
    return found && physical(selectedPrimary, selectedLane) &&
           filter(selectedPrimary, selectedLane);
}
} // namespace CustomComponents::Pick3::RevivePairContract1723
