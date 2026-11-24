#pragma once
#include <atomic>
#include <optional>
#include <BNM/Class.hpp>
#include <BNM/Method.hpp>
#include <BNM/Field.hpp>
#include "core/HookRegistry.hpp"
#include "core/Performance.hpp"

namespace CustomComponents::Engine::AiPerformance {
using Obj = BNM::IL2CPP::Il2CppObject;
inline void (*originalUpdate)(Obj*, const void*) = nullptr;
inline void (*originalEndTurn)(Obj*,Obj*,const void*)=nullptr;
inline void EndTurn(Obj* service,Obj* play,const void* method){
    Core::Performance::Scope timing(Core::Performance::Area::TurnPass,"local-end-turn");
    originalEndTurn(service,play,method);
}
inline void (*originalSelectTarget)(Obj*,const void*)=nullptr;
inline void (*originalSimulateUpdate)(Obj*,const void*)=nullptr;
inline void (*originalCopyGameState)(Obj*,Obj*,const void*)=nullptr;
inline void CopyGameState(Obj* copier,Obj* injector,const void* method){
    const bool ai=Core::Performance::aiDepth!=0;
    std::optional<Core::Performance::Scope> timing;
    if(ai)timing.emplace(Core::Performance::Area::AiModelCopy,"AI-GameStateCopier.CopyGameStateTo");
    originalCopyGameState(copier,injector,method);
}
inline void SimulateUpdate(Obj* executor,const void* method){
    Core::Performance::AiScope ai;
    Core::Performance::Scope timing(Core::Performance::Area::AiCandidateStep,"stock-simulate-step");
    originalSimulateUpdate(executor,method);
}
inline void SelectTarget(Obj* executor,const void* method){
    Core::Performance::AiScope ai;
    Core::Performance::Scope timing(Core::Performance::Area::AiCandidateStep,"select-target");
    if(!Core::Performance::enabled){originalSelectTarget(executor,method);return;}
    // AiActionDescriptor is declared on the closed generic parent
    // AiActionExecutor<AiSelectTargetActionDescriptor>, not on this subclass.
    static auto descriptor=[] {
        auto executor=BNM::Class("PvZCards.Core.AI.AiActions","AiSelectTargetActionExecutor");
        return executor.GetParent().GetField("AiActionDescriptor").cast<Obj*>();
    }();
    static auto target=BNM::Class("PvZCards.Core.AI.AiActions","AiSelectTargetActionDescriptor").GetField("TargetId").cast<int>();
    static auto player=BNM::Class("PvZCards.Core.AI.AiActions","AiSelectTargetActionDescriptor").GetField("PlayerId").cast<int>();
    Obj* selection=executor && descriptor.IsValid()?descriptor[executor].Get():nullptr;
    const int selectedTarget=selection && target.IsValid()?target[selection].Get():-1;
    const int owner=selection && player.IsValid()?player[selection].Get():-1;
    static std::atomic<unsigned> samples{0};
    if(samples.fetch_add(1)<128)LOG_INFO("CustomComponents/AI/PF3_TARGET: phase=SELECT_BEGIN executor=%p descriptor=%p player=%d target=%d stockSelection=1 hiddenCardData=none",
        executor,selection,owner,selectedTarget);
    originalSelectTarget(executor,method);
    if(samples.load()<128)LOG_INFO("CustomComponents/AI/PF3_TARGET: phase=SELECT_RETURN executor=%p player=%d target=%d nativeResult=async-or-stock",
        executor,owner,selectedTarget);
}

inline void Update(Obj* executor, const void* method) {
    // No sampling: three-second Performance reports contain aggregate call
    // count, total, maximum and exclusive time, including idle polling.
    // The Scope is inert unless the opt-in Performance setting is enabled.
    Core::Performance::AiScope ai;
    Core::Performance::Scope timing(Core::Performance::Area::AiScorerPoll,"scorer-poll-all");
    originalUpdate(executor, method);
}

inline bool Initialize() {
    auto executor = BNM::Class("PvZCards.Core.AI.AiActions", "AiSimulateAndScoreActionExecutor");
    const bool scorer=Core::Hooks::Install(executor.GetMethod("OnUpdate",0),Update,originalUpdate);
    const bool target=Core::Hooks::Install(BNM::Class("PvZCards.Core.AI.AiActions","AiSelectTargetActionExecutor").GetMethod("SelectTarget",0),SelectTarget,originalSelectTarget);
    const bool simulation=Core::Hooks::Install(BNM::Class("PvZCards.Core.AI.AiActions","AiSimulateActionExecutor").GetMethod("OnUpdate",0),SimulateUpdate,originalSimulateUpdate);
    const bool copy=Core::Hooks::Install(BNM::Class("PvZCards.Core.Utilities","GameStateCopier").GetMethod("CopyGameStateTo",1),CopyGameState,originalCopyGameState);
    LOG_INFO("CustomComponents/AI/PROFILE_INIT: scorer=%d target=%d simulate=%d copy=%d",int(scorer),int(target),int(simulation),int(copy));
    const bool endTurn=Core::Hooks::Install(BNM::Class("PvZCards.Core.Services","LocalEndTurnService")
        .GetMethod("Process",1),EndTurn,originalEndTurn);
    return scorer && target && simulation && copy && endTurn;
}
}
