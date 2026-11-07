#pragma once
#include "level/LevelSelectionController.hpp"
#include "queries/LeveledQuery.hpp"
namespace CustomComponents::Level {
inline bool Initialize(){
#define LEVEL_STEP(call,name) if(!(call)){LOG_ERR("CustomComponents/Level: initialization failed at " name);return false;}
    LEVEL_STEP(InitializeComponent(),"table component");
    LEVEL_STEP(InitializeLeveled(),"committed state");
    LEVEL_STEP(Queries::InitializeLeveledQuery(),"query");
    LEVEL_STEP(Plays::Initialize(),"wire and play copies");
    LEVEL_STEP(Plays::InitializeEffectMetadata(),"queued play metadata");
    LEVEL_STEP(InitializeRuntime(),"payment and departure");
    LEVEL_STEP(Selection::InitializeSelection(),"hand presentation");
#undef LEVEL_STEP
    LOG_INFO("CustomComponents/Level: ready wireVersion=1 gesture=hand-only handModes=synchronized");return true;
}
}
