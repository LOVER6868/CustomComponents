#pragma once
// Stock creation adapter: observe the chosen GUID, verify the returned card,
// and complete its already accepted model transaction. Never create/replay cards.
#include <atomic>
#include <BNM/Class.hpp>
#include <BNM/Method.hpp>
#include <BNM/Field.hpp>
#include "log.h"
#include "pick3/PickedFromThreeSelection.hpp"
#include "pick3/SelectionTransactionRuntime.hpp"
#include "pick3/Patch2ExactRouting.hpp"
#include <mutex>
#include <vector>

namespace CustomComponents::Pick3::CreationTrace {
using Obj = BNM::IL2CPP::Il2CppObject;
inline int (*originalFindCard)(void*, Obj*, Obj*, const void*) = nullptr;
inline Obj* (*originalDrawCreate)(void*, Obj*, const void*) = nullptr;
inline void (*originalPlaceSource)(void*, Obj*, Obj*, const void*) = nullptr;
inline void (*originalTransformCard)(void*, Obj*, int, int, const void*) = nullptr;
inline BNM::Field<int> entityId;
inline BNM::Field<Obj*> drawEntityModel;
inline BNM::Method<Obj*> createdGetComponent;
inline BNM::Field<int> createdCardGuid;
inline BNM::Class createdCardClass;
inline void (*originalModifyCost)(void*, Obj*, const void*) = nullptr;
inline BNM::Field<int> costEffectTargetId;
inline BNM::Field<int> costEffectDelta;
inline BNM::Field<int> costEffectEntityId;
inline BNM::Field<Obj*> costSystemEntities;
inline void (*originalRecordCost)(void*, Obj*, int, const void*) = nullptr;

inline Transaction::Ref ActiveCostTransaction(Obj* self, Obj* effect) {
    namespace TX = CustomComponents::Pick3::Transaction;
    namespace RT = CustomComponents::Pick3::TransactionRuntime;
    const auto value = TX::active.value;
    if (!TX::Current(value) || !self || !effect || !costEffectEntityId.IsValid() ||
        !costSystemEntities.IsValid()) return {};
    Obj* model = costSystemEntities[ self ].Get();
    const int effectId = costEffectEntityId[effect].Get();
    if (!model || effectId <= 0 ||
        (effectId != TX::active.effect && effectId != TX::active.contextEffect) ||
        value->model != RT::ModelOf(model)) return {};
    return value;
}

static void ModifyCostHook(void* self, Obj* effect, const void* method) {
    namespace TX = CustomComponents::Pick3::Transaction;
    namespace RT = CustomComponents::Pick3::TransactionRuntime;
    int target = effect && costEffectTargetId.IsValid() ? costEffectTargetId[effect].Get() : -1;
    const int delta = effect && costEffectDelta.IsValid() ? costEffectDelta[effect].Get() : 0;
    const auto transaction = ActiveCostTransaction(static_cast<Obj*>(self), effect);
    bool targetMatched = false;
    bool exactTarget = transaction && TX::Resolve(transaction->model,
        costEffectEntityId[effect].Get(), target, false, targetMatched);
    int selectedTarget = -1;
    if (exactTarget && !targetMatched && transaction->cards.size() == 1) {
        selectedTarget = TX::LiveTarget(transaction, 0);
        if (selectedTarget > 0) {
            LOG_INFO("CustomComponents/Pick3/PF3_TX: phase=COST_TARGET_MISMATCH seq=%llu effect=%d group=%d actualTarget=%d selectedTarget=%d delta=%d changed=0",
                (unsigned long long)transaction->sequence, costEffectEntityId[effect].Get(),
                RT::IntAt(effect, 0x3C), target, selectedTarget, delta);
        }
    }
    if (transaction) {
        LOG_INFO("CustomComponents/Pick3/PF3_TX: phase=COST_EFFECT_ENTER seq=%llu effect=%d group=%d target=%d targetMatched=%d delta=%d",
            (unsigned long long)transaction->sequence, costEffectEntityId[effect].Get(),
            RT::IntAt(effect, 0x3C), target, int(exactTarget && targetMatched), delta);
    }
    LOG_INFO("CustomComponents/Pick3/PICK3_COST_EFFECT: phase=before effect=%p effectId=%d group=%d source=%d target=%d delta=%d seq=%llu",
        effect, effect && costEffectEntityId.IsValid() ? costEffectEntityId[effect].Get() : -1,
        effect ? RT::IntAt(effect, 0x3C) : -1, effect ? RT::IntAt(effect, 0x24) : -1,
        target, delta, (unsigned long long)(transaction ? transaction->sequence : 0));
    originalModifyCost(self, effect, method);
    LOG_INFO("CustomComponents/Pick3/PICK3_COST_EFFECT: phase=after effect=%p effectId=%d group=%d source=%d target=%d delta=%d seq=%llu",
        effect, effect && costEffectEntityId.IsValid() ? costEffectEntityId[effect].Get() : -1,
        effect ? RT::IntAt(effect, 0x3C) : -1, effect ? RT::IntAt(effect, 0x24) : -1,
        target, delta, (unsigned long long)(transaction ? transaction->sequence : 0));
}

static void RecordCostHook(void* self, Obj* effect, int appliedDelta, const void* method) {
    namespace RT = CustomComponents::Pick3::TransactionRuntime;
    const auto transaction = ActiveCostTransaction(static_cast<Obj*>(self), effect);
    const int target = effect && costEffectTargetId.IsValid() ? costEffectTargetId[effect].Get() : -1;
    const int effectId = effect && costEffectEntityId.IsValid() ? costEffectEntityId[effect].Get() : -1;
    LOG_INFO("CustomComponents/Pick3/PICK3_COST_EFFECT: phase=delta_recorded seq=%llu effectId=%d group=%d source=%d target=%d appliedDelta=%d",
        (unsigned long long)(transaction ? transaction->sequence : 0), effectId,
        effect ? RT::IntAt(effect, 0x3C) : -1, effect ? RT::IntAt(effect, 0x24) : -1,
        target, appliedDelta);
    if (originalRecordCost) originalRecordCost(self, effect, appliedDelta, method);
}

inline std::atomic<unsigned> events{0};
// Optional verified binding callback: invoked only after stock CreateCard returned.
inline void (*onVerifiedDatabaseDraw)(int sourceEffect, int createdId) = nullptr;
struct PendingDraw { Obj* effect; int sourceId; int guid; int group=-1; std::uint64_t sequence=0; Transaction::Ref transaction; };
inline std::mutex pendingMutex;
inline std::vector<PendingDraw> pendingDraws;
inline constexpr std::size_t kMaxPending = 64;
inline bool ShouldLog() { return events.fetch_add(1, std::memory_order_relaxed) < 200; }
inline int Id(Obj* entity) { return entity && entityId.IsValid() ? entityId[entity].Get() : -1; }

static int FindCardHook(void* self, Obj* query, Obj* effect, const void* method) {
    ExactRouting::ScopedStockFinder exactScope(effect,"EXISTING_CREATION_TRACE_FINDCARD");
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
            const auto transaction=Transaction::active.value;
            const bool exact=Transaction::Current(transaction) && transaction->effect==sourceId;
            const auto finderScope=ExactRouting::active;
            const bool scoped=!exact && finderScope.effect==sourceId && finderScope.group>=0 &&
                finderScope.sequence && finderScope.totalMatches==1;
            pendingDraws.push_back({effect,sourceId,guid,
                exact?transaction->group:(scoped?finderScope.group:-1),
                exact?transaction->sequence:(scoped?finderScope.sequence:0),exact?transaction:Transaction::Ref{}});
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
    if(matching.sourceId<=0) return created; // ordinary draws need no PF3 inspection
    const int entityId = Id(created);
    Obj* model=self && drawEntityModel.IsValid()?drawEntityModel[static_cast<Obj*>(self)].Get():nullptr;
    Obj* card=created && createdGetComponent.IsValid() && createdCardClass.IsValid()
        ? createdGetComponent[created](createdCardClass.GetMonoType()):nullptr;
    const int actualGuid=card && createdCardGuid.IsValid()?createdCardGuid[card].Get():-1;
    const bool identityVerified=entityId>0 && matching.guid>0 && actualGuid==matching.guid;
    if(!Transaction::Current(matching.transaction) && identityVerified) {
        matching.transaction=TransactionRuntime::FindAwaitingCreation(model,
            matching.sourceId,matching.group,matching.sequence,matching.guid);
        LOG_INFO("CustomComponents/Pick3/PF3_TX: phase=DATABASE_TRANSACTION_RECOVERY seq=%llu effect=%d group=%d entity=%d recovered=%d",
            (unsigned long long)matching.sequence,matching.sourceId,matching.group,entityId,
            int(Transaction::Current(matching.transaction)));
    }
    const bool hasTransaction=Transaction::Current(matching.transaction);
    const bool modelBound=identityVerified && hasTransaction && model &&
        matching.transaction->model==TransactionRuntime::ModelOf(model) &&
        Transaction::BindProduced(matching.transaction,matching.guid,entityId);
    // Exact model binding owns this outcome. Do not conceal its failure by
    // writing a shared compatibility entry from another prediction model.
    const bool compatibilityBound=identityVerified && matching.sourceId>0 &&
        (!hasTransaction || modelBound) && (matching.sequence
            ? Picked::BindCreatedExact(matching.sourceId,matching.group,matching.sequence,matching.guid,entityId)
            : Picked::BindCreated(matching.sourceId,matching.guid,entityId));
    // Compatibility metadata is not proof that a waiting transaction completed.
    const bool bound=hasTransaction?modelBound:compatibilityBound;
    LOG_INFO("CustomComponents/Pick3/PF3_TX: phase=CREATED_CARD_BIND seq=%llu effect=%d expectedGuid=%d actualGuid=%d entity=%d modelBound=%d compatibilityBound=%d",
        (unsigned long long)matching.sequence,matching.sourceId,matching.guid,actualGuid,entityId,int(modelBound),int(compatibilityBound));
    if(modelBound && matching.sequence)
        TransactionRuntime::Produced(model,effect,
            matching.sourceId,matching.group,matching.sequence);
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
    drawEntityModel=draw.GetField("<Model>k__BackingField").cast<Obj*>();
    createdGetComponent=entity.GetMethod("GetComponent",1).cast<Obj*>();
    createdCardClass=BNM::Class("PvZCards.Engine.Components","Card");
    createdCardGuid=createdCardClass.GetField("Guid").cast<int>();
    entityId = entity.GetField("<Id>k__BackingField").cast<int>();
    // Fail closed: all methods must resolve before installing any hook.
    if (!find.IsValid() || !create.IsValid() || !source.IsValid() ||
        !transformCard.IsValid() || !entityId.IsValid() || !drawEntityModel.IsValid() ||
        !createdGetComponent.IsValid() || !createdCardGuid.IsValid()) {
        LOG_ERR("CustomComponents/Pick3/CREATION_TRACE: preflight=failed no-hooks");
        return false;
    }
    BNM::BasicHook(find, FindCardHook, originalFindCard);
    BNM::BasicHook(create, DrawCreateHook, originalDrawCreate);
    BNM::BasicHook(source, PlaceSourceHook, originalPlaceSource);
    BNM::BasicHook(transformCard, TransformCardHook, originalTransformCard);
    auto modifyCostSystem = BNM::Class("PvZCards.Core.Systems", "ModifySunCostSystem");
    auto modifyCostEffect = BNM::Class("PvZCards.Engine.Effects", "ModifySunCostEffect");
    // Effect is declared in PvZCards.Engine; the derived ModifySunCostEffect
    // itself is in PvZCards.Engine.Effects.
    auto stockCostEffect = BNM::Class("PvZCards.Engine", "Effect");
    auto processCost = modifyCostSystem.GetMethod("ProcessEffect", {modifyCostEffect.GetCompileTimeClass()});
    if (!processCost.IsValid()) processCost = modifyCostSystem.GetMethod("ProcessEffect", 1);
    auto recordCost = modifyCostSystem.GetMethod("RecordSunCostModified", 2);
    costEffectTargetId = stockCostEffect.GetField("TargetId").cast<int>();
    costEffectEntityId = stockCostEffect.GetField("EffectEntityId").cast<int>();
    costSystemEntities = modifyCostSystem.GetField("<Entities>k__BackingField").cast<Obj*>();
    costEffectDelta = modifyCostEffect.GetField("SunCostDelta").cast<int>();
    if (processCost.IsValid() && costEffectTargetId.IsValid() && costEffectEntityId.IsValid() &&
        costSystemEntities.IsValid() && costEffectDelta.IsValid()) {
        BNM::BasicHook(processCost, ModifyCostHook, originalModifyCost);
    }
    if (recordCost.IsValid() && costEffectTargetId.IsValid() && costEffectEntityId.IsValid() &&
        !originalRecordCost) BNM::BasicHook(recordCost, RecordCostHook, originalRecordCost);
    LOG_INFO("CustomComponents/Pick3/PICK3_COST_TRACE: enabled=%d processResolved=%d processOffset=%u recordEnabled=%d recordResolved=%d targetField=%d effectField=%d entitiesField=%d deltaField=%d",
        originalModifyCost ? 1 : 0, processCost.IsValid() ? 1 : 0,
        processCost.IsValid() ? unsigned(processCost.GetOffset()) : 0,
        originalRecordCost ? 1 : 0, recordCost.IsValid() ? 1 : 0,
        costEffectTargetId.IsValid() ? 1 : 0, costEffectEntityId.IsValid() ? 1 : 0,
        costSystemEntities.IsValid() ? 1 : 0, costEffectDelta.IsValid() ? 1 : 0);
    const bool ready = originalFindCard && originalDrawCreate && originalPlaceSource && originalTransformCard;
    LOG_INFO("CustomComponents/Pick3/CREATION_TRACE: ready=%d finder=%d draw=%d placer=%d transform=%d drawBinding=1 otherAdaptersReadOnly=1 maxEvents=200",
        ready ? 1 : 0, originalFindCard ? 1 : 0, originalDrawCreate ? 1 : 0,
        originalPlaceSource ? 1 : 0, originalTransformCard ? 1 : 0);
    return ready;
}
} // namespace CustomComponents::Pick3::CreationTrace
