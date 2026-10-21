#pragma once

#include <BNM/Class.hpp>
#include <BNM/ClassesManagement.hpp>
#include <BNM/Defaults.hpp>
#include <BNM/Field.hpp>
#include <BNM/Il2CppHeaders.hpp>
#include <BNM/Method.hpp>

#include "effects/DrawCardFromSubsetInDeckEffect.hpp"

namespace CustomComponents::Components {

    // PvZCards.Engine.Components.DrawCardFromSubsetInDeckEffectDescriptor
    //     : DrawCardFromSubsetEffectDescriptor
    //
    // Inherited card_data fields:
    //     EntityQuery SubsetQuery;
    //     int DrawAmount;
    //
    // New field:
    //     bool PluckFailSafe;
    //
    // false: no matching card in the real deck -> draw nothing.
    // true : no matching card in the real deck -> run the ordinary stock
    //        DrawCardFromSubset behavior (conjure from the subset).
    struct DrawCardFromSubsetInDeckEffectDescriptor : BNM::IL2CPP::Il2CppObject {
        // DrawCardFromSubsetEffectDescriptor ends at 0x20 on this build.
        // Preserve the inherited 0x10 bytes after the Il2CppObject header.
        unsigned char _baseStorage[0x10]{};
        bool PluckFailSafe{};

        BNM_CustomClass(
            DrawCardFromSubsetInDeckEffectDescriptor,
            BNM::CompileTimeClassBuilder(
                "PvZCards.Engine.Components",
                "DrawCardFromSubsetInDeckEffectDescriptor",
                "EngineLib.dll"
            ).Build(),
            BNM::CompileTimeClassBuilder(
                "PvZCards.Engine.Components",
                "DrawCardFromSubsetEffectDescriptor",
                "EngineLib.dll"
            ).Build(),
            {}
        );

        BNM_CustomField(
            PluckFailSafe,
            BNM::Defaults::Get<bool>(),
            "PluckFailSafe"
        );

        BNM::IL2CPP::Il2CppObject* ToEffect(
            BNM::IL2CPP::Il2CppObject* instances,
            BNM::IL2CPP::Il2CppObject* effectEntity
        ) {
            if (!instances)
                return nullptr;

            auto descriptorClass = BNM::Class(
                "PvZCards.Engine.Components",
                "DrawCardFromSubsetEffectDescriptor"
            );
            auto runtimeClass = BNM::Class(
                "PvZCards.Engine.Effects",
                "DrawCardFromSubsetInDeckEffect"
            );
            auto instanceManager = BNM::Class(
                "PvZCards.Engine",
                "InstanceManager"
            );
            auto queryUtils = BNM::Class(
                "PvZCards.Engine",
                "EntityQueryUtils"
            );
            auto baseRuntime = BNM::Class(
                "PvZCards.Engine.Effects",
                "DrawCardFromSubsetAbilityEffect"
            );

            if (!descriptorClass.IsValid() || !runtimeClass.IsValid() ||
                !instanceManager.IsValid() || !queryUtils.IsValid() ||
                !baseRuntime.IsValid()) {
                return nullptr;
            }

            auto descriptorSubset = descriptorClass.GetField("SubsetQuery");
            auto descriptorAmount = descriptorClass.GetField("DrawAmount");
            auto runtimeSubset = baseRuntime.GetField("SubsetQuery");
            auto runtimeAmount = baseRuntime.GetField("DrawAmount");
            auto acquire = instanceManager.GetMethod("Acquire", 2).cast<void*>();
            auto deepCopyQuery = queryUtils.GetMethod("DeepCopy", 2).cast<void*>();

            if (!descriptorSubset.IsValid() || !descriptorAmount.IsValid() ||
                !runtimeSubset.IsValid() || !runtimeAmount.IsValid() ||
                !acquire.IsValid() || !deepCopyQuery.IsValid()) {
                return nullptr;
            }

            void* runtime = acquire[instances](runtimeClass.GetMonoType(), true);
            if (!runtime)
                return nullptr;

            void* query =
                descriptorSubset.cast<void*>()[
                    static_cast<BNM::IL2CPP::Il2CppObject*>(this)
                ];
            void* queryCopy = query ? deepCopyQuery(query, instances) : nullptr;

            runtimeSubset.cast<void*>()[runtime] = queryCopy;
            runtimeAmount.cast<int>()[runtime] =
                descriptorAmount.cast<int>()[
                    static_cast<BNM::IL2CPP::Il2CppObject*>(this)
                ];
            reinterpret_cast<Effects::DrawCardFromSubsetInDeckEffect*>(runtime)
                ->PluckFailSafe = PluckFailSafe;

            return static_cast<BNM::IL2CPP::Il2CppObject*>(runtime);
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

            auto* from = reinterpret_cast<DrawCardFromSubsetInDeckEffectDescriptor*>(source);
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

} // namespace CustomComponents::Components
