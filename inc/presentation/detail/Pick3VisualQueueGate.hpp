#pragma once
// Patch 1.3: passive stock animation observations. NEVER invoke CanYield from
// a diagnostic/picker callback. Do not gate game/UI progression on a stale poll.
#include <atomic>
#include <cstdint>
#include "presentation/detail/Pick3VisualBarrierTracker.hpp"

namespace CustomComponents::Presentation::Pick3VisualQueueGate {
    inline std::atomic<std::uintptr_t> lastQueue{0};
    inline std::atomic<int> lastStockYield{-1};
    inline std::atomic<std::uint64_t> stockYieldCalls{0};
    inline std::atomic<std::uint64_t> stockUpdates{0};

    inline void ObserveStockYield(void* queue, bool yielded) {
        lastQueue.store(reinterpret_cast<std::uintptr_t>(queue), std::memory_order_relaxed);
        lastStockYield.store(yielded ? 1 : 0, std::memory_order_relaxed);
        stockYieldCalls.fetch_add(1, std::memory_order_relaxed);
    }
    inline void ObserveStockUpdate() {
        stockUpdates.fetch_add(1, std::memory_order_relaxed);
    }
    inline std::atomic<std::uintptr_t> lastAnimationQueue{0};
    inline std::atomic<std::uintptr_t> lastAnimation{0};
    inline std::atomic<std::uint64_t> enqueueEvents{0};
    inline std::atomic<std::uint64_t> startEvents{0};
    inline std::atomic<std::uint64_t> retireTransitions{0};
    inline std::atomic<int> normalCount{-1};
    inline std::atomic<int> frontCount{-1};
    // Diagnostics only: this is not evidence of ownership of the picker queue.
    inline void ObserveEnqueue(void* queue, void* animation, bool front) {
        lastAnimationQueue.store(reinterpret_cast<std::uintptr_t>(queue), std::memory_order_relaxed);
        lastAnimation.store(reinterpret_cast<std::uintptr_t>(animation), std::memory_order_relaxed);
        enqueueEvents.fetch_add(1, std::memory_order_relaxed);
        (void)front;
    }
    inline void ObserveStart(void* queue, void* animation) {
        lastAnimationQueue.store(reinterpret_cast<std::uintptr_t>(queue), std::memory_order_relaxed);
        lastAnimation.store(reinterpret_cast<std::uintptr_t>(animation), std::memory_order_relaxed);
        startEvents.fetch_add(1, std::memory_order_relaxed);
    }
    // Read-only phase: a stock CanYield return is not sufficient evidence that
    // this PickFromThree transaction's preceding visual work has completed.
    inline bool IsPresentationBusy() { return false; }
}
