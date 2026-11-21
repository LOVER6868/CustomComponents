#pragma once

// Read the stock PrimaryTargets.TargetEntityIds selected for an effect entity.
// This is *not* EntityQueryContext.TargetId, which denotes the trigger target.
// Optional FromAbilityGroupId lets a later effect group reference an earlier
// selected group on the SAME ability owner, without global selection history.

#include <BNM/Class.hpp>
#include <BNM/ClassesManagement.hpp>
#include <BNM/Defaults.hpp>
#include <BNM/Field.hpp>
#include <BNM/Method.hpp>
#include <BNM/BasicMonoStructures.hpp>

#include "queries/LaneQueries.hpp"
#include "queries/PrimaryTargetsSelectionPolicy.hpp"

namespace CustomComponents::Queries {

namespace PrimaryTargetsAccess {
using IntList = BNM::Structures::Mono::List<int>;
using Obj = BNM::IL2CPP::Il2CppObject;

inline BNM::Field<IntList*> targetIds;
inline BNM::Field<int> sourceId;
inline BNM::Field<int> groupId;
inline BNM::Method<int> contextEffectId;
inline BNM::IL2CPP::Il2CppClass* primaryType = nullptr;

inline bool Read(Obj* effect, PrimaryTargetsSelectionPolicy::Selection& selection) {
    Obj* component = Engine::EntityAccess::Component(effect, "PrimaryTargets");
    if (!component || component->klass != primaryType || !targetIds.IsValid()) return false;
    const auto* list = targetIds[component].Get();
    if (!list || !list->items || list->size <= 0 ||
        list->size > static_cast<int>(PrimaryTargetsSelectionPolicy::kMaxTargets) ||
        list->size > static_cast<int>(list->items->capacity)) return false;
    return selection.Add(list->items->m_Items, static_cast<std::size_t>(list->size));
}

inline bool Matches(Obj* candidate, Obj* context, int fromGroup, bool byGuid = false) {
    if (!candidate || !context || fromGroup < -1 || !contextEffectId.IsValid())
        return false;
    Obj* model = Engine::BoardPlacement::Model(context);
    const int candidateId = Engine::Match::Id(candidate);
    if (!model || (!byGuid && candidateId <= 0) ||
        (!byGuid && Engine::EntityAccess::Find(model, candidateId) != candidate)) return false;

    const auto selectedMatches = [&](const PrimaryTargetsSelectionPolicy::Selection& selected) {
        if (!byGuid) return selected.Matches(candidateId);
        if (!selected.present || selected.ambiguous) return false;
        const int guid = Engine::BoardPlacement::Guid(candidate);
        if (guid <= 0) return false;
        for (const int id : selected.ids) {
            Obj* card = Engine::EntityAccess::Find(model, id);
            if (card && Engine::BoardPlacement::Guid(card) == guid) return true;
        }
        return false;
    };
    const int effectId = contextEffectId[context]();
    Obj* effect = effectId > 0 ? Engine::EntityAccess::Find(model, effectId) : nullptr;
    if (!effect) return false;
    PrimaryTargetsSelectionPolicy::Selection selection;

    if (fromGroup == -1) {
        // The simplest and safest case: follow the PrimaryTargets that stock
        // attached to this exact effect entity. A missing selection isn't a
        // reason to substitute the trigger target.
        PrimaryTargetsAccess::Read(effect, selection);
        return selectedMatches(selection);
    }

    Obj* currentEffect = Engine::EntityAccess::Component(effect, "EffectEntity");
    if (!currentEffect || !sourceId.IsValid() || !groupId.IsValid()) return false;
    const int ownerId = sourceId[currentEffect].Get();
    if (ownerId <= 0) return false;

    auto api = Engine::EntityAccess::Api();
    if (!api.entities.IsValid()) return false;
    auto* entities = api.entities[model].Get();
    if (!Engine::EntityAccess::Valid(entities)) return false;
    for (int i = 0; i < entities->size; ++i) {
        Obj* donor = entities->items->m_Items[i];
        if (!donor) continue;
        Obj* owner = Engine::EntityAccess::Component(donor, "EffectEntity");
        Obj* group = Engine::EntityAccess::Component(donor, "EffectEntityGrouping");
        if (!owner || !group || sourceId[owner].Get() != ownerId ||
            groupId[group].Get() != fromGroup) continue;
        PrimaryTargetsAccess::Read(donor, selection);
    }
    // Different selected ID sets on eligible donors = ambiguous, not a union.
    return selectedMatches(selection);
}
}  // namespace PrimaryTargetsAccess

struct PrimaryTargetsQuery : Obj {
    // -1: own effect entity. >=0: the selected effect entity (or entities) in
    // that AbilityGroupId, on the same owning card instance.
    int FromAbilityGroupId = -1;

