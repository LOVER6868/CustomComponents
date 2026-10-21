#pragma once
#include "diagnostics/DiagnosticLogging.hpp"
// PickedFromThreeQuery evaluates the GUID of CardDatabase backing entities.
// PickedFromThreeEntityQuery evaluates live match Entity.Id values instead.
// Both use the stock EntityQuery interface through InHandQuery inheritance.

#include <cstdint>
#include <atomic>
#include <BNM/Class.hpp>
#include <BNM/ClassesManagement.hpp>
#include <BNM/Defaults.hpp>
#include <BNM/Field.hpp>
#include <BNM/Method.hpp>
#include <BNM/Exceptions.hpp>

#include "queries/InDeckQuery.hpp"
#include "pick3/PickedFromThreeSelection.hpp"
#include "log.h"

namespace CustomComponents::Queries {

inline BNM::Method<int> pickedFromThreeContextEffectId;
inline BNM::Field<int> pickedFromThreeEntityId;
inline std::atomic<unsigned int> pickedFromThreeQueryCalls{0};

inline int PickedEffectId(Obj* context) {
    if (!context || !pickedFromThreeContextEffectId.IsValid()) return -1;
    int id = -1;
    const auto ex = BNM::TryInvoke([&] { id = pickedFromThreeContextEffectId[context](); });
    return ex.IsValid() ? -1 : id;
}

struct PickedFromThreeQuery final : BNM::IL2CPP::Il2CppObject {
    void Constructor() {
        auto ctor = BNM::Class("PvZCards.Engine.Queries", "InHandQuery").GetMethod(".ctor", 0);
        if (ctor.IsValid()) ctor.cast<void>()[this]();
    }
    bool Matches(Obj* entity, Obj* context) {
        const int effectId = PickedEffectId(context);
        const int guid = InDeckCardGuidOf(entity);
        const bool match = CustomComponents::Pick3::Picked::Matches(effectId, guid, true);
        const unsigned int n = ++pickedFromThreeQueryCalls;
        if (n <= 36) CC_DIAG("CustomComponents/Pick3/PICKED_QUERY: domain=GUID effect=%d candidateGuid=%d match=%d",
                              effectId, guid, match ? 1 : 0);
        return match;
    }
    BNM_CustomClass(PickedFromThreeQuery,
        BNM::CompileTimeClassBuilder("PvZCards.Engine.Queries", "PickedFromThreeQuery", "EngineLib.dll").Build(),
        BNM::CompileTimeClassBuilder("PvZCards.Engine.Queries", "InHandQuery", "EngineLib.dll").Build(), {});
    BNM_CustomMethod(Constructor, false, BNM::Defaults::Get<void>(), ".ctor");
    BNM_CustomMethod(Matches, false, BNM::Defaults::Get<bool>(), "Matches",
        BNM::CompileTimeClassBuilder("PvZCards.Engine", "Entity", "EngineLib.dll").Build(),
        BNM::CompileTimeClassBuilder("PvZCards.Engine", "EntityQueryContext", "EngineLib.dll").Build());
    BNM_CustomMethodSkipTypeMatch(Matches);
};

struct PickedFromThreeEntityQuery final : BNM::IL2CPP::Il2CppObject {
    void Constructor() {
        auto ctor = BNM::Class("PvZCards.Engine.Queries", "InHandQuery").GetMethod(".ctor", 0);
        if (ctor.IsValid()) ctor.cast<void>()[this]();
    }
    bool Matches(Obj* entity, Obj* context) {
        const int effectId = PickedEffectId(context);
        const int entityId = entity && pickedFromThreeEntityId.IsValid()
            ? pickedFromThreeEntityId[entity].Get() : -1;
        // Live-entity query is explicitly cross-effect: Captain Cucumber can
        // exclude the card chosen by a different source effect earlier.
        // DB selections have no Entity.Id; match the card GUID instead.
        const int cardGuid = InDeckCardGuidOf(entity);
        const bool match = CustomComponents::Pick3::Picked::MatchesLatestLiveCard(entityId, cardGuid);
        const unsigned int n = ++pickedFromThreeQueryCalls;
        if (n <= 36) CC_DIAG("CustomComponents/Pick3/PICKED_QUERY: domain=LATEST_ENTITY effect=%d candidateId=%d cardGuid=%d match=%d",
                              effectId, entityId, cardGuid, match ? 1 : 0);
        return match;
    }
    BNM_CustomClass(PickedFromThreeEntityQuery,
        BNM::CompileTimeClassBuilder("PvZCards.Engine.Queries", "PickedFromThreeEntityQuery", "EngineLib.dll").Build(),
        BNM::CompileTimeClassBuilder("PvZCards.Engine.Queries", "InHandQuery", "EngineLib.dll").Build(), {});
    BNM_CustomMethod(Constructor, false, BNM::Defaults::Get<void>(), ".ctor");
    BNM_CustomMethod(Matches, false, BNM::Defaults::Get<bool>(), "Matches",
        BNM::CompileTimeClassBuilder("PvZCards.Engine", "Entity", "EngineLib.dll").Build(),
        BNM::CompileTimeClassBuilder("PvZCards.Engine", "EntityQueryContext", "EngineLib.dll").Build());
    BNM_CustomMethodSkipTypeMatch(Matches);
};

inline bool InitializePickedFromThreeQueries() {
    const auto guid = BNM::Class("PvZCards.Engine.Queries", "PickedFromThreeQuery");
    const auto live = BNM::Class("PvZCards.Engine.Queries", "PickedFromThreeEntityQuery");
    const auto base = BNM::Class("PvZCards.Engine.Queries", "InHandQuery");
    const auto context = BNM::Class("PvZCards.Engine", "EntityQueryContext");
    const auto entity = BNM::Class("PvZCards.Engine", "Entity");
    pickedFromThreeContextEffectId = context.GetMethod("get_EffectEntityId", 0).cast<int>();
    pickedFromThreeEntityId = entity.GetField("<Id>k__BackingField").cast<int>();
    if (!guid.IsValid() || !live.IsValid() || !base.IsValid() ||
        !pickedFromThreeContextEffectId.IsValid() || !pickedFromThreeEntityId.IsValid() ||
        !PatchInDeckQueryDispatch(guid, base, "PickedFromThreeQuery") ||
        !PatchInDeckQueryDispatch(live, base, "PickedFromThreeEntityQuery")) {
        LOG_ERR("CustomComponents/Pick3/PICKED_QUERY: registration preflight failed");
        return false;
    }
    if (!RegisterQuery({"PickedFromThreeQuery", "PvZCards.Engine.Queries.PickedFromThreeQuery",
                        "PvZCards.Engine.Queries", "PickedFromThreeQuery",
                        "PvZCards.Engine.Queries", "InHandQuery"}) ||
        !RegisterQuery({"PickedFromThree", "PvZCards.Engine.Queries.PickedFromThree",
                        "PvZCards.Engine.Queries", "PickedFromThreeQuery",
                        "PvZCards.Engine.Queries", "InHandQuery"}) ||
        !RegisterQuery({"PickedFromThreeGuidQuery", "PvZCards.Engine.Queries.PickedFromThreeGuidQuery",
                        "PvZCards.Engine.Queries", "PickedFromThreeQuery",
                        "PvZCards.Engine.Queries", "InHandQuery"}) ||
        !RegisterQuery({"PickedFromThreeEntityQuery", "PvZCards.Engine.Queries.PickedFromThreeEntityQuery",
                        "PvZCards.Engine.Queries", "PickedFromThreeEntityQuery",
                        "PvZCards.Engine.Queries", "InHandQuery"})) return false;
    CC_DIAG("CustomComponents/Pick3/PICKED_QUERY: ready names=PickedFromThree,PickedFromThreeQuery,PickedFromThreeGuidQuery,PickedFromThreeEntityQuery");
    return true;
}
} // namespace CustomComponents::Queries
