#pragma once

#include <BNM/Class.hpp>
#include <BNM/ClassesManagement.hpp>
#include <BNM/Field.hpp>
#include <BNM/Il2CppHeaders.hpp>
#include <BNM/Method.hpp>

namespace CustomComponents::Components {

    struct ReturnFromDiscardPileToLaneEffectDescriptor : BNM::IL2CPP::Il2CppObject {
        BNM_CustomClass(
            ReturnFromDiscardPileToLaneEffectDescriptor,
            BNM::CompileTimeClassBuilder(
                "PvZCards.Engine.Components",
                "ReturnFromDiscardPileToLaneEffectDescriptor",
                "EngineLib.dll"
            ).Build(),
            BNM::CompileTimeClassBuilder(
                "PvZCards.Engine.Components",
                "MoveCardToLanesEffectDescriptor",
                "EngineLib.dll"
            ).Build(),
            {}
        );

        using Obj = BNM::IL2CPP::Il2CppObject;

        // MoveCardToLanesEffect normally stores a small lane-order value here.
        // We use an impossible/high value only as a routing tag. The actual lane is
        // supplied natively through Effect.SecondaryTargetId, exactly like stock
        // secondary-target lane selection.
        static constexpr int kReturnFromDiscardToLaneSentinel = 0x6F000003;

        static Obj* CreateRuntime(Obj* instances) {
            auto instanceManager = BNM::Class("PvZCards.Engine", "InstanceManager");
            auto runtime = BNM::Class(
                "PvZCards.Engine.Effects",
                "MoveCardToLanesEffect"
            );
            auto systemType = BNM::Class("System", "Type");
            auto boolType = BNM::Class("System", "Boolean");

            if (!instances || !instanceManager.IsValid() || !runtime.IsValid() ||
                !systemType.IsValid() || !boolType.IsValid()) {
                return nullptr;
            }

            auto acquire = instanceManager.GetMethod(
                "Acquire",
                {systemType.GetCompileTimeClass(), boolType.GetCompileTimeClass()}
            ).cast<Obj*>();
            auto orderInLane = runtime.GetField("OrderInLane").cast<int>();

            if (!acquire.IsValid() || !orderInLane.IsValid())
                return nullptr;

            Obj* effect = acquire[instances](
                reinterpret_cast<Obj*>(runtime.GetMonoType()),
                true
            );
            if (effect)
                orderInLane[effect] = kReturnFromDiscardToLaneSentinel;
            return effect;
        }
    };

} // namespace CustomComponents::Components
