#pragma once

#include <BNM/BasicMonoStructures.hpp>
#include <BNM/ClassesManagement.hpp>
#include <BNM/Defaults.hpp>
#include <BNM/Il2CppHeaders.hpp>

namespace CustomComponents::Components {

    // Generic creator-defined trigger listener.
    //
    // JSON:
    // {
    //   "$type":"PvZCards.Engine.Components.CustomTrigger, EngineLib, Version=1.0.0.0, Culture=neutral, PublicKeyToken=null",
    //   "$data":{"TriggerName":"Whatever"}
    // }
    //
    // CustomTriggerSystem::FireCustom("Whatever", effect) dispatches this event.
    // Named public custom triggers also expose their component type name as an
    // event name, so TriggerName="DiscardOutsidePlayTrigger" can listen to the
    // same event as DiscardOutsidePlayTrigger.
    struct CustomTrigger : BNM::IL2CPP::Il2CppObject {
        BNM::Structures::Mono::String* TriggerName{};

        BNM_CustomClass(
            CustomTrigger,
            BNM::CompileTimeClassBuilder(
                "PvZCards.Engine.Components",
                "CustomTrigger",
                "EngineLib.dll"
            ).Build(),
            BNM::CompileTimeClassBuilder(
                "PvZCards.Engine.Components",
                "MarkerComponent",
                "EngineLib.dll"
            ).Build(),
            {},
            BNM::CompileTimeClassBuilder(
                "PvZCards.Engine",
                "Trigger",
                "EngineLib.dll"
            ).Build()
        );

        BNM_CustomField(
            TriggerName,
            BNM::Defaults::Get<BNM::Structures::Mono::String*>(),
            "TriggerName"
        );

        void DeepCopyFrom(
            BNM::IL2CPP::Il2CppObject* source,
            BNM::IL2CPP::Il2CppObject* instanceManager
        ) {
            auto* other = reinterpret_cast<CustomTrigger*>(source);

            BNM_CallCustomMethodOrigin(
                DeepCopyFrom,
                this,
                source,
                instanceManager
            );

            // System.String is immutable; retaining the managed reference is the
            // correct deep-copy behavior for this value.
            TriggerName = other ? other->TriggerName : nullptr;
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
