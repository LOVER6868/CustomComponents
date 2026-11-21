#pragma once

#include <BNM/BasicMonoStructures.hpp>
#include <BNM/Class.hpp>
#include <BNM/ClassesManagement.hpp>
#include <BNM/Defaults.hpp>
#include <BNM/Field.hpp>
#include <BNM/Il2CppHeaders.hpp>
#include <BNM/Method.hpp>
#include "core/ManagedRuntime.hpp"

namespace CustomComponents::Components {

    struct ReturnFromDiscardPileToLaneEffectDescriptor : BNM::IL2CPP::Il2CppObject {
        bool ForceFaceDown{};
        BNM::Structures::Mono::String* CreationPosition{};
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

        BNM_CustomField(
            ForceFaceDown,
            BNM::Defaults::Get<bool>(),
            "ForceFaceDown"
        );
        BNM_CustomField(CreationPosition,
            BNM::Defaults::Get<BNM::Structures::Mono::String*>(),
            "CreationPosition");

        using Obj = BNM::IL2CPP::Il2CppObject;

        void DeepCopyFrom(Obj* source, Obj* instances) {
            BNM_CallCustomMethodOrigin(DeepCopyFrom, this, source, instances);
            auto* other = static_cast<ReturnFromDiscardPileToLaneEffectDescriptor*>(source);
            ForceFaceDown = other && other->ForceFaceDown;
            Obj* position = other ? reinterpret_cast<Obj*>(other->CreationPosition) : nullptr;
            if (Core::ManagedRuntime::writeBarrier)
                Core::ManagedRuntime::writeBarrier(this, reinterpret_cast<void**>(&CreationPosition), position);
            else
                CreationPosition = reinterpret_cast<BNM::Structures::Mono::String*>(position);
        }
        BNM_CustomMethod(DeepCopyFrom, false, BNM::Defaults::Get<void>(), "DeepCopyFrom",
            BNM::Defaults::Get<Obj*>(),
            BNM::CompileTimeClassBuilder("PvZCards.Engine", "InstanceManager", "EngineLib.dll").Build());

        void HandleReturnToPool(Obj* instances) {
            ForceFaceDown = false;
            if (Core::ManagedRuntime::writeBarrier)
                Core::ManagedRuntime::writeBarrier(this, reinterpret_cast<void**>(&CreationPosition), nullptr);
            else CreationPosition = nullptr;
            BNM_CallCustomMethodOrigin(HandleReturnToPool, this, instances);
        }
        BNM_CustomMethod(HandleReturnToPool, false, BNM::Defaults::Get<void>(), "HandleReturnToPool",
            BNM::CompileTimeClassBuilder("PvZCards.Engine", "InstanceManager", "EngineLib.dll").Build());

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
