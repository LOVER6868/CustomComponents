#pragma once

#include <BNM/Class.hpp>
#include <BNM/ClassesManagement.hpp>
#include <BNM/Field.hpp>
#include <BNM/Il2CppHeaders.hpp>
#include <BNM/Method.hpp>

namespace CustomComponents::Components {

    struct ReturnFromDiscardPileToDeckEffectDescriptor : BNM::IL2CPP::Il2CppObject {
        BNM_CustomClass(
            ReturnFromDiscardPileToDeckEffectDescriptor,
            BNM::CompileTimeClassBuilder(
                "PvZCards.Engine.Components",
                "ReturnFromDiscardPileToDeckEffectDescriptor",
                "EngineLib.dll"
            ).Build(),
            BNM::CompileTimeClassBuilder(
                "PvZCards.Engine.Components",
                "CreateCardInDeckEffectDescriptor",
                "EngineLib.dll"
            ).Build(),
            {}
        );

        using Obj = BNM::IL2CPP::Il2CppObject;

        // CreateCardInDeckEffect.CardGuid can never legitimately be this value in
        // this mod. It marks a runtime that means "move existing target entity from
        // discard to deck" rather than "manufacture CardGuid".
        static constexpr int kExistingEntityDeckSentinel = -2000000001;

        static Obj* CreateRuntime(Obj* instances, int deckPositionValue) {
            auto instanceManager = BNM::Class("PvZCards.Engine", "InstanceManager");
            auto runtime = BNM::Class(
                "PvZCards.Engine.Effects",
                "CreateCardInDeckEffect"
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
            auto cardGuid = runtime.GetField("CardGuid").cast<int>();
            auto amount = runtime.GetField("AmountToCreate").cast<int>();
            auto deckPosition = runtime.GetField("DeckPosition").cast<int>();

            if (!acquire.IsValid() || !cardGuid.IsValid() || !amount.IsValid() ||
                !deckPosition.IsValid()) {
                return nullptr;
            }

            Obj* effect = acquire[instances](
                reinterpret_cast<Obj*>(runtime.GetMonoType()),
                true
            );
            if (effect) {
                cardGuid[effect] = kExistingEntityDeckSentinel;
                amount[effect] = 0;
                deckPosition[effect] = deckPositionValue;
            }
            return effect;
        }
    };

} // namespace CustomComponents::Components