    BNM_CustomClass(PrimaryTargetsQuery,
        BNM::CompileTimeClassBuilder("PvZCards.Engine.Queries", "PrimaryTargetsQuery", "EngineLib.dll").Build(),
        BNM::CompileTimeClassBuilder("PvZCards.Engine.Queries", "InHandQuery", "EngineLib.dll").Build(), {});
    BNM_CustomField(FromAbilityGroupId, BNM::Defaults::Get<int>(), "FromAbilityGroupId");

    void Constructor() {
        auto ctor = BNM::Class("PvZCards.Engine.Queries", "InHandQuery").GetMethod(".ctor", 0).cast<void>();
        if (ctor.IsValid()) ctor[this]();
        FromAbilityGroupId = -1;
    }
    bool Matches(Obj* candidate, Obj* context) {
        return PrimaryTargetsAccess::Matches(candidate, context, FromAbilityGroupId);
    }
    void DeepCopyFrom(Obj* source, Obj*) {
        FromAbilityGroupId = source ? reinterpret_cast<PrimaryTargetsQuery*>(source)->FromAbilityGroupId : -1;
    }
    void HandleReturnToPool(Obj*) { FromAbilityGroupId = -1; }
    BNM_CustomMethod(Constructor, false, BNM::Defaults::Get<void>(), ".ctor");
    BNM_CustomMethod(Matches, false, BNM::Defaults::Get<bool>(), "Matches",
        BNM::CompileTimeClassBuilder("PvZCards.Engine", "Entity", "EngineLib.dll").Build(),
        BNM::CompileTimeClassBuilder("PvZCards.Engine", "EntityQueryContext", "EngineLib.dll").Build());
    BNM_CustomMethodSkipTypeMatch(Matches);
    BNM_CustomMethod(DeepCopyFrom, false, BNM::Defaults::Get<void>(), "DeepCopyFrom",
        BNM::Defaults::Get<Obj*>(),
        BNM::CompileTimeClassBuilder("PvZCards.Engine", "InstanceManager", "EngineLib.dll").Build());
    BNM_CustomMethod(HandleReturnToPool, false, BNM::Defaults::Get<void>(), "HandleReturnToPool",
        BNM::CompileTimeClassBuilder("PvZCards.Engine", "InstanceManager", "EngineLib.dll").Build());
};

struct PrimaryTargetsGuidQuery : Obj {
    // -1: own effect entity. >=0: the selected effect entity (or entities) in
    // that AbilityGroupId, on the same owning card instance.
    int FromAbilityGroupId = -1;

    BNM_CustomClass(PrimaryTargetsGuidQuery,
        BNM::CompileTimeClassBuilder("PvZCards.Engine.Queries", "PrimaryTargetsGuidQuery", "EngineLib.dll").Build(),
        BNM::CompileTimeClassBuilder("PvZCards.Engine.Queries", "InHandQuery", "EngineLib.dll").Build(), {});
    BNM_CustomField(FromAbilityGroupId, BNM::Defaults::Get<int>(), "FromAbilityGroupId");

