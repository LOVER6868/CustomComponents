#pragma once
// Phase 5 diagnostics ONLY: no mutations, no animation creation, no view pooling.
#include <atomic>
#include <cstdint>
#include <cstring>
#include <array>
#include <BNM/Class.hpp>
#include <BNM/Method.hpp>
#include "log.h"

namespace CustomComponents::Systems::HandRemovalTraceSystem {
using Obj = BNM::IL2CPP::Il2CppObject;
inline std::atomic<unsigned> removalCount{0}, tweenStartCount{0}, tweenDoneCount{0}, playCount{0}, laneStartCount{0}, laneDoneCount{0};
inline constexpr unsigned kMaxPerEvent = 12; // Only early tween baseline samples; correlated events remain visible.
inline constexpr unsigned kMaxRemovals = 128;
// IL2CPP instance methods receive (this, managed arguments..., MethodInfo*).
using RemoveFn = void (*)(Obj*, Obj*, const void*);
using TweenFn = void (*)(Obj*, const void*);
using PlayFn = void (*)(Obj*, Obj*, const void*);
inline RemoveFn originalRemove = nullptr;
inline TweenFn originalTweenStart = nullptr, originalTweenDone = nullptr;
inline PlayFn originalPlay = nullptr;
inline TweenFn originalLaneStart = nullptr, originalLaneDone = nullptr;
using RemoveMappedFn = void (*)(Obj*, int, const void*);
inline RemoveMappedFn originalMappedRemove = nullptr;
inline TweenFn originalCardDestroy = nullptr;
inline std::atomic<unsigned> mappedRemoveCount{0}, cardDestroyCount{0};
// Store numeric identities only: never keep live managed references in these slots.
struct TrackedRemoval {
    std::atomic<uintptr_t> view{0};
    std::atomic<uintptr_t> hand{0};
    std::atomic<int> cardId{-1};
    std::atomic<int> guid{-1};
};
inline std::array<TrackedRemoval, 64> tracked{};
inline std::atomic<unsigned> trackingIndex{0};
static int FindTrackedView(uintptr_t view) {
    if (!view) return -1;
    for (unsigned i=0; i<tracked.size(); ++i)
        if (tracked[i].view.load(std::memory_order_relaxed) == view) return static_cast<int>(i);
    return -1;
}
static int FindTrackedId(int id) {
    if (id < 0) return -1;
    for (unsigned i=0; i<tracked.size(); ++i)
        if (tracked[i].cardId.load(std::memory_order_relaxed) == id &&
            tracked[i].view.load(std::memory_order_relaxed) != 0) return static_cast<int>(i);
    return -1;
}
// Observational addresses only. Do not retain managed references or dereference them later.
inline std::atomic<uintptr_t> lastRemovedView{0};
inline std::atomic<uintptr_t> lastRemovedHand{0};
struct Vec3 { float x, y, z; };
template<class T> static T ReadAt(const Obj* obj, size_t offset) {
    T result{};
    if (obj) std::memcpy(&result, reinterpret_cast<const uint8_t*>(obj) + offset, sizeof(T));
    return result;
}
static void DescribeCard(const char* label, Obj* card) {
    // CardViewComponent dump.cs TypeDefIndex 5227; these offsets assume this concrete type.
    LOG_INFO("CustomComponents/HandTrace: %s cardView=%p CardViewComponent.CardId=%d Guid=%d (raw fields 0x108/0x10C; verify concrete type)",
        label, card, card ? ReadAt<int>(card, 0x108) : -1, card ? ReadAt<int>(card, 0x10C) : -1);
}
static void DescribeTween(const char* label, unsigned seq, Obj* animation) {
    // TweenCardEffectAnimation dump.cs TypeDefIndex 6135.
    Obj* moving = ReadAt<Obj*>(animation, 0x30);
    const Vec3 scale = ReadAt<Vec3>(animation, 0x44);
    const Vec3 target = ReadAt<Vec3>(animation, 0x38);
    const float duration = ReadAt<float>(animation, 0x50);
    const bool same = moving && reinterpret_cast<uintptr_t>(moving) == lastRemovedView.load(std::memory_order_relaxed);
    LOG_INFO("CustomComponents/HandTrace: %s seq=%u animation=%p moving=%p LAST_REMOVED_MATCH=%d lastHand=%p scale=(%.3f,%.3f,%.3f) position=(%.3f,%.3f,%.3f) duration=%.3f",
        label, seq, animation, moving, (int)same,
        reinterpret_cast<void*>(lastRemovedHand.load(std::memory_order_relaxed)),
        scale.x,scale.y,scale.z,target.x,target.y,target.z,duration);
    const int trackedSlot = FindTrackedView(reinterpret_cast<uintptr_t>(moving));
    if (trackedSlot >= 0) {
        LOG_INFO("CustomComponents/HandTrace/FOCUS: TWEEN %s animation=%p removedView=%p entityId=%d guid=%d scale=(%.3f,%.3f,%.3f) pos=(%.3f,%.3f,%.3f) time=%.3f",
            label, animation, moving,
            tracked[trackedSlot].cardId.load(std::memory_order_relaxed),
            tracked[trackedSlot].guid.load(std::memory_order_relaxed),
            scale.x, scale.y, scale.z, target.x, target.y, target.z, duration);
    }
}
static void LaneStartHook(Obj* animation, const void* info) {
    const unsigned seq = ++laneStartCount;
    if (seq <= kMaxPerEvent) {
        LOG_INFO("CustomComponents/HandTrace: PlayCardToLane.OnStarted ENTER seq=%u animation=%p handId=%d cardId=%d laneId=%d cardView=%p",
            seq, animation, ReadAt<int>(animation,0x50),ReadAt<int>(animation,0x54),ReadAt<int>(animation,0x58),ReadAt<Obj*>(animation,0x48));
    }
    originalLaneStart(animation,info);
    if (seq <= kMaxPerEvent) LOG_INFO("CustomComponents/HandTrace: PlayCardToLane.OnStarted EXIT seq=%u animation=%p cardView=%p",
        seq, animation,ReadAt<Obj*>(animation,0x48));
}
static void LaneDoneHook(Obj* animation, const void* info) {
    const unsigned seq = ++laneDoneCount;
    if (seq <= kMaxPerEvent) LOG_INFO("CustomComponents/HandTrace: PlayCardToLane.HandleCompleted ENTER seq=%u animation=%p cardId=%d cardView=%p",
        seq,animation,ReadAt<int>(animation,0x54),ReadAt<Obj*>(animation,0x48));
    originalLaneDone(animation,info);
    if (seq <= kMaxPerEvent) LOG_INFO("CustomComponents/HandTrace: PlayCardToLane.HandleCompleted EXIT seq=%u",seq);
}


static void MappedRemoveHook(Obj* mapper, int entityId, const void* info) {
    const unsigned seq = ++mappedRemoveCount;
    const int slot = FindTrackedId(entityId);
    if (slot >= 0 || seq <= 8)
        LOG_INFO("CustomComponents/HandTrace/LIFECYCLE: EntityViewMapper.RemoveViewObject ENTER id=%d mapper=%p tracked=%d view=%p",
            entityId, mapper, static_cast<int>(slot >= 0),
            slot >= 0 ? reinterpret_cast<void*>(tracked[slot].view.load(std::memory_order_relaxed)) : nullptr);
    originalMappedRemove(mapper, entityId, info);
    if (slot >= 0 || seq <= 8)
        LOG_INFO("CustomComponents/HandTrace/LIFECYCLE: EntityViewMapper.RemoveViewObject EXIT id=%d", entityId);
}
static void CardDestroyHook(Obj* cardView, const void* info) {
    const int slot = FindTrackedView(reinterpret_cast<uintptr_t>(cardView));
    const unsigned seq = ++cardDestroyCount;
    if (slot >= 0 || seq <= 8)
        LOG_INFO("CustomComponents/HandTrace/LIFECYCLE: CardViewComponent.OnDestroy ENTER view=%p tracked=%d cardId=%d guid=%d",
            cardView, static_cast<int>(slot >= 0),
            slot >= 0 ? tracked[slot].cardId.load(std::memory_order_relaxed) : -1,
            slot >= 0 ? tracked[slot].guid.load(std::memory_order_relaxed) : -1);
    originalCardDestroy(cardView, info);
    if (slot >= 0 || seq <= 8)
        LOG_INFO("CustomComponents/HandTrace/LIFECYCLE: CardViewComponent.OnDestroy EXIT view=%p", cardView);
    if (slot >= 0) tracked[slot].view.store(0, std::memory_order_relaxed);
}
static void RemoveHook(Obj* hand, Obj* cardView, const void* info) {
    const unsigned seq = removalCount.fetch_add(1, std::memory_order_relaxed) + 1;
    if (seq <= kMaxRemovals)
        LOG_INFO("CustomComponents/HandTrace: RemoveFromHand ENTER seq=%u hand=%p cardView=%p", seq, hand, cardView);
    if (seq <= kMaxRemovals) DescribeCard("REMOVE-BEFORE", cardView);
    const unsigned slot = trackingIndex.fetch_add(1, std::memory_order_relaxed) % tracked.size();
    // Publish view last, so readers never observe the new view with a stale ID.
    tracked[slot].view.store(0, std::memory_order_relaxed);
    const int id = cardView ? ReadAt<int>(cardView, 0x108) : -1;
    const int guid = cardView ? ReadAt<int>(cardView, 0x10C) : -1;
    tracked[slot].hand.store(reinterpret_cast<uintptr_t>(hand), std::memory_order_relaxed);
    tracked[slot].cardId.store(id, std::memory_order_relaxed);
    tracked[slot].guid.store(guid, std::memory_order_relaxed);
    tracked[slot].view.store(reinterpret_cast<uintptr_t>(cardView), std::memory_order_relaxed);
    if (seq <= kMaxRemovals)
        LOG_INFO("CustomComponents/HandTrace/FOCUS: REMOVED entityId=%d guid=%d view=%p hand=%p slot=%u", id, guid, cardView, hand, slot);
    lastRemovedView.store(reinterpret_cast<uintptr_t>(cardView), std::memory_order_relaxed);
    lastRemovedHand.store(reinterpret_cast<uintptr_t>(hand), std::memory_order_relaxed);
    originalRemove(hand, cardView, info);
    if (seq <= kMaxRemovals)
        LOG_INFO("CustomComponents/HandTrace: RemoveFromHand EXIT seq=%u hand=%p cardView=%p", seq, hand, cardView);
}
static void TweenStartHook(Obj* animation, const void* info) {
    const unsigned seq = tweenStartCount.fetch_add(1, std::memory_order_relaxed) + 1;
    if (seq <= kMaxPerEvent || FindTrackedView(reinterpret_cast<uintptr_t>(ReadAt<Obj*>(animation, 0x30))) >= 0)
        DescribeTween("TweenCard OnStarted ENTER", seq, animation);
    originalTweenStart(animation, info);
    if (seq <= kMaxPerEvent)
        LOG_INFO("CustomComponents/HandTrace: TweenCard OnStarted EXIT seq=%u animation=%p", seq, animation);
}
static void TweenDoneHook(Obj* animation, const void* info) {
    const unsigned seq = tweenDoneCount.fetch_add(1, std::memory_order_relaxed) + 1;
    if (seq <= kMaxPerEvent || FindTrackedView(reinterpret_cast<uintptr_t>(ReadAt<Obj*>(animation, 0x30))) >= 0)
        DescribeTween("TweenCard HandleTweenComplete", seq, animation);
    originalTweenDone(animation, info);
}
static void PlayHook(Obj* processor, Obj* record, const void* info) {
    const unsigned seq = playCount.fetch_add(1, std::memory_order_relaxed) + 1;
    if (seq <= kMaxPerEvent)
        LOG_INFO("CustomComponents/HandTrace: PlayCardRecordProcessor.ProcessRecord ENTER seq=%u processor=%p record=%p", seq, processor, record);
    originalPlay(processor, record, info);
    if (seq <= kMaxPerEvent)
        LOG_INFO("CustomComponents/HandTrace: PlayCardRecordProcessor.ProcessRecord EXIT seq=%u", seq);
}
inline bool Initialize() {
    // All hooks best effort; do not block the rest of CustomComponents.
    const auto hand = BNM::Class("PvZCards.Game", "HandViewComponent");
    const auto tween = BNM::Class("PvZCards.Game", "TweenCardEffectAnimation");
    const auto play = BNM::Class("PvZCards.Game", "PlayCardRecordProcessor");
    const auto lane = BNM::Class("PvZCards.Game", "PlayCardToLaneAnimation");
    const auto mapper = BNM::Class("PvZCards.Game", "EntityViewMapper");
    const auto card = BNM::Class("PvZCards.Game", "CardViewComponent");
    auto install = [](auto method, auto hook, auto& original, const char* label) {
        if (!method.IsValid()) {
            LOG_ERR("CustomComponents/HandTrace: missing %s", label);
            return false;
        }
        BNM::BasicHook(method, hook, original);
        const bool ok = original != nullptr;
        LOG_INFO("CustomComponents/HandTrace: %s hooked=%d", label, static_cast<int>(ok));
        return ok;
    };
    bool ok = true;
    ok = install(hand.GetMethod("RemoveFromHand", 1), RemoveHook, originalRemove, "HandViewComponent.RemoveFromHand") && ok;
    ok = install(tween.GetMethod("OnStarted", 0), TweenStartHook, originalTweenStart, "TweenCardEffectAnimation.OnStarted") && ok;
    ok = install(tween.GetMethod("HandleTweenComplete", 0), TweenDoneHook, originalTweenDone, "TweenCardEffectAnimation.HandleTweenComplete") && ok;
    ok = install(play.GetMethod("ProcessRecord", 1), PlayHook, originalPlay, "PlayCardRecordProcessor.ProcessRecord") && ok;
    ok = install(lane.GetMethod("OnStarted", 0), LaneStartHook, originalLaneStart, "PlayCardToLaneAnimation.OnStarted [RVA 0x1FA1D98]") && ok;
    ok = install(lane.GetMethod("HandleCompleted", 0), LaneDoneHook, originalLaneDone, "PlayCardToLaneAnimation.HandleCompleted [RVA 0x1FA2330]") && ok;
    // These are optional observational lifecycle hooks. Failure never stops gameplay.
    const bool mapOk = install(mapper.GetMethod("RemoveViewObject", 1), MappedRemoveHook, originalMappedRemove,
        "EntityViewMapper.RemoveViewObject [RVA 0x1F1A3A8]");
    const bool destroyOk = install(card.GetMethod("OnDestroy", 0), CardDestroyHook, originalCardDestroy,
        "CardViewComponent.OnDestroy [RVA 0x1EF71A8]");
    LOG_INFO("CustomComponents/HandTrace: ready=%d lifecycleMapper=%d lifecycleDestroy=%d (read-only, focused tracking; verified dump.cs offsets)",
        static_cast<int>(ok), static_cast<int>(mapOk), static_cast<int>(destroyOk));
    return ok;
}
} // namespace CustomComponents::Systems::HandRemovalTraceSystem
