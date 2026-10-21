#pragma once

#include <BNM/BasicMonoStructures.hpp>
#include <BNM/Class.hpp>
#include <BNM/ClassesManagement.hpp>
#include <BNM/Defaults.hpp>
#include <BNM/Il2CppHeaders.hpp>
#include <BNM/Method.hpp>

namespace CustomComponents::Components {

    // Global creator-defined trigger emitter.
    //
    // JSON:
    // {
    //   "$type":"PvZCards.Engine.Components.FireCustomTriggerEffectDescriptor, EngineLib, Version=1.0.0.0, Culture=neutral, PublicKeyToken=null",
    //   "$data":{"TriggerName":"Whatever"}
    // }
    //
    // Unlike FireTriggerEffectDescriptor, this is a broadcast. Every
    // CustomTrigger whose TriggerName matches participates, subject to the
    // ordinary stock trigger-condition pipeline.
    //
    // It derives from stock ReturnToHandFromPlayEffectDescriptor so the engine
    // uses an existing, proven EffectDescriptor virtual slot. CustomTriggerSystem
    // hooks that stock ToEffect and substitutes our StateCheckEffect carrier.
    struct FireCustomTriggerEffectDescriptor : BNM::IL2CPP::Il2CppObject {
        BNM::Structures::Mono::String* TriggerName{};

        BNM_CustomClass(
            FireCustomTriggerEffectDescriptor,
            BNM::CompileTimeClassBuilder(
                "PvZCards.Engine.Components",
                "FireCustomTriggerEffectDescriptor",
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
                reinterpret_cast<FireCustomTriggerEffectDescriptor*>(source);

            BNM_CallCustomMethodOrigin(
                DeepCopyFrom,
                this,
                source,
                instanceManager
            );

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
