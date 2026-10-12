#pragma once

#include <BNM/BasicMonoStructures.hpp>
#include <BNM/Class.hpp>
#include <BNM/ClassesManagement.hpp>
#include <BNM/Defaults.hpp>
#include <BNM/Field.hpp>
#include <BNM/Il2CppHeaders.hpp>
#include <BNM/Method.hpp>

#include "log.h"

namespace CustomComponents::Components {

    // Targeted trigger activator.
    //
    // JSON:
    // {
    //   "$type":"PvZCards.Engine.Components.FireTriggerEffectDescriptor, EngineLib, Version=1.0.0.0, Culture=neutral, PublicKeyToken=null",
    //   "$data":{
    //     "TriggerName":"PlayTrigger",
    //     "IgnoreTriggerConditions":false
    //   }
    // }
    //
    // Primary targeting supplies Effect.TargetId. At runtime CustomTriggerSystem
    // activates only trigger effect entities whose EffectEntity.SourceId equals
    // that TargetId.
    //
    // This class derives from stock ReturnToHandFromPlayEffectDescriptor so it
    // inherits a proven EffectDescriptor virtual slot. CustomTriggerSystem hooks
    // the stock ToEffect implementation and substitutes a StateCheckEffect
    // carrier for this derived type, then StateCheckSystem dispatches the
    // targeted trigger.
    struct FireTriggerEffectDescriptor : BNM::IL2CPP::Il2CppObject {
        BNM::Structures::Mono::String* TriggerName{};
        bool IgnoreTriggerConditions{};
        BNM_CustomClass(
            FireTriggerEffectDescriptor,
            BNM::CompileTimeClassBuilder(
                "PvZCards.Engine.Components",
                "FireTriggerEffectDescriptor",
                "EngineLib.dll"
            ).Build(),
            BNM::CompileTimeClassBuilder(
                "PvZCards.Engine.Components",
                "ReturnToHandFromPlayEffectDescriptor",
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
        static BNM::IL2CPP::Il2CppObject* CreateRuntime(
            BNM::IL2CPP::Il2CppObject* instances
        ) {
            if (!instances)
                return nullptr;

            auto runtimeClass = BNM::Class(
                "PvZCards.Engine.Effects",
                "StateCheckEffect"
            );
            auto instanceManager = BNM::Class(
                "PvZCards.Engine",
                "InstanceManager"
            );

            if (!runtimeClass.IsValid() || !instanceManager.IsValid())
                return nullptr;

            auto acquire =
                instanceManager.GetMethod("Acquire", 2).cast<void*>();
            if (!acquire.IsValid())
                return nullptr;

            void* runtime = acquire[instances](
                runtimeClass.GetMonoType(),
                true
            );
            return static_cast<BNM::IL2CPP::Il2CppObject*>(runtime);
        }

        // ToEffect is intentionally inherited from the stock
        // ReturnToHandFromPlayEffectDescriptor. CustomTriggerSystem hooks that
        // stock virtual and substitutes our StateCheckEffect carrier only for
        // these two derived custom descriptor types.

        void DeepCopyFrom(
            BNM::IL2CPP::Il2CppObject* source,
            BNM::IL2CPP::Il2CppObject* instanceManager
        ) {
            auto* other =
                reinterpret_cast<FireTriggerEffectDescriptor*>(source);

            BNM_CallCustomMethodOrigin(
                DeepCopyFrom,
                this,
                source,
                instanceManager
            );

            TriggerName = other ? other->TriggerName : nullptr;
            IgnoreTriggerConditions =
                other ? other->IgnoreTriggerConditions : false;
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
