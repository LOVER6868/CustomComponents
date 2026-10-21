#pragma once

#include <BNM/BasicMonoStructures.hpp>
#include <BNM/ClassesManagement.hpp>
#include <BNM/Defaults.hpp>
#include <BNM/Il2CppHeaders.hpp>

namespace CustomComponents::Components {

    // JSON-facing targeted deck reorder descriptor:
    //   Mode: "Add" or "Move"
    //   Position: signed offset or absolute zero-based index (-1 = last).
    //
    // Inherit the stock ReturnCardToPlayersDeckEffectDescriptor virtual
    // ToEffect slot. The existing ReturnFromDiscardPileSystem hook dispatches
    // only this exact subclass to the deck-reorder runtime transport.
    struct MoveCardInDeckEffectDescriptor : BNM::IL2CPP::Il2CppObject {
        BNM::Structures::Mono::String* Mode{};
        int Position{};

        BNM_CustomClass(
            MoveCardInDeckEffectDescriptor,
            BNM::CompileTimeClassBuilder(
                "PvZCards.Engine.Components", "MoveCardInDeckEffectDescriptor", "EngineLib.dll"
            ).Build(),
            BNM::CompileTimeClassBuilder(
                "PvZCards.Engine.Components", "ReturnCardToPlayersDeckEffectDescriptor", "EngineLib.dll"
            ).Build(),
            {}
        );

        BNM_CustomField(Mode, BNM::Defaults::Get<BNM::Structures::Mono::String*>(), "Mode");
        BNM_CustomField(Position, BNM::Defaults::Get<int>(), "Position");

        void DeepCopyFrom(
            BNM::IL2CPP::Il2CppObject* source,
            BNM::IL2CPP::Il2CppObject* instances
        ) {
            auto* other = reinterpret_cast<MoveCardInDeckEffectDescriptor*>(source);
            BNM_CallCustomMethodOrigin(DeepCopyFrom, this, source, instances);
            Mode = other ? other->Mode : nullptr;
            Position = other ? other->Position : 0;
        }

        BNM_CustomMethod(
            DeepCopyFrom, false, BNM::Defaults::Get<void>(), "DeepCopyFrom",
            BNM::Defaults::Get<BNM::IL2CPP::Il2CppObject*>(),
            BNM::CompileTimeClassBuilder(
                "PvZCards.Engine", "InstanceManager", "EngineLib.dll"
            ).Build()
        );

        void HandleReturnToPool(BNM::IL2CPP::Il2CppObject* instances) {
            BNM_CallCustomMethodOrigin(HandleReturnToPool, this, instances);
            Mode = nullptr;
            Position = 0;
        }

        BNM_CustomMethod(
            HandleReturnToPool, false, BNM::Defaults::Get<void>(), "HandleReturnToPool",
            BNM::CompileTimeClassBuilder(
                "PvZCards.Engine", "InstanceManager", "EngineLib.dll"
            ).Build()
        );
    };

} // namespace CustomComponents::Components
