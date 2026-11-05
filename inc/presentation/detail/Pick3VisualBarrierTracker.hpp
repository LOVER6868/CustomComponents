#pragma once
// Patch 1.5: passive, diagnostic-only per-queue animation identity tracking.
// A disappearance from queue lists means LEFT_LIST, not confirmed completed.
// Never use this information to block gameplay/UI or invoke stock methods.
#include <algorithm>
#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace CustomComponents::Presentation::Pick3VisualBarrierTracker {
struct AnimationToken { std::uintptr_t pointer = 0; std::uint64_t generation = 0; };
struct QueueRecord {
    std::uint64_t nextGeneration = 0;
    std::uint64_t updates = 0;
    bool hasSnapshot = false;
    std::vector<AnimationToken> inLists;
    std::vector<AnimationToken> pendingAdds;
    std::vector<AnimationToken> started;
    std::uint64_t leftList = 0;
};
struct OfferRecord {
    int effect = -1, group = -1;
    std::uintptr_t candidateQueue = 0;
    bool ownershipVerified = false;
    std::vector<AnimationToken> dependencies;
    bool reportedClear = false;
    bool reportedPending = false;
    int lastRemaining = -1;
};
inline std::mutex mutex;
inline std::unordered_map<std::uintptr_t, QueueRecord> queues;
inline std::unordered_map<std::uint64_t, OfferRecord> offers;
inline std::uintptr_t lastObservedQueue = 0;
inline std::uint64_t serial = 0;
inline std::uint64_t MakeKey(int effect, int group) {
    return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(effect)) << 32) |
           static_cast<std::uint32_t>(group);
}
inline void Enqueue(void* owner, void* animation) {
    if (!owner || !animation) return;
    std::lock_guard<std::mutex> lock(mutex);
    const auto id = reinterpret_cast<std::uintptr_t>(owner);
    auto& q = queues[id];
    const auto ptr = reinterpret_cast<std::uintptr_t>(animation);
    q.pendingAdds.push_back({ptr, ++q.nextGeneration});
    lastObservedQueue = id;
}
inline void Start(void* owner, void* animation) {
    if (!owner || !animation) return;
    std::lock_guard<std::mutex> lock(mutex);
    auto& q = queues[reinterpret_cast<std::uintptr_t>(owner)];
    const auto ptr = reinterpret_cast<std::uintptr_t>(animation);
    const auto it = std::find_if(q.inLists.begin(), q.inLists.end(),
        [ptr](const AnimationToken& x) { return x.pointer == ptr; });
    if (it != q.inLists.end()) q.started.push_back(*it);
}
// Called from the stock Update observer after the original Update returns.
// Inputs are identities read directly from existing List<T> fields, not native calls.
inline void ObserveList(void* owner, const std::vector<std::uintptr_t>& observed) {
    if (!owner) return;
    std::lock_guard<std::mutex> lock(mutex);
    const auto id = reinterpret_cast<std::uintptr_t>(owner);
    auto& q = queues[id];
    std::vector<AnimationToken> next;
    next.reserve(observed.size());
    for (auto ptr : observed) {
        if (!ptr) continue;
        const auto current = std::find_if(q.inLists.begin(), q.inLists.end(),
            [ptr](const AnimationToken& x) { return x.pointer == ptr; });
        if (current != q.inLists.end()) { next.push_back(*current); continue; }
        const auto added = std::find_if(q.pendingAdds.begin(), q.pendingAdds.end(),
            [ptr](const AnimationToken& x) { return x.pointer == ptr; });
        if (added != q.pendingAdds.end()) {
            next.push_back(*added);
            q.pendingAdds.erase(added);
        } else next.push_back({ptr, ++q.nextGeneration});
    }
    for (const auto& old : q.inLists) {
        if (std::find_if(next.begin(), next.end(), [&](const AnimationToken& x) {
            return x.pointer == old.pointer && x.generation == old.generation;
        }) == next.end()) ++q.leftList;
    }
    q.inLists = std::move(next);
    q.hasSnapshot = true;
    ++q.updates;
    lastObservedQueue = id;
    // Keep diagnostics bounded across long sessions. This is not a provenance ledger.
    if (queues.size() > 32) {
        for (auto it = queues.begin(); it != queues.end();) {
            if (it->first != id && it->second.inLists.empty() && it->second.pendingAdds.empty())
                it = queues.erase(it);
            else ++it;
            if (queues.size() <= 24) break;
        }
    }
}
struct CaptureSummary {
    std::uintptr_t queue = 0;
    int dependencies = 0;
    int queuesSeen = 0;
    bool ownershipVerified = false;
};
inline CaptureSummary CaptureOffer(int effect, int group) {
    std::lock_guard<std::mutex> lock(mutex);
    OfferRecord offer;
    offer.effect = effect;
    offer.group = group;
    offer.candidateQueue = lastObservedQueue;
    // An observed queue is NOT verified to be the picker gameplay queue.
    offer.ownershipVerified = false;
    auto it = queues.find(offer.candidateQueue);
    if (it != queues.end() && it->second.hasSnapshot) offer.dependencies = it->second.inLists;
    auto summary = CaptureSummary{offer.candidateQueue,
        static_cast<int>(offer.dependencies.size()), static_cast<int>(queues.size()), false};
    offers[MakeKey(effect, group)] = std::move(offer);
    if (offers.size() > 64) {
        // Evict the oldest observation rather than discarding the new offer.
        offers.erase(offers.begin());
    }
    return summary;
}
struct PollSummary { bool exists=false; bool known=false; bool clear=false; int remaining=0; std::uintptr_t queue=0; };
struct BarrierTransition { int effect=0, group=0, remaining=0; bool ownershipVerified=false; };
inline std::vector<BarrierTransition> CollectTransitions(void* owner) {
    std::vector<BarrierTransition> changes;
    std::lock_guard<std::mutex> lock(mutex);
    const auto queue = reinterpret_cast<std::uintptr_t>(owner);
    auto q = queues.find(queue);
    if (q == queues.end()) return changes;
    for (auto& entry : offers) {
        auto& offer = entry.second;
        if (offer.candidateQueue != queue) continue;
        int remaining = 0;
        for (const auto& token : offer.dependencies)
            if (std::find_if(q->second.inLists.begin(), q->second.inLists.end(),
                [&](const AnimationToken& active) {return token.pointer == active.pointer && token.generation == active.generation;})
                != q->second.inLists.end()) ++remaining;
        if (remaining != offer.lastRemaining) {
            changes.push_back({offer.effect, offer.group, remaining, offer.ownershipVerified});
            offer.lastRemaining = remaining;
        }
    }
    return changes;
}

inline PollSummary PollOffer(int effect, int group) {
    std::lock_guard<std::mutex> lock(mutex);
    auto it = offers.find(MakeKey(effect, group));
    if (it == offers.end()) return {};
    auto& offer = it->second;
    PollSummary result;
    result.exists = true;
    result.queue = offer.candidateQueue;
    auto q = queues.find(offer.candidateQueue);
    if (q == queues.end()) return result;
    // Cannot assert clearance or visual completion without verified queue ownership.
    result.known = offer.ownershipVerified;
    for (const auto& token : offer.dependencies)
        if (std::find_if(q->second.inLists.begin(), q->second.inLists.end(),
             [&](const AnimationToken& active) {return token.pointer == active.pointer && token.generation == active.generation;})
            != q->second.inLists.end()) ++result.remaining;
    result.clear = result.known && result.remaining == 0;
    return result;
}
}
