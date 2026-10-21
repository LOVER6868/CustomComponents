#include "diagnostics/DiagnosticLogging.hpp"
    // =====================================================================
    // Descriptor hooks
    // =====================================================================

    static Obj* ReturnDescriptorToEffectHook(
        void* self,
        Obj* instances,
        Obj* effectEntity,
        const void* method
    ) {
        auto* descriptor = static_cast<Obj*>(self);
        const std::string typeName = TypeName(descriptor);

        if (typeName == "ReturnFromDiscardPileEffectDescriptor") {
            Obj* runtime = Components::ReturnFromDiscardPileEffectDescriptor::CreateRuntime(instances);
            CC_DIAG(
                "CustomComponents/ReturnFromDiscardPile: descriptor -> runtime=%p",
                runtime
            );
            return runtime;
        }

        if (typeName == "RemoveFromDiscardPileEffectDescriptor") {
            Obj* runtime = Components::RemoveFromDiscardPileEffectDescriptor::CreateRuntime(instances);
            CC_DIAG(
                "CustomComponents/RemoveFromDiscardPile: descriptor -> runtime=%p",
                runtime
            );
            return runtime;
        }

        return originalReturnDescriptorToEffect(self, instances, effectEntity, method);
    }
