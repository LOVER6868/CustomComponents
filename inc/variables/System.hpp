#pragma once
#include "variables/Queries.hpp"
#include "variables/Multiplier.hpp"
#include "engine/PhasePermissions.hpp"
namespace CustomComponents::Variables {
inline bool Initialize(){
#define VARIABLE_STEP(call,label) if(!(call)){LOG_ERR("CustomComponents/Variables: initialization failed at " label);return false;}
    VARIABLE_STEP(InitializeStorage(),"storage");
    VARIABLE_STEP(InitializeDescriptors(),"descriptor");
    VARIABLE_STEP(InitializeQueries(),"queries");
    VARIABLE_STEP(InitializeMultiplier(),"multiplier");
    VARIABLE_STEP(Engine::SimulationChanges::Initialize(),"completion/change tracking");
    VARIABLE_STEP(Engine::EffectGuards::RegisterPreparation(Prepare,-30),"setter");
    VARIABLE_STEP(Engine::EffectGuards::RegisterPreparation(Reactive::Prepare,-29),"continuous grant");
    VARIABLE_STEP(Engine::SimulationChanges::settled.Add(Reactive::Reconcile),"reactive reconciliation");
    VARIABLE_STEP(Engine::PhasePermissions::Initialize(),"card-local phase permission");
#undef VARIABLE_STEP
    LOG_INFO("CustomComponents/Variables: ready typed=1 reactive=revision-based SelfTeleport=card-local");return true;
}
}
