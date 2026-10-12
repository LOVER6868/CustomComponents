#pragma once

#include <BNM/ClassesManagement.hpp>
#include <BNM/Defaults.hpp>
#include <BNM/Il2CppHeaders.hpp>

namespace CustomComponents::Effects {

    // PvZCards.Engine.Effects.DrawCardFromSubsetInDeckEffect
    //     : DrawCardFromSubsetAbilityEffect
    //
    // The stock base already owns:
    //     int DrawAmount;
    //     EntityQuery SubsetQuery;
    //
    // This distinct runtime effect adds only the tutor fallback policy.
    struct DrawCardFromSubsetInDeckEffect : BNM::IL2CPP::Il2CppObject {
        // DrawCardFromSubsetAbilityEffect ends at 0x58 on this game build.
        // Il2CppObject is 0x10, so preserve the inherited 0x48 bytes here.
        unsigned char _baseStorage[0x48]{};
        bool PluckFailSafe{};

        BNM_CustomClass(
            DrawCardFromSubsetInDeckEffect,
            BNM::CompileTimeClassBuilder(
                "PvZCards.Engine.Effects",
                "DrawCardFromSubsetInDeckEffect",
                "EngineLib.dll"
            ).Build(),
            BNM::CompileTimeClassBuilder(
                "PvZCards.Engine.Effects",
                "DrawCardFromSubsetAbilityEffect",
                "EngineLib.dll"
            ).Build(),
            {}
        );

        BNM_CustomField(
            PluckFailSafe,
            BNM::Defaults::Get<bool>(),
            "PluckFailSafe"
        );

        void DeepCopyFrom(
            BNM::IL2CPP::Il2CppObject* source,
            BNM::IL2CPP::Il2CppObject* instanceManager
        ) {
            BNM_CallCustomMethodOrigin(
                DeepCopyFrom,
                this,
                source,
                instanceManager
            );

            auto* from = reinterpret_cast<DrawCardFromSubsetInDeckEffect*>(source);
            PluckFailSafe = from && from->PluckFailSafe;
        }

        BNM_CustomMethod(
            DeepCopyFrom,
            false,
            BNM::Defaults::Get<void>(),
            "DeepCopyFrom",
            BNM::Defaults::Get<BNM::IL2CPP::Il2CppObject*>(),
            BNM::CompileTimeClassBuilder(
                "PvZCards.Engine",
                "InstanceManager",
                "EngineLib.dll"
            ).Build()
        );

        void HandleReturnToPool(
            BNM::IL2CPP::Il2CppObject* instanceManager
        ) {
            BNM_CallCustomMethodOrigin(
                HandleReturnToPool,
                this,
                instanceManager
            );
            PluckFailSafe = false;
        }

        BNM_CustomMethod(
            HandleReturnToPool,
            false,
            BNM::Defaults::Get<void>(),
            "HandleReturnToPool",
            BNM::CompileTimeClassBuilder(
                "PvZCards.Engine",
                "InstanceManager",
                "EngineLib.dll"
            ).Build()
        );
    };

} // namespace CustomComponents::Effects
