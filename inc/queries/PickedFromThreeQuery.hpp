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
#include "pick3/SelectionTransactionRuntime.hpp"
#include "pick3/Patch2ProvenanceObservation.hpp"
#include "pick3/Patch2QueryCallerObservation.hpp"
#include "pick3/Patch2ShadowResolution.hpp"
#include "pick3/Patch2ExactRouting.hpp"
#include "log.h"
#include "pick3/SelectionResolutionPolicy.hpp"

namespace CustomComponents::Queries {

inline BNM::Method<int> pickedFromThreeContextEffectId;
inline BNM::Field<int> pickedFromThreeEntityId;
// 2.6.2: Never invoke diagnostic-only managed getters from Matches().
// Reading get_Model/get_SourceId/get_TargetId/get_SpringboardId here could
// enter IL2CPP on a partially constructed or invalid context and crash Unity.
// The pre-existing get_EffectEntityId is needed for the query itself and stays.
inline std::atomic<unsigned int> pickedFromThreeQueryCalls{0};
inline std::atomic<unsigned int> patch2ContextCalls{0};
inline std::atomic<unsigned int> patch2MissingContextCalls{0};
inline std::atomic<unsigned int> patch2InvalidEffectCalls{0};

inline void TracePatch2QueryContext(const char* domain, Obj* query, Obj* entity, Obj* context,
                                    int effectId, int entityId, int guid, bool matched) {
#if CUSTOMCOMPONENTS_PF3_SNAPSHOT_TRACE
    const unsigned int n = ++patch2ContextCalls;
    if (n <= 180) {
        // Native pointers are only printed; never dereferenced to gather
        // additional log fields or retained as persistent identities.
        LOG_INFO("CustomComponents/Pick3/PATCH2_CONTEXT: domain=%s query=%p context=%p candidate=%p effect=%d entity=%d guid=%d matched=%d nativeOnly=1 diagnosticGetters=0 (OBSERVATION_ONLY)",
            domain, (void*)query, (void*)context, (void*)entity,
            effectId, entityId, guid, matched ? 1 : 0);
        CustomComponents::Pick3::Picked::TraceLedger(domain, effectId, -1, entityId > 0 ? entityId : guid);
    }
    if (!context) {
        const unsigned int missing = ++patch2MissingContextCalls;
        if (missing <= 16 || (missing & (missing - 1)) == 0)
            LOG_INFO("CustomComponents/Pick3/PATCH2_CONTEXT: phase=QUERY_CONTEXT_MISSING domain=%s event=%u effect=%d guid=%d nativeOnly=1 fallback=LEGACY",
                domain, missing, effectId, guid);
    } else if (effectId <= 0) {
        const unsigned int invalid = ++patch2InvalidEffectCalls;
        if (invalid <= 16 || (invalid & (invalid - 1)) == 0)
            LOG_INFO("CustomComponents/Pick3/PATCH2_CONTEXT: phase=QUERY_EFFECT_UNAVAILABLE domain=%s event=%u effect=%d guid=%d nativeOnly=1 fallback=LEGACY",
                domain, invalid, effectId, guid);
    }
#else
    (void)domain; (void)query; (void)entity; (void)context;
    (void)effectId; (void)entityId; (void)guid; (void)matched;
#endif
}

inline int PickedEffectId(Obj* context) {
    if (!context || !pickedFromThreeContextEffectId.IsValid()) return -1;
    int id = -1;
    const auto ex = BNM::TryInvoke([&] { id = pickedFromThreeContextEffectId[context](); });
    return ex.IsValid() ? -1 : id;
}

// One winning resolver per evaluation. An exact negative is final, too.
using PickedResolution=CustomComponents::Pick3::SelectionResolution;
inline PickedResolution ResolvePicked(Obj* context,int effectId,int entity,int guid,bool guidDomain,bool legacy) {
    namespace P=CustomComponents::Pick3::Picked;
    if(guid<=0 || (!guidDomain && entity<=0)) return {false,"INVALID_CARD",false};
    namespace RT=CustomComponents::Pick3::TransactionRuntime;
    namespace TX=CustomComponents::Pick3::Transaction;
#if CUSTOMCOMPONENTS_PF3_TRANSACTION_PIPELINE && CUSTOMCOMPONENTS_PF3_TRANSACTION_ROUTING
    if(RT::pipelineReady && TX::Current(TX::active.value) &&
       (effectId==TX::active.effect || effectId==TX::active.contextEffect) &&
       (!context || !RT::queryModel.IsValid() || RT::ModelOf(RT::queryModel[context].Get())!=TX::active.value->model))
        return {false,"SCOPE_MODEL_MISMATCH",false};
#endif
    return CustomComponents::Pick3::ResolveSelectionByPriority(legacy,
        [&](bool& result) { return CustomComponents::Pick3::TransactionRuntime::Resolve(context,effectId,guidDomain?guid:entity,guidDomain,result); },
        [&](bool& result) {
#if CUSTOMCOMPONENTS_PF3_ENTITY_LINK_ROUTING
            return P::DeferredLink::Resolve(effectId,guidDomain?guid:entity,guidDomain,result);
#else
            return false;
#endif
        },
        [&](bool& result) {
            const auto& flight=P::InFlight::active;
            if(!flight.valid || !context || context!=flight.context || effectId<=0 || effectId!=flight.effect) return false;
            result=guidDomain ? P::InFlight::Matches(context,effectId,guid) : P::InFlight::MatchesEntity(context,effectId,entity);
            return true;
        },
        [&](bool& result) { return guidDomain
            ? CustomComponents::Pick3::ExactRouting::Evaluate(context,effectId,guid,legacy,result)
            : CustomComponents::Pick3::ExactRouting::EvaluateEntity(context,effectId,entity,guid,legacy,result); });
}
inline std::atomic<unsigned> pickedResolutionLogs{0};
inline void LogPickedResolution(const char* domain,int effect,int entity,int guid,const PickedResolution& r) {
    const auto n=++pickedResolutionLogs;
    if(n<=256 || r.matched) LOG_INFO("CustomComponents/Pick3/PF3_QUERY: domain=%s effect=%d entity=%d guid=%d resolver=%s matched=%d seq=%llu",
        domain,effect,entity,guid,r.origin,int(r.matched),
        (unsigned long long)(r.transaction?CustomComponents::Pick3::Transaction::active.value->sequence:0));
}

struct PickedFromThreeQuery final : BNM::IL2CPP::Il2CppObject {
    void Constructor() {
        auto ctor = BNM::Class("PvZCards.Engine.Queries", "InHandQuery").GetMethod(".ctor", 0);
        if (ctor.IsValid()) ctor.cast<void>()[this]();
    }
    bool Matches(Obj* entity, Obj* context) {
        const int effectId = PickedEffectId(context);
        const int guid = InDeckCardGuidOf(entity);
        const bool legacyMatch = CustomComponents::Pick3::Picked::Matches(effectId, guid, true);
        bool match = legacyMatch;
        TracePatch2QueryContext("GUID", this, entity, context, effectId, -1, guid, legacyMatch);
        CustomComponents::Pick3::Provenance::Query("GUID", effectId, guid, -1, match);
        CustomComponents::Pick3::QueryCaller::ObserveQuery(context,"GUID",effectId);
        CustomComponents::Pick3::Shadow::Observe("GUID",context,effectId,-1,guid,legacyMatch,false);
        const auto resolved=ResolvePicked(context,effectId,-1,guid,true,legacyMatch);
        match=resolved.matched;
        LogPickedResolution("GUID",effectId,-1,guid,resolved);
        if(match && CustomComponents::Pick3::Picked::finderObserving)
            CustomComponents::Pick3::Picked::ObserveGuidMatch(resolved.transaction
                ? CustomComponents::Pick3::Transaction::active.value->effect : effectId,guid);
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
        // Database selections require their verified created entity; GUIDs do not identify copies.
        const int cardGuid = InDeckCardGuidOf(entity);
        const bool legacyMatch = CustomComponents::Pick3::Picked::MatchesLatestLiveCard(entityId, cardGuid);
        bool match = legacyMatch;
        TracePatch2QueryContext("ENTITY", this, entity, context, effectId, entityId, cardGuid, match);
        CustomComponents::Pick3::Provenance::Query("ENTITY", effectId, cardGuid, entityId, match);
        CustomComponents::Pick3::QueryCaller::ObserveQuery(context,"ENTITY",effectId);
        CustomComponents::Pick3::Shadow::Observe("ENTITY",context,effectId,entityId,cardGuid,legacyMatch,true);
        const auto resolved=ResolvePicked(context,effectId,entityId,cardGuid,false,legacyMatch);
        match=resolved.matched;
        LogPickedResolution("ENTITY",effectId,entityId,cardGuid,resolved);
        const unsigned int n = ++pickedFromThreeQueryCalls;
        if (n <= 120) LOG_INFO("CustomComponents/Pick3/PICK3_ENTITY_QUERY: contextEffect=%d candidateEntity=%d candidateGuid=%d matched=%d", effectId, entityId, cardGuid, match ? 1 : 0);
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
#if CUSTOMCOMPONENTS_PF3_SNAPSHOT_TRACE
    LOG_INFO("CustomComponents/Pick3/PATCH2_CONTEXT: INIT version=2.16 trace=1 diagnosticGetters=0 (OBSERVATION_ONLY)");
#endif
    pickedFromThreeContextEffectId = context.GetMethod("get_EffectEntityId", 0).cast<int>();
#if CUSTOMCOMPONENTS_PF3_SNAPSHOT_TRACE
    LOG_INFO("CustomComponents/Pick3/PATCH2_CONTEXT: phase=CONTEXT_FIELDS version=2.16 effect=%d diagnosticGetters=0 nativeOnly=1 reason=AVOID_IL2CPP_IN_QUERY_LOGGING",
        pickedFromThreeContextEffectId.IsValid());
#endif
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
