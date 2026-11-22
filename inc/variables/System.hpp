#pragma once
#include "variables/Queries.hpp"
#include "variables/Multiplier.hpp"
#include "variables/ContinuousMultiplier.hpp"
#include "variables/EventCounter.hpp"
#include "variables/TriggerTiming.hpp"
#include "variables/QueuedTriggerValues.hpp"
#include "engine/PhasePermissions.hpp"
namespace CustomComponents::Variables {
// Read-only phase ordering trace: distinguish the actual Zombie surprise
// phase-start router event from a plant ability's eventual BuffEffect.
inline void TraceSurpriseOrder(const Engine::EffectGuards::Context& context){
    Obj* effect=context.effect;
    if(!effect || !effect->klass || !effect->klass->name)return;
    const std::string_view name=effect->klass->name;
    if(name!="SurprisePhaseStartEffect" && name!="BuffEffect")return;
    Obj* model=Core::ModelRegistry::Get(Engine::RouterContext::RouterModel(context.router));
    if(!model)return;
    if(name=="SurprisePhaseStartEffect"){
        static std::atomic<unsigned> samples{0};if(samples.fetch_add(1)<28)
            LOG_INFO("CustomComponents/Phase/SURPRISE_START: player=%d effect=%d",Field(effect,"PlayerId"),Field(effect,"EffectEntityId"));
        return;
    }
    Obj* ability=Engine::EntityAccess::Find(model,Field(effect,"EffectEntityId"));
    Obj* source=Engine::EntityAccess::Find(model,Source(ability));
    if(Engine::BoardPlacement::Guid(source)!=732)return;
    static std::atomic<unsigned> samples{0};if(samples.fetch_add(1)<28)
        LOG_INFO("CustomComponents/Phase/MYTHICAL_BUFF: source=%d effectEntity=%d target=%d",
            Engine::Match::Id(source),Field(effect,"EffectEntityId"),Field(effect,"TargetId"));
}
inline bool Initialize(){
#define VARIABLE_STEP(call,label) if(!(call)){LOG_ERR("CustomComponents/Variables: initialization failed at " label);return false;}
    VARIABLE_STEP(InitializeResolver(),"resolver metadata");
    VARIABLE_STEP(InitializeStorage(),"storage");
    if(!EventCounter::Initialize())LOG_ERR("CustomComponents/Variables: authored event counters unavailable; ordinary variable effects remain enabled");
    // The snapshot adapter is an optional enhancement. A new game build may
    // change an IL2CPP generic or hook entrypoint; do not abort unrelated
    // descriptor registration (including SwapPosition) when that happens.
    if(!QueuedTriggerValues::Initialize())
        LOG_ERR("CustomComponents/TriggerSnapshots: unavailable; using live variable state until hook is repaired");
    VARIABLE_STEP(InitializeDescriptors(),"descriptor");
    VARIABLE_STEP(InitializeQueries(),"queries");
    VARIABLE_STEP(InitializeMultiplier(),"multiplier");
    VARIABLE_STEP(ContinuousMultipliers::Initialize(),"continuous multiplier refresh");
    VARIABLE_STEP(TriggerTiming::Initialize(),"normal effect trigger timing");
    VARIABLE_STEP(Engine::SimulationChanges::Initialize(),"completion/change tracking");
    VARIABLE_STEP(Engine::EffectGuards::RegisterPreparation(Prepare,-30),"setter");

    VARIABLE_STEP(Engine::EffectGuards::RegisterPreparation(Reactive::Prepare,-29),"continuous grant");
    VARIABLE_STEP(Engine::SimulationChanges::settled.Add(Reactive::Reconcile),"reactive reconciliation");
    VARIABLE_STEP(Engine::EffectGuards::before.Add(TraceSurpriseOrder),"surprise-phase trace");
    VARIABLE_STEP(Engine::PhasePermissions::Initialize(),"card-local phase permission");
#undef VARIABLE_STEP
    LOG_INFO("CustomComponents/Variables: ready typed=1 reactive=revision-based SelfTeleport=card-local");return true;
}
}
