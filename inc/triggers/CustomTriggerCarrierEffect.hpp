#pragma once

#include <BNM/BasicMonoStructures.hpp>
#include <BNM/ClassesManagement.hpp>
#include <BNM/Defaults.hpp>
#include <BNM/Il2CppHeaders.hpp>

namespace CustomComponents::Effects {

    // Managed runtime carrier for FireTriggerEffectDescriptor and
    // FireCustomTriggerEffectDescriptor.
    //
    // We deliberately do not depend on looking the descriptor back up from the
    // EffectEntity after ToEffect. The descriptor may have been cloned/pool-copied
    // and its custom fields can be lost. Instead, the stock ToEffect hook copies
    // the trigger metadata directly onto this runtime Effect.
    //
    // EffectRouter.ProcessEffect is hooked by CustomTriggerSystem and consumes
    // this exact runtime type before stock exact-Type routing.
    struct CustomTriggerCarrierEffect : BNM::IL2CPP::Il2CppObject {
        BNM::Structures::Mono::String* TriggerName{};
        bool IgnoreTriggerConditions{};
        bool IsTargeted{};

        BNM_CustomClass(
            CustomTriggerCarrierEffect,
            BNM::CompileTimeClassBuilder(
                "PvZCards.Engine.Effects",
                "CustomTriggerCarrierEffect",
                "EngineLib.dll"
            ).Build(),
            BNM::CompileTimeClassBuilder(
                "PvZCards.Engine.Effects",
                "StateCheckEffect",
                "EngineLib.dll"
            ).Build(),
            {}
        );

        BNM_CustomField(
            TriggerName,
            BNM::Defaults::Get<BNM::Structures::Mono::String*>(),
            "TriggerName"
        );

        BNM_CustomField(
            IgnoreTriggerConditions,
            BNM::Defaults::Get<bool>(),
            "IgnoreTriggerConditions"
        );

        BNM_CustomField(
            IsTargeted,
            BNM::Defaults::Get<bool>(),
            "IsTargeted"
        );

        void DeepCopyFrom(
            BNM::IL2CPP::Il2CppObject* source,
            BNM::IL2CPP::Il2CppObject* instanceManager
        ) {
            auto* other =
                reinterpret_cast<CustomTriggerCarrierEffect*>(source);

            BNM_CallCustomMethodOrigin(
                DeepCopyFrom,
                this,
                source,
                instanceManager
            );

            TriggerName = other ? other->TriggerName : nullptr;
            IgnoreTriggerConditions =
                other ? other->IgnoreTriggerConditions : false;
            IsTargeted = other ? other->IsTargeted : false;
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

            TriggerName = nullptr;
            IgnoreTriggerConditions = false;
            IsTargeted = false;
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
