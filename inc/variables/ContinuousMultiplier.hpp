#pragma once
#include "variables/Multiplier.hpp"
#include "core/NativeConstruction.hpp"

namespace CustomComponents::Variables::ContinuousMultipliers {
// This state is part of the engine model, so AI copies retain the multiplier
// that produced their active modifiers. A process-global cache cannot do that.
struct State : Obj {
    int Value = 0;
    bool Initialized = false;
    BNM_CustomClass(State,
        BNM::CompileTimeClassBuilder("PvZCards.Engine.Components", "ContinuousMultiplierState", "EngineLib.dll").Build(),
        BNM::CompileTimeClassBuilder("PvZCards.Engine.Components", "MarkerComponent", "EngineLib.dll").Build(), {});
    BNM_CustomField(Value, BNM::Defaults::Get<int>(), "Value");
    BNM_CustomField(Initialized, BNM::Defaults::Get<bool>(), "Initialized");
    void Construct(){Core::NativeConstruction::Base(this);}
    BNM_CustomMethod(Construct,false,BNM::Defaults::Get<void>(),".ctor");
    void DeepCopyFrom(Obj* source, Obj*) {
        const auto* state = reinterpret_cast<State*>(source);
        Value = state ? state->Value : 0;
        Initialized = state && state->Initialized;
    }
    void HandleReturnToPool(Obj*) { Value = 0; Initialized = false; }
    BNM_CustomMethod(DeepCopyFrom, false, BNM::Defaults::Get<void>(), "DeepCopyFrom",
        BNM::Defaults::Get<Obj*>(), BNM::CompileTimeClassBuilder("PvZCards.Engine", "InstanceManager", "EngineLib.dll").Build());
    BNM_CustomMethod(HandleReturnToPool, false, BNM::Defaults::Get<void>(), "HandleReturnToPool",
        BNM::CompileTimeClassBuilder("PvZCards.Engine", "InstanceManager", "EngineLib.dll").Build());
};
inline void (*originalCheck)(Obj*, Obj*, Obj*, const void*) = nullptr;
inline BNM::Field<Obj*> remover;
inline BNM::Method<void> removeActive;
static void Check(Obj* system, Obj* entity, Obj* context, const void* method) {
    if (entity && context && (Engine::EntityAccess::Component(entity, "VariableMultiplier") ||
                            Engine::EntityAccess::Component(entity, "LevelSpentMultiplier"))) {
        Context evaluation{ContextModel(context), Source(entity), ContextId(context, "get_TargetId"),
            Engine::Match::Id(entity), nullptr, context};
        const auto factor = BindingFactor(evaluation, entity);
        const auto type = BNM::Class("PvZCards.Engine.Components", "ContinuousMultiplierState");
        auto* state = reinterpret_cast<State*>(Engine::Match::Component(entity, type.GetMonoType(), true));
        if (state) {
            const int value = factor.value_or(0);
            if (state->Initialized && state->Value != value) {
                Obj* service = Core::Metadata::Bind(remover, system).Get();
                if (service) {
                    // Stock removes source-owned modifiers and clears ActiveTargets.
                    // The normal check then reapplies with the new multiplier.
                    Core::Metadata::Bind(removeActive, service)(entity);
                    LOG_INFO("CustomComponents/ContinuousMultiplier: effect=%d old=%d new=%d",
                        evaluation.effect, state->Value, value);
                }
            }
            state->Value = value;
            state->Initialized = true;
        }
    }
    originalCheck(system, entity, context, method);
}
inline bool Initialize() {
    const auto type = BNM::Class("PvZCards.Engine.Components", "ContinuousMultiplierState");
    const auto system = BNM::Class("PvZCards.Core.Systems", "ContinuousEffectSystem");
    remover = system.GetField("<EffectRemover>k__BackingField").cast<Obj*>();
    removeActive = BNM::Class("PvZCards.Core.Systems.Utilities", "ContinuousEffectRemover").GetMethod("RemoveActiveEffects", 1).cast<void>();
    return type.IsValid() && remover.IsValid() && removeActive.IsValid() &&
        CustomComponentRegistry::Register({"ContinuousMultiplierState", "PvZCards.Engine.Components", "EngineLib.dll",
            "MarkerComponent", "PvZCards.Engine.Components", "EngineLib.dll"}) &&
        Core::ComponentLifecycle::Patch(type, BNM::Class("PvZCards.Engine.Components", "MarkerComponent")) &&
        Core::Hooks::Install(system.GetMethod("CheckContinuousEffect", 2), Check, originalCheck);
}
}
