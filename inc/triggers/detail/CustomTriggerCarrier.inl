#include "diagnostics/DiagnosticLogging.hpp"
    static Obj* StockTriggerCarrierToEffectHook(
        void* self,
        Obj* instances,
        Obj* effectEntity,
        const void* method
    ) {
        Obj* descriptor = static_cast<Obj*>(self);
        const char* descriptorType = TypeName(descriptor);

        const bool targeted =
            descriptorType &&
            std::strcmp(descriptorType, "FireTriggerEffectDescriptor") == 0;
        const bool global =
            descriptorType &&
            std::strcmp(descriptorType, "FireCustomTriggerEffectDescriptor") == 0;

        if (!targeted && !global) {
            return originalStockTriggerCarrierToEffect(
                self,
                instances,
                effectEntity,
                method
            );
        }

        BNM::Structures::Mono::String* managedName = nullptr;
        bool ignoreTriggerConditions = false;
        if (targeted) {
            managedName = fireTriggerName[descriptor].Get();
            ignoreTriggerConditions =
                fireTriggerIgnoreTriggerConditions[descriptor].Get();
        } else {
            managedName = fireCustomTriggerName[descriptor].Get();
        }

        Obj* runtime = targeted
            ? Components::FireTriggerEffectDescriptor::CreateRuntime(instances)
            : Components::FireCustomTriggerEffectDescriptor::CreateRuntime(instances);

        if (!runtime) {
            LOG_ERR(
                "CustomComponents/CustomTrigger/Carrier: failed to create stock StateCheckEffect type=%s",
                descriptorType ? descriptorType : "<null>"
            );
            return nullptr;
        }

        const int sourceEffectEntityId =
            effectEntity && entityId.IsValid()
                ? entityId[effectEntity].Get()
                : -1;

        effectEffectEntityId[runtime] = sourceEffectEntityId;

        StoreCarrierMetadata(
            runtime,
            managedName,
            ignoreTriggerConditions,
            targeted
        );

        CC_DIAG(
            "CustomComponents/CustomTrigger/Carrier: CREATED stock type=%s runtime=%p TriggerNamePtr=%p TriggerName='%s' IgnoreTriggerConditions=%d EffectEntityId=%d",
            descriptorType ? descriptorType : "<null>",
            runtime,
            managedName,
            managedName ? managedName->str().c_str() : "<null>",
            ignoreTriggerConditions ? 1 : 0,
            sourceEffectEntityId
        );

        return runtime;
    }


    static void StateCheckProcessEffectHook(
        void* self,
        Obj* effect,
        const void* method
    ) {
        Obj* stateCheckInstances = GetStateCheckInstances(self);
        if (stateCheckInstances)
            activeStateCheckInstances = stateCheckInstances;

        if constexpr (CustomComponents::Diagnostics::kVerbose) CC_DIAG(
            "CustomComponents/CustomTrigger/StateCheck: ENTER self=%p effect=%p activeInstances=%p",
            self,
            effect,
            activeStateCheckInstances
        );

        if (!effect) {
            LOG_ERR(
                "CustomComponents/CustomTrigger/StateCheck: null effect; forwarding stock"
            );
            originalStateCheckProcessEffect(self, effect, method);
            return;
        }

        CarrierMetadata metadata{};
        if (TakeCarrierMetadata(effect, metadata)) {
            const std::string triggerName =
                metadata.triggerName
                    ? metadata.triggerName->str()
                    : std::string();

            CC_DIAG(
                "CustomComponents/CustomTrigger/StateCheck: CUSTOM-CARRIER TriggerName='%s' Targeted=%d IgnoreTriggerConditions=%d SourceId=%d TargetId=%d EffectEntityId=%d",
                triggerName.c_str(),
                metadata.targeted ? 1 : 0,
                metadata.ignoreTriggerConditions ? 1 : 0,
                effectSourceId[effect].Get(),
                effectTargetId[effect].Get(),
                effectEffectEntityId[effect].Get()
            );

            if (triggerName.empty()) {
                LOG_ERR(
                    "CustomComponents/CustomTrigger/StateCheck: custom carrier has empty TriggerName; swallowing"
                );
                return;
            }

            bool fired = false;
            if (metadata.targeted) {
                const int targetId = effectTargetId[effect].Get();
                if (targetId < 0) {
                    LOG_ERR(
                        "CustomComponents/CustomTrigger/StateCheck: targeted carrier missing TargetId"
                    );
                    return;
                }

                fired = FireForEntity(
                    triggerName.c_str(),
                    targetId,
                    effect,
                    metadata.ignoreTriggerConditions
                );
            } else {
                fired = FireCustom(triggerName.c_str(), effect);
            }

            CC_DIAG(
                "CustomComponents/CustomTrigger/StateCheck: custom carrier dispatch result=%d",
                fired ? 1 : 0
            );
            return;
        }

        // Not one of our metadata-tagged carrier effects. Leave stock
        // StateCheck processing completely untouched.
        originalStateCheckProcessEffect(self, effect, method);
        return;
    }

