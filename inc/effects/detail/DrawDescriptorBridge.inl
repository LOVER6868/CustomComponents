    // -------------------------------------------------------------------------
    // Focused custom-field deserialization + descriptor lifecycle bridge
    // -------------------------------------------------------------------------

    inline bool IsCustomDescriptor(Obj* object) {
        if (!object) return false;
        auto klass = BNM::Class(
            "PvZCards.Engine.Components",
            "DrawCardFromSubsetInDeckEffectDescriptor"
        );
        return klass.IsValid() && BNM::Class(object).GetClass() == klass.GetClass();
    }

    inline void PutDescriptorPolicy(Obj* descriptor, bool value) {
        if (!descriptor) return;
        std::lock_guard lock(descriptorPolicyMutex);
        descriptorPolicy[descriptor] = value;
    }

    inline bool GetDescriptorPolicy(Obj* descriptor, bool fallback = false) {
        if (!descriptor) return fallback;
        std::lock_guard lock(descriptorPolicyMutex);
        const auto it = descriptorPolicy.find(descriptor);
        return it == descriptorPolicy.end() ? fallback : it->second;
    }

    inline void EraseDescriptorPolicy(Obj* descriptor) {
        if (!descriptor) return;
        std::lock_guard lock(descriptorPolicyMutex);
        descriptorPolicy.erase(descriptor);
    }

    static void DescriptorCopyHook(
        void* self, Obj* source, Obj* instances, const void* method
    ) {
        const bool custom = IsCustomDescriptor(static_cast<Obj*>(self));
        const bool policy = custom && source
            ? GetDescriptorPolicy(source, descriptorFailSafe[source].Get())
            : false;

        originalDescriptorCopy(self, source, instances, method);

        if (custom) {
            auto* target = static_cast<Obj*>(self);
            descriptorFailSafe[target] = policy;
            PutDescriptorPolicy(target, policy);
        }
    }

    static void DescriptorPoolHook(void* self, Obj* instances, const void* method) {
        auto* descriptor = static_cast<Obj*>(self);
        const bool custom = IsCustomDescriptor(descriptor);
        if (custom)
            EraseDescriptorPolicy(descriptor);

        originalDescriptorPool(self, instances, method);

        if (custom)
            descriptorFailSafe[descriptor] = false;
    }

    // -------------------------------------------------------------------------
    // Descriptor -> stock runtime policy bridge
    //
    // ComponentSupport dispatches this custom descriptor through the inherited
    // stock ToEffect virtual. Keep the stock runtime (the lifecycle that advances
    // effect groups correctly), but remember which stock runtime came from us and
    // copy PluckFailSafe alongside it.
    // -------------------------------------------------------------------------

    static Obj* DescriptorToEffectHook(
        void* self,
        Obj* source,
        Obj* context,
        const void* method
    ) {
        Obj* runtime = originalDescriptorToEffect(self, source, context, method);

        if (self && runtime &&
            BNM::Class(static_cast<Obj*>(self)).GetClass() ==
                BNM::Class(
                    "PvZCards.Engine.Components",
                    "DrawCardFromSubsetInDeckEffectDescriptor"
                ).GetClass()) {
            auto* descriptor = static_cast<Obj*>(self);
            failSafeByAbilityEffect[runtime] =
                GetDescriptorPolicy(
                    descriptor,
                    descriptorFailSafe[descriptor].Get()
                );
        }

        return runtime;
    }

    // -------------------------------------------------------------------------
    // EffectRouter registration bridge
    //
    // DrawCardFromSubsetInDeckEffect is a NEW runtime Type. We do not replace the
    // stock type. When DrawCardAbilitySystem registers its stock handler for
    // DrawCardFromSubsetAbilityEffect, mirror that same handler onto our subtype.
    // -------------------------------------------------------------------------

    static void RegisterForEffectHook(
        void* self,
        void* effectType,
        void* handler,
        const void* method
    ) {
        // Preserve stock router registration exactly. The actual component path
        // produces DrawCardFromSubsetAbilityEffect, so no custom runtime handler
        // is injected here.
        originalRegisterForEffect(self, effectType, handler, method);
    }

    // The stock DrawCardAbilitySystem handler builds the ordinary child
    // DrawCardFromSubsetEffect(s). The descriptor bridge marks only stock runtime
    // objects originating from our custom component, so ordinary stock effects
    // remain completely unchanged.
    static void DrawSubsetAbilityHook(
        void* self,
        Obj* effect,
        const void* method
    ) {
        auto it = failSafeByAbilityEffect.find(effect);
        const bool bridgedCustom = it != failSafeByAbilityEffect.end();

        if (bridgedCustom) {
            PendingDraw pending;
            pending.sourceId = effectSource[effect].Get();
            pending.playerId = effectPlayer[effect].Get();
            pending.effectGroupId = effectGroup[effect].Get();
            pending.remaining = std::max(0, customDrawAmount[effect].Get());
            pending.pluckFailSafe = it->second;

            // Consume the bridge now. PendingDraw owns the policy for the children.
            failSafeByAbilityEffect.erase(it);

            if (pending.remaining > 0) {
                pendingDraws.push_back(pending);
            }
        }

        originalDrawSubsetAbility(self, effect, method);
    }

