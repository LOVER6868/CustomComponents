#pragma once
// A single optional read-only observer of the CustomComponents-owned
// DrawCardSystem::ProcessEffect hook. Do not install another draw detour.
// All arguments are copied scalar IDs; no IL2CPP pointers leave this boundary.
#include <atomic>

namespace CustomComponents::Integration::ReplayDrawObserver {
using Callback = void(*)(int handId, int deckId, int entityId,
                         int cardGuid, int playerId, int handOwnerId);
inline std::atomic<Callback> current{nullptr};
inline bool Register(Callback f) {
    if (!f) return false;
    Callback expected = nullptr;
    return current.compare_exchange_strong(expected, f,
        std::memory_order_release, std::memory_order_relaxed) || expected == f;
}
inline Callback Listener() {
    return current.load(std::memory_order_acquire);
}
} // namespace CustomComponents::Integration::ReplayDrawObserver
