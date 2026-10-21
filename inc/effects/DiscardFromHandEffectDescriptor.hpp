#pragma once

#include <BNM/Class.hpp>
#include <BNM/ClassesManagement.hpp>
#include <BNM/Defaults.hpp>
#include <BNM/Field.hpp>
#include <BNM/Il2CppHeaders.hpp>
#include <BNM/Method.hpp>

namespace CustomComponents::Components {

    // Gameplay-only bridge: use stock MoveToDiscardPileEffect.
    // DiscardPileId=-1 is our sentinel, resolved from the actual target owner
    // by the existing MoveToDiscardPileSystem.ProcessEffect hook.
    struct DiscardFromHandEffectDescriptor : BNM::IL2CPP::Il2CppObject {
        BNM_CustomClass(
            DiscardFromHandEffectDescriptor,
            BNM::CompileTimeClassBuilder(
                "PvZCards.Engine.Components",
                "DiscardFromHandEffectDescriptor",
                "EngineLib.dll"
            ).Build(),
            BNM::CompileTimeClassBuilder(
                "PvZCards.Engine.Components",
                "DestroyCardEffectDescriptor",
                "EngineLib.dll"
            ).Build(),
            {}
        );

        static BNM::IL2CPP::Il2CppObject* CreateRuntime(
            BNM::IL2CPP::Il2CppObject* instances
        ) {
            if (!instances)
                return nullptr;

            auto runtimeClass = BNM::Class(
                "PvZCards.Engine.Effects",
                "MoveToDiscardPileEffect"
            );
            auto instanceManager = BNM::Class(
                "PvZCards.Engine",
                "InstanceManager"
            );
            if (!runtimeClass.IsValid() || !instanceManager.IsValid())
                return nullptr;

            auto acquire = instanceManager.GetMethod("Acquire", 2).cast<void*>();
            auto discardPileId = runtimeClass.GetField("DiscardPileId").cast<int>();
            if (!acquire.IsValid() || !discardPileId.IsValid())
                return nullptr;

            void* runtime = acquire[instances](runtimeClass.GetMonoType(), true);
            if (!runtime)
                return nullptr;

            // The stock system never receives this sentinel; the owning
            // existing zone-effects hook resolves the target's discard pile.
            discardPileId[runtime] = -1;
            return static_cast<BNM::IL2CPP::Il2CppObject*>(runtime);
        }

        BNM::IL2CPP::Il2CppObject* ToEffect(
            BNM::IL2CPP::Il2CppObject* instances,
            BNM::IL2CPP::Il2CppObject* effectEntity
        ) {
            (void)effectEntity;
            return CreateRuntime(instances);
        }

        BNM_CustomMethod(
            ToEffect,
            false,
            BNM::CompileTimeClassBuilder(
                "PvZCards.Engine",
                "Effect",
                "EngineLib.dll"
            ).Build(),
            "ToEffect",
            BNM::CompileTimeClassBuilder(
                "PvZCards.Engine",
                "InstanceManager",
                "EngineLib.dll"
            ).Build(),
            BNM::CompileTimeClassBuilder(
                "PvZCards.Engine",
                "Entity",
                "EngineLib.dll"
            ).Build()
        );
    };

} // namespace CustomComponents::Components