    void Constructor() {
        auto ctor = BNM::Class("PvZCards.Engine.Queries", "InHandQuery").GetMethod(".ctor", 0).cast<void>();
        if (ctor.IsValid()) ctor[this]();
        FromAbilityGroupId = -1;
    }
    bool Matches(Obj* candidate, Obj* context) {
        return PrimaryTargetsAccess::Matches(candidate, context, FromAbilityGroupId, true);
    }
    void DeepCopyFrom(Obj* source, Obj*) {
        FromAbilityGroupId = source ? reinterpret_cast<PrimaryTargetsGuidQuery*>(source)->FromAbilityGroupId : -1;
    }
    void HandleReturnToPool(Obj*) { FromAbilityGroupId = -1; }
    BNM_CustomMethod(Constructor, false, BNM::Defaults::Get<void>(), ".ctor");
    BNM_CustomMethod(Matches, false, BNM::Defaults::Get<bool>(), "Matches",
        BNM::CompileTimeClassBuilder("PvZCards.Engine", "Entity", "EngineLib.dll").Build(),
        BNM::CompileTimeClassBuilder("PvZCards.Engine", "EntityQueryContext", "EngineLib.dll").Build());
    BNM_CustomMethodSkipTypeMatch(Matches);
    BNM_CustomMethod(DeepCopyFrom, false, BNM::Defaults::Get<void>(), "DeepCopyFrom",
        BNM::Defaults::Get<Obj*>(),
        BNM::CompileTimeClassBuilder("PvZCards.Engine", "InstanceManager", "EngineLib.dll").Build());
    BNM_CustomMethod(HandleReturnToPool, false, BNM::Defaults::Get<void>(), "HandleReturnToPool",
        BNM::CompileTimeClassBuilder("PvZCards.Engine", "InstanceManager", "EngineLib.dll").Build());
};

inline bool InitializePrimaryTargetsQuery() {
    const auto query = BNM::Class("PvZCards.Engine.Queries", "PrimaryTargetsQuery");
    const auto base = BNM::Class("PvZCards.Engine.Queries", "InHandQuery");
    const auto stockTargets = BNM::Class("PvZCards.Engine.Components", "Targets");
    const auto stockPrimary = BNM::Class("PvZCards.Engine.Components", "PrimaryTargets");
    const auto context = BNM::Class("PvZCards.Engine", "EntityQueryContext");
    const auto owner = BNM::Class("PvZCards.Engine.Components", "EffectEntity");
    const auto grouping = BNM::Class("PvZCards.Engine.Components", "EffectEntityGrouping");

    PrimaryTargetsAccess::targetIds = stockTargets.GetField("TargetEntityIds").cast<PrimaryTargetsAccess::IntList*>();
    PrimaryTargetsAccess::sourceId = owner.GetField("SourceId").cast<int>();
    PrimaryTargetsAccess::groupId = grouping.GetField("AbilityGroupId").cast<int>();
    PrimaryTargetsAccess::contextEffectId = context.GetMethod("get_EffectEntityId", 0).cast<int>();
    PrimaryTargetsAccess::primaryType = stockPrimary.GetClass();

    if (!query.IsValid() || !base.IsValid() || !stockPrimary.IsValid() ||
        !PrimaryTargetsAccess::targetIds.IsValid() || !PrimaryTargetsAccess::sourceId.IsValid() ||
        !PrimaryTargetsAccess::groupId.IsValid() || !PrimaryTargetsAccess::contextEffectId.IsValid() ||
        !PrimaryTargetsAccess::primaryType ||
        !PatchInDeckQueryDispatch(query, base, "PrimaryTargetsQuery") ||
        !PatchDiscardedCopyDispatch(query, base) ||
        !Core::ComponentLifecycle::Patch(query, base) ||
        !RegisterQuery({"PrimaryTargetsQuery", "PvZCards.Engine.Queries.PrimaryTargetsQuery",
            "PvZCards.Engine.Queries", "PrimaryTargetsQuery",
            "PvZCards.Engine.Queries", "InHandQuery"}) ||
        !Systems::CustomFieldBridgeSystem::Register("PvZCards.Engine.Queries", "PrimaryTargetsQuery",
            "PvZCards.Engine.Queries", "InHandQuery", {{"FromAbilityGroupId", K::Int32}}, false)) {
        LOG_ERR("CustomComponents/PrimaryTargetsQuery: registration failed");
        return false;
    }
    const auto guidQuery = BNM::Class("PvZCards.Engine.Queries", "PrimaryTargetsGuidQuery");
    if (!guidQuery.IsValid() ||
        !PatchInDeckQueryDispatch(guidQuery, base, "PrimaryTargetsGuidQuery") ||
        !PatchDiscardedCopyDispatch(guidQuery, base) ||
        !Core::ComponentLifecycle::Patch(guidQuery, base) ||
        !RegisterQuery({"PrimaryTargetsGuidQuery", "PvZCards.Engine.Queries.PrimaryTargetsGuidQuery",
            "PvZCards.Engine.Queries", "PrimaryTargetsGuidQuery", "PvZCards.Engine.Queries", "InHandQuery"}) ||
        !Systems::CustomFieldBridgeSystem::Register("PvZCards.Engine.Queries", "PrimaryTargetsGuidQuery",
            "PvZCards.Engine.Queries", "InHandQuery", {{"FromAbilityGroupId", K::Int32}}, false)) {
        LOG_ERR("CustomComponents/PrimaryTargetsGuidQuery: registration failed");
        return false;
    }
    LOG_INFO("CustomComponents/PrimaryTargetsQuery: ready (current effect or explicit ability group)");
    return true;
}

}  // namespace CustomComponents::Queries
