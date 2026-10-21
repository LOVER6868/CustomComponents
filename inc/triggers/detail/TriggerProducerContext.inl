#include "diagnostics/DiagnosticLogging.hpp"
    inline const char* TypeName(Obj* object) {
        if (!object)
            return "null";
        auto* klass = BNM::Class(object).GetClass();
        return klass && klass->name ? klass->name : "?";
    }

    inline Obj* FindNamedComponent(Obj* entity, const char* componentName) {
        if (!entity || !componentName)
            return nullptr;

        auto* components = entityComponents[entity].Get();
        if (!components || !components->items)
            return nullptr;

        for (int i = 0; i < components->size; ++i) {
            Obj* component = components->items->m_Items[i];
            if (component && std::strcmp(TypeName(component), componentName) == 0)
                return component;
        }
        return nullptr;
    }

    inline bool HasNamedComponent(Obj* entity, const char* componentName) {
        return FindNamedComponent(entity, componentName) != nullptr;
    }

    inline bool CustomTriggerMatches(Obj* entity, const char* eventName) {
        if (!entity || !eventName || !*eventName || !customTriggerName.IsValid())
            return false;

        Obj* component = FindNamedComponent(entity, "CustomTrigger");
        if (!component)
            return false;

        auto* managedName = customTriggerName[component].Get();
        return managedName && managedName->str() == eventName;
    }

    inline bool EntityListensForEvent(
        Obj* entity,
        const char* markerComponentName,
        const char* eventName
    ) {
        const bool markerMatch =
            markerComponentName &&
            *markerComponentName &&
            HasNamedComponent(entity, markerComponentName);

        return markerMatch || CustomTriggerMatches(entity, eventName);
    }

    inline Obj* ReadObjectFieldByOffset(Obj* object, std::uintptr_t offset) {
        if (!object)
            return nullptr;
        return *reinterpret_cast<Obj**>(
            reinterpret_cast<std::uintptr_t>(object) + offset
        );
    }

    inline bool RefreshStockTriggerContext(StockTriggerContext& context) {
        if (!context.triggerSystem)
            return false;

        Obj* instances = ReadObjectFieldByOffset(
            context.triggerSystem,
            kTriggerSystemInstancesOffset
        );
        Obj* triggeredEffects = ReadObjectFieldByOffset(
            context.triggerSystem,
            kTriggerSystemTriggeredEffectsOffset
        );
        Obj* finder = ReadObjectFieldByOffset(
            context.triggerSystem,
            kTriggerSystemFinderOffset
        );

        if (!instances || !triggeredEffects || !finder)
            return false;

        if (std::strcmp(TypeName(finder), "GenericEffectEntityFinder`1") != 0)
            return false;

        Obj* model = ReadObjectFieldByOffset(
            finder,
            kGenericFinderModelOffset
        );
        if (!model)
            return false;

        context.instances = instances;
        context.triggeredEffects = triggeredEffects;
        context.finder = finder;
        context.model = model;
        return true;
    }

    inline void RememberStockTriggerCandidate(Obj* triggerSystem) {
        if (!triggerSystem)
            return;

        // The DI container assigns TriggerSystem<T>'s injected fields through
        // generated setters. RegisterEffectHandlers can run before Finder is
        // assigned in this build, so keep the TriggerSystem object itself and
        // resolve its injected fields lazily when an event is fired.
        for (std::size_t i = 0; i < stockTriggerContextCount; ++i) {
            if (stockTriggerContexts[i].triggerSystem != triggerSystem)
                continue;

            StockTriggerContext refreshed = stockTriggerContexts[i];
            RefreshStockTriggerContext(refreshed);

            for (std::size_t j = i + 1; j < stockTriggerContextCount; ++j)
                stockTriggerContexts[j - 1] = stockTriggerContexts[j];

            stockTriggerContexts[stockTriggerContextCount - 1] = refreshed;
            return;
        }

        StockTriggerContext candidate{};
        candidate.triggerSystem = triggerSystem;
        RefreshStockTriggerContext(candidate);

        if (stockTriggerContextCount < stockTriggerContexts.size()) {
            stockTriggerContexts[stockTriggerContextCount++] = candidate;
        } else {
            // Only evict once the large recency window is genuinely full.
            // This preserves all producer systems from the current match and
            // its prediction models under normal play.
            for (std::size_t i = 1; i < stockTriggerContexts.size(); ++i)
                stockTriggerContexts[i - 1] = stockTriggerContexts[i];
            stockTriggerContexts[stockTriggerContexts.size() - 1] = candidate;
        }

        if constexpr (CustomComponents::Diagnostics::kVerbose) CC_DIAG(
            "CustomComponents/CustomTrigger: captured TriggerSystem candidate system=%p instances=%p triggered=%p finder=%p model=%p count=%zu",
            candidate.triggerSystem,
            candidate.instances,
            candidate.triggeredEffects,
            candidate.finder,
            candidate.model,
            stockTriggerContextCount
        );
    }

    static void TriggerSystemRegisterEffectHandlersHook(
        void* self,
        Obj* router,
        const void* method
    ) {
        // Capture the TriggerSystem object itself, but do not require its
        // injected fields to be populated yet. In this build Finder can still
        // be null when RegisterEffectHandlers runs. RefreshStockTriggerContext
        // resolves Instances/TriggeredEffects/Finder/Model lazily at fire time.
        RememberStockTriggerCandidate(static_cast<Obj*>(self));
        originalTriggerSystemRegisterHandlers(self, router, method);
    }

    inline bool ModelContainsEntityId(Obj* model, int id);

    inline Obj* GetStateCheckInstances(void* stateCheckSystem) {
        if (!stateCheckSystem)
            return nullptr;

        Obj* effectAdder = ReadObjectFieldByOffset(
            static_cast<Obj*>(stateCheckSystem),
            kStateCheckEffectAdderOffset
        );
        if (!effectAdder)
            return nullptr;

        return ReadObjectFieldByOffset(
            effectAdder,
            kStateCheckAdderInstancesOffset
        );
    }

    inline StockTriggerContext* FindStockTriggerContextForEffect(
        Obj* effect,
        Obj* preferredInstances = nullptr
    ) {
        if (!effect)
            return nullptr;

        const int effectEntityId =
            effectEffectEntityId.IsValid()
                ? effectEffectEntityId[effect].Get()
                : -1;
        const int targetId = effectTargetId[effect].Get();
        const int sourceId = effectSourceId[effect].Get();

        // First choice: exact DI/simulation scope identity.
        //
        // Entity ids are reused heavily by prediction/snapshot models, so
        // "model contains id N" is not sufficient to prove a context is live.
        // If StateCheckSystem gave us its live InstanceManager, only accept a
        // TriggerSystem<T> injected with that exact same InstanceManager.
        if (preferredInstances) {
            for (std::size_t i = stockTriggerContextCount; i-- > 0;) {
                StockTriggerContext& context = stockTriggerContexts[i];
                if (!RefreshStockTriggerContext(context))
                    continue;

                if (context.instances != preferredInstances)
                    continue;

                if constexpr (CustomComponents::Diagnostics::kVerbose) CC_DIAG(
                    "CustomComponents/CustomTrigger: producer select ACTIVE-SCOPE system=%p model=%p instances=%p triggered=%p finder=%p",
                    context.triggerSystem,
                    context.model,
                    context.instances,
                    context.triggeredEffects,
                    context.finder
                );
                return &context;
            }

            // Do not fall back to recycled entity-id matching when a concrete
            // live StateCheck scope is known. A stale model with the same ids
            // is exactly what caused the previous engine lock.
            LOG_ERR(
                "CustomComponents/CustomTrigger: no TriggerSystem producer matched active StateCheck InstanceManager=%p contexts=%zu",
                preferredInstances,
                stockTriggerContextCount
            );
            return nullptr;
        }

        auto matches = [&](StockTriggerContext& context, int id) -> bool {
            if (id < 0)
                return false;

            if (!RefreshStockTriggerContext(context))
                return false;

            return context.model && ModelContainsEntityId(context.model, id);
        };

        // The descriptor EffectEntityId is the strongest discriminator because
        // it identifies the actual ability/effect entity that produced this
        // runtime effect. Prefer contexts containing it, then target, then source.
        if (effectEntityId >= 0) {
            for (std::size_t i = stockTriggerContextCount; i-- > 0;) {
                if (matches(stockTriggerContexts[i], effectEntityId)) {
                    if constexpr (CustomComponents::Diagnostics::kVerbose) CC_DIAG(
                        "CustomComponents/CustomTrigger: producer select EFFECT-ENTITY system=%p model=%p finder=%p EffectEntityId=%d",
                        stockTriggerContexts[i].triggerSystem,
                        stockTriggerContexts[i].model,
                        stockTriggerContexts[i].finder,
                        effectEntityId
                    );
                    return &stockTriggerContexts[i];
                }
            }
        }

        if (targetId >= 0) {
            for (std::size_t i = stockTriggerContextCount; i-- > 0;) {
                if (matches(stockTriggerContexts[i], targetId)) {
                    if constexpr (CustomComponents::Diagnostics::kVerbose) CC_DIAG(
                        "CustomComponents/CustomTrigger: producer select TARGET system=%p model=%p finder=%p TargetId=%d",
                        stockTriggerContexts[i].triggerSystem,
                        stockTriggerContexts[i].model,
                        stockTriggerContexts[i].finder,
                        targetId
                    );
                    return &stockTriggerContexts[i];
                }
            }
        }

        if (sourceId >= 0) {
            for (std::size_t i = stockTriggerContextCount; i-- > 0;) {
                if (matches(stockTriggerContexts[i], sourceId)) {
                    if constexpr (CustomComponents::Diagnostics::kVerbose) CC_DIAG(
                        "CustomComponents/CustomTrigger: producer select SOURCE system=%p model=%p finder=%p SourceId=%d",
                        stockTriggerContexts[i].triggerSystem,
                        stockTriggerContexts[i].model,
                        stockTriggerContexts[i].finder,
                        sourceId
                    );
                    return &stockTriggerContexts[i];
                }
            }
        }

        LOG_ERR(
            "CustomComponents/CustomTrigger: no TriggerSystem producer context matched EffectEntityId=%d TargetId=%d SourceId=%d contexts=%zu",
            effectEntityId,
            targetId,
            sourceId,
            stockTriggerContextCount
        );
        return nullptr;
    }

    inline bool ModelContainsEntityId(Obj* model, int id) {
        if (!model || id < 0)
            return false;

        auto* entities = modelEntities[model].Get();
        if (!entities || !entities->items)
            return false;

        for (int i = 0; i < entities->size; ++i) {
            Obj* entity = entities->items->m_Items[i];
            if (entity && entityId[entity].Get() == id)
                return true;
        }
        return false;
    }

    inline Obj* FindEntityById(Obj* model, int id) {
        if (!model || id < 0)
            return nullptr;

        auto* entities = modelEntities[model].Get();
        if (!entities || !entities->items)
            return nullptr;

        for (int i = 0; i < entities->size; ++i) {
            Obj* entity = entities->items->m_Items[i];
            if (entity && entityId[entity].Get() == id)
                return entity;
        }
        return nullptr;
    }

