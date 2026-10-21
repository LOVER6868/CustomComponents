#pragma once

#include <BNM/Class.hpp>
#include <BNM/ClassesManagement.hpp>
#include <BNM/Defaults.hpp>
#include <BNM/Field.hpp>
#include <BNM/Il2CppHeaders.hpp>
#include <BNM/Method.hpp>

namespace CustomComponents::Components {

    // JSON-facing descriptor for discard-to-hand return routing.
    //
    // Runtime transport: stock ReturnCardToPlayersDeckEffect.
    // DeckPosition 0/1/2 are stock Top/Bottom/Random. We reserve 99 as a
    // private marker and intercept it before the stock system processes it.
    struct ReturnFromDiscardPileEffectDescriptor : BNM::IL2CPP::Il2CppObject {
        BNM_CustomClass(
            ReturnFromDiscardPileEffectDescriptor,
            BNM::CompileTimeClassBuilder(
                "PvZCards.Engine.Components",
                "ReturnFromDiscardPileEffectDescriptor",
                "EngineLib.dll"
            ).Build(),
            BNM::CompileTimeClassBuilder(
                "PvZCards.Engine.Components",
                "ReturnCardToPlayersDeckEffectDescriptor",
                "EngineLib.dll"
            ).Build(),
            {}
        );

        static constexpr int kReturnFromDiscardSentinel = 99;

        static BNM::IL2CPP::Il2CppObject* CreateRuntime(
            BNM::IL2CPP::Il2CppObject* instances
        ) {
            if (!instances)
                return nullptr;

            auto runtimeClass = BNM::Class(
                "PvZCards.Engine.Effects",
                "ReturnCardToPlayersDeckEffect"
            );
            auto instanceManager = BNM::Class(
                "PvZCards.Engine",
                "InstanceManager"
            );
            if (!runtimeClass.IsValid() || !instanceManager.IsValid())
                return nullptr;

            auto acquire = instanceManager.GetMethod("Acquire", 2).cast<void*>();
            auto deckPosition = runtimeClass.GetField("DeckPosition").cast<int>();
            if (!acquire.IsValid() || !deckPosition.IsValid())
                return nullptr;

            void* runtime = acquire[instances](runtimeClass.GetMonoType(), true);
            if (!runtime)
                return nullptr;

            deckPosition[runtime] = kReturnFromDiscardSentinel;
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
                "PvZCards.Engine", "Effect", "EngineLib.dll"
            ).Build(),
            "ToEffect",
            BNM::CompileTimeClassBuilder(
                "PvZCards.Engine", "InstanceManager", "EngineLib.dll"
            ).Build(),
            BNM::CompileTimeClassBuilder(
                "PvZCards.Engine", "Entity", "EngineLib.dll"
            ).Build()
        );
    };

} // namespace CustomComponents::Components
