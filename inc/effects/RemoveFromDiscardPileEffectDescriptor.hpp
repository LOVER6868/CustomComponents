#pragma once

#include <BNM/Class.hpp>
#include <BNM/ClassesManagement.hpp>
#include <BNM/Field.hpp>
#include <BNM/Il2CppHeaders.hpp>
#include <BNM/Method.hpp>

namespace CustomComponents::Components {

    struct RemoveFromDiscardPileEffectDescriptor : BNM::IL2CPP::Il2CppObject {
        BNM_CustomClass(
            RemoveFromDiscardPileEffectDescriptor,
            BNM::CompileTimeClassBuilder(
                "PvZCards.Engine.Components",
                "RemoveFromDiscardPileEffectDescriptor",
                "EngineLib.dll"
            ).Build(),
            BNM::CompileTimeClassBuilder(
                "PvZCards.Engine.Components",
                "ReturnCardToPlayersDeckEffectDescriptor",
                "EngineLib.dll"
            ).Build(),
            {}
        );

        using Obj = BNM::IL2CPP::Il2CppObject;

        // Outside the stock DeckPosition enum (0..2). Used only to route our
        // custom runtime through ReturnCardToPlayersDeckSystem without invoking
        // any deck behaviour.
        static constexpr int kRemoveFromDiscardSentinel = 0x6F000001;

        static Obj* CreateRuntime(Obj* instances) {
            auto instanceManager = BNM::Class("PvZCards.Engine", "InstanceManager");
            auto runtime = BNM::Class(
                "PvZCards.Engine.Effects",
                "ReturnCardToPlayersDeckEffect"
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
            auto deckPosition = runtime.GetField("DeckPosition").cast<int>();

            if (!acquire.IsValid() || !deckPosition.IsValid())
                return nullptr;

            Obj* effect = acquire[instances](
                reinterpret_cast<Obj*>(runtime.GetMonoType()),
                true
            );
            if (effect)
                deckPosition[effect] = kRemoveFromDiscardSentinel;
            return effect;
        }
    };

} // namespace CustomComponents::Components
