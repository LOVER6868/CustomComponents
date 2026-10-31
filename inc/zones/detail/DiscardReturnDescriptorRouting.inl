#include "diagnostics/DiagnosticLogging.hpp"

    static Obj* CreateDeckDescriptorToEffectHook(
        void* self,
        Obj* instances,
        Obj* effectEntity,
        const void* method
    ) {
        auto* descriptor = static_cast<Obj*>(self);
        if (TypeName(descriptor) != "ReturnFromDiscardPileToDeckEffectDescriptor") {
            return originalCreateDeckDescriptorToEffect(self, instances, effectEntity, method);
        }

        const int position = createDeckDescriptorPosition[descriptor].Get();
        Obj* runtime = Components::ReturnFromDiscardPileToDeckEffectDescriptor::CreateRuntime(
            instances,
            position
        );
        CC_DIAG(
            "CustomComponents/ReturnFromDiscardPileToDeck: descriptor -> runtime=%p DeckPosition=%d",
            runtime,
            position
        );
        return runtime;
    }

    static Obj* MoveLaneDescriptorToEffectHook(
        void* self,
        Obj* instances,
        Obj* effectEntity,
        const void* method
    ) {
        if constexpr (false) CC_DIAG("CustomComponents/LANE-DIAG: DTE ENTER self=%p instances=%p effectEntity=%p method=%p", self, instances, effectEntity, method);
        auto* descriptor = static_cast<Obj*>(self);
        if constexpr (false) CC_DIAG("CustomComponents/LANE-DIAG: DTE before TypeName descriptor=%p", descriptor);
        const std::string descriptorType = TypeName(descriptor);
        if constexpr (false) CC_DIAG("CustomComponents/LANE-DIAG: DTE type=%s", descriptorType.c_str());
        if (descriptorType != "ReturnFromDiscardPileToLaneEffectDescriptor") {
            if constexpr (false) CC_DIAG("CustomComponents/LANE-DIAG: DTE stock delegate ENTER");
            Obj* stock = originalMoveLaneDescriptorToEffect(self, instances, effectEntity, method);
            if constexpr (false) CC_DIAG("CustomComponents/LANE-DIAG: DTE stock delegate EXIT runtime=%p", stock);
            return stock;
        }
        if constexpr (false) CC_DIAG("CustomComponents/LANE-DIAG: DTE custom CreateRuntime ENTER");

        LOG_INFO("CustomComponents/Pick3/UNEARCH_EFFECT_HANDOFF: phase=DESCRIPTOR_TO_EFFECT descriptor=%p effectEntity=%p instances=%p",
                 (void*)descriptor, (void*)effectEntity, (void*)instances);
        Obj* runtime = Components::ReturnFromDiscardPileToLaneEffectDescriptor::CreateRuntime(instances);
        // Show whether the effect was instantiated with a real discarded-card
        // target or still carries the temporary trick-play lane. This is
        // intentionally observational; the actual engine owns both fields.
        const int primary = runtime && effectTargetId.IsValid() ? effectTargetId[runtime].Get() : -1;
        const int secondary = runtime && effectSecondaryTargetId.IsValid() ? effectSecondaryTargetId[runtime].Get() : -1;
        LOG_INFO("CustomComponents/Pick3/UNEARCH_EFFECT_HANDOFF: phase=RUNTIME_CREATED runtime=%p primary=%d secondary=%d",
                 (void*)runtime, primary, secondary);
        if constexpr (false) CC_DIAG("CustomComponents/LANE-DIAG: DTE custom CreateRuntime EXIT runtime=%p", runtime);
        CC_DIAG(
            "CustomComponents/ReturnFromDiscardPileToLane: descriptor -> runtime=%p (lane supplied by SecondaryTargetId)",
            runtime
        );
        return runtime;
    }

