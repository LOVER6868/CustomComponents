#pragma once
// Read-only investigation: observe GUID choice and actual entity outcomes.
// No selection cache mutations, card creation, or trigger dispatch.
#include <atomic>
#include <BNM/Class.hpp>
#include <BNM/Method.hpp>
#include <BNM/Field.hpp>
#include "log.h"
#include "pick3/PickedFromThreeSelection.hpp"
#include <mutex>
#include <vector>

namespace CustomComponents::Pick3::CreationTrace {
using Obj = BNM::IL2CPP::Il2CppObject;
inline int (*originalFindCard)(void*, Obj*, Obj*, const void*) = nullptr;
inline Obj* (*originalDrawCreate)(void*, Obj*, const void*) = nullptr;
inline void (*originalPlaceSource)(void*, Obj*, Obj*, const void*) = nullptr;
inline void (*originalTransformCard)(void*, Obj*, int, int, const void*) = nullptr;
inline BNM::Field<int> entityId;
inline void (*originalModifyCost)(void*, Obj*, const void*) = nullptr;
inline BNM::Field<int> costEffectTargetId;
inline BNM::Field<int> costEffectDelta;
static void ModifyCostHook(void* self, Obj* effect, const void* method) {
    const int target = effect && costEffectTargetId.IsValid() ? costEffectTargetId[effect].Get() : -1;
    const int delta = effect && costEffectDelta.IsValid() ? costEffectDelta[effect].Get() : 0;
    LOG_INFO("CustomComponents/Pick3/PICK3_COST_EFFECT: phase=before effect=%p target=%d delta=%d", effect, target, delta);
    originalModifyCost(self, effect, method);
    LOG_INFO("CustomComponents/Pick3/PICK3_COST_EFFECT: phase=after effect=%p target=%d delta=%d", effect, target, delta);
}

inline std::atomic<unsigned> events{0};
// Optional verified binding callback: invoked only after stock CreateCard returned.
inline void (*onVerifiedDatabaseDraw)(int sourceEffect, int createdId) = nullptr;
struct PendingDraw { Obj* effect; int sourceId; int guid; };
inline std::mutex pendingMutex;
inline std::vector<PendingDraw> pendingDraws;
inline constexpr std::size_t kMaxPending = 64;
inline bool ShouldLog() { return events.fetch_add(1, std::memory_order_relaxed) < 200; }
inline int Id(Obj* entity) { return entity && entityId.IsValid() ? entityId[entity].Get() : -1; }

static int FindCardHook(void* self, Obj* query, Obj* effect, const void* method) {
    // Only accept a selection identity observed by our GUID query while
    // this particular stock finder invocation is executing.
    const bool outer = Picked::finderObserving;
    const int previousSource = Picked::finderSelectionEffect;
    const int previousGuid = Picked::finderSelectionGuid;
    const unsigned previousMatches = Picked::finderSelectionMatches;
    Picked::finderObserving = true;
    Picked::finderSelectionEffect = -1;
    Picked::finderSelectionGuid = -1;
    Picked::finderSelectionMatches = 0;
    const int guid = originalFindCard(self, query, effect, method);
    const int sourceId = Picked::finderSelectionGuid == guid ? Picked::finderSelectionEffect : -1;
    Picked::finderObserving = outer;
    Picked::finderSelectionEffect = previousSource;
    Picked::finderSelectionGuid = previousGuid;
    Picked::finderSelectionMatches = previousMatches;
    if (effect) {
        std::lock_guard<std::mutex> guard(pendingMutex);
        // A reused effect pointer cannot inherit an earlier GUID decision.
        pendingDraws.erase(std::remove_if(pendingDraws.begin(), pendingDraws.end(),
            [effect](const PendingDraw& p) { return p.effect == effect; }), pendingDraws.end());
        if (sourceId > 0) {
            if (pendingDraws.size() == kMaxPending) pendingDraws.erase(pendingDraws.begin());
            pendingDraws.push_back({effect, sourceId, guid});
        }
    }
    if (ShouldLog()) LOG_INFO("CustomComponents/Pick3/CREATION_TRACE: boundary=FINDER effect=%p chosenGuid=%d sourceEffect=%d verifiedQuery=%d",
        effect, guid, sourceId, sourceId > 0 ? 1 : 0);
    return guid;
}
static Obj* DrawCreateHook(void* self, Obj* effect, const void* method) {
    Obj* created = originalDrawCreate(self, effect, method);
    PendingDraw matching{};
    {
        std::lock_guard<std::mutex> guard(pendingMutex);
        auto it = std::find_if(pendingDraws.begin(), pendingDraws.end(),
            [effect](const PendingDraw& p) { return p.effect == effect; });
        if (it != pendingDraws.end()) { matching = *it; pendingDraws.erase(it); }
    }
    const int entityId = Id(created);
    const bool bound = matching.sourceId > 0 && entityId > 0 &&
        Picked::BindCreated(matching.sourceId, matching.guid, entityId);
    if (bound && onVerifiedDatabaseDraw) onVerifiedDatabaseDraw(matching.sourceId, entityId);
    if (ShouldLog()) LOG_INFO("CustomComponents/Pick3/CREATION_TRACE: boundary=DRAW_CREATE effect=%p entityId=%d sourceEffect=%d chosenGuid=%d bound=%d",
        effect, entityId, matching.sourceId, matching.guid, bound ? 1 : 0);
    return created;
}
static void PlaceSourceHook(void* self, Obj* card, Obj* effect, const void* method) {
    // Called after the stock method so its normal work completes unmodified.
    originalPlaceSource(self, card, effect, method);
    if (ShouldLog()) LOG_INFO("CustomComponents/Pick3/CREATION_TRACE: boundary=PLACE_SOURCE effect=%p entity=%p entityId=%d note=placed-creation-source", effect, card, Id(card));
}
static void TransformCardHook(void* self, Obj* card, int guid, int effectEntityId, const void* method) {
    const int before = Id(card);
    originalTransformCard(self, card, guid, effectEntityId, method);
    if (ShouldLog()) LOG_INFO("CustomComponents/Pick3/CREATION_TRACE: boundary=TRANSFORM effectEntityId=%d card=%p entityBefore=%d entityAfter=%d targetGuid=%d note=existing-or-replacement-unverified", effectEntityId, card, before, Id(card), guid);
}

inline void Reset() {
    std::lock_guard<std::mutex> guard(pendingMutex);
    pendingDraws.clear();
}
inline bool Initialize() {
    auto finder = BNM::Class("PvZCards.Core.Systems.Utilities", "CardFromSubsetFinder");
    auto draw = BNM::Class("PvZCards.Core.Systems", "DrawCardFromSubsetSystem");
    auto placer = BNM::Class("PvZCards.Core.Systems.Utilities", "CreatedCardPlacer");
    auto transform = BNM::Class("PvZCards.Core.Systems", "TransformIntoCardFromSubsetSystem");
    auto entity = BNM::Class("PvZCards.Engine", "Entity");
    auto find = finder.GetMethod("FindCard", 2);
    auto create = draw.GetMethod("CreateCard", 1);
    auto source = placer.GetMethod("AddCreatedCardEffect", 2);
    auto transformCard = transform.GetMethod("TransformCard", 3);
    entityId = entity.GetField("<Id>k__BackingField").cast<int>();
    // Fail closed: all methods must resolve before installing any hook.
    if (!find.IsValid() || !create.IsValid() || !source.IsValid() ||
        !transformCard.IsValid() || !entityId.IsValid()) {
        LOG_ERR("CustomComponents/Pick3/CREATION_TRACE: preflight=failed no-hooks");
        return false;
    }
    BNM::BasicHook(find, FindCardHook, originalFindCard);
    BNM::BasicHook(create, DrawCreateHook, originalDrawCreate);
    BNM::BasicHook(source, PlaceSourceHook, originalPlaceSource);
    BNM::BasicHook(transformCard, TransformCardHook, originalTransformCard);
    auto modifyCostSystem = BNM::Class("PvZCards.Core.Systems", "ModifySunCostSystem");
    auto modifyCostEffect = BNM::Class("PvZCards.Engine.Effects", "ModifySunCostEffect");
    auto stockCostEffect = BNM::Class("PvZCards.Engine.Effects", "Effect");
    auto processCost = modifyCostSystem.GetMethod("ProcessEffect", 1);
    costEffectTargetId = stockCostEffect.GetField("TargetId").cast<int>();
    costEffectDelta = modifyCostEffect.GetField("SunCostDelta").cast<int>();
    if (processCost.IsValid() && costEffectTargetId.IsValid() && costEffectDelta.IsValid()) {
        BNM::BasicHook(processCost, ModifyCostHook, originalModifyCost);
    }
    LOG_INFO("CustomComponents/Pick3/PICK3_COST_TRACE: enabled=%d targetField=%d deltaField=%d", originalModifyCost ? 1 : 0, costEffectTargetId.IsValid() ? 1 : 0, costEffectDelta.IsValid() ? 1 : 0);
    const bool ready = originalFindCard && originalDrawCreate && originalPlaceSource && originalTransformCard;
    LOG_INFO("CustomComponents/Pick3/CREATION_TRACE: ready=%d finder=%d draw=%d placer=%d transform=%d drawBinding=1 otherAdaptersReadOnly=1 maxEvents=200",
        ready ? 1 : 0, originalFindCard ? 1 : 0, originalDrawCreate ? 1 : 0,
        originalPlaceSource ? 1 : 0, originalTransformCard ? 1 : 0);
    return ready;
}
} // namespace CustomComponents::Pick3::CreationTrace
