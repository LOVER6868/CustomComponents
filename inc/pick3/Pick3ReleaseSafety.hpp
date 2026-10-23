#pragma once

// Deliberately dependency-free release safety policy.
// Keep unverified network transport and the crash-associated CardView factory
// behind independent, explicit experimental permissions. Stale config.ini
// values alone cannot re-enable either feature.
namespace CustomComponents::Pick3::ReleaseSafety {
inline constexpr bool AllowExperimentalPvp(bool requested, bool experimentalOptIn) {
    return requested && experimentalOptIn;
}
inline constexpr bool AllowNativeCardViews(int stage, bool experimentalOptIn,
                                           bool isolationDisabled, bool requested) {
    return stage >= 2 && experimentalOptIn && isolationDisabled && requested;
}
inline constexpr bool CanUseDatabaseSelection(bool remote, int requiredTargets) {
    // The stock Play.Targets serialization is entity-ID based, not GUID-based.
    return !remote && requiredTargets == 1;
}
}
