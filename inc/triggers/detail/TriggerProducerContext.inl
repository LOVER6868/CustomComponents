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

        auto* components = Core::Metadata::Bind(entityComponents,entity).Get();
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

        auto* managedName = Core::Metadata::Bind(customTriggerName,component).Get();
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

    inline bool RefreshStockTriggerContext(StockTriggerContext& context) {
        if (!context.triggerSystem)
            return false;
        auto instancesField=producerInstancesField;
        auto triggeredField=producerTriggeredEffectsField;
        auto finderField=producerFinderField;
        Obj* instances=instancesField.IsValid()?instancesField[context.triggerSystem].Get():nullptr;
        Obj* triggeredEffects=triggeredField.IsValid()?triggeredField[context.triggerSystem].Get():nullptr;
        Obj* finder=finderField.IsValid()?finderField[context.triggerSystem].Get():nullptr;
        auto reject = [&](const char* reason) {
            static std::atomic<unsigned> samples{0};
            if (samples.fetch_add(1) < 32)
                LOG_WARN("CustomComponents/CustomTrigger: producer rejected reason=%s system=%p instances=%p triggered=%p finder=%p finderType=%s model=%p fields=%d/%d/%d/%d",
                    reason, context.triggerSystem, instances, triggeredEffects, finder,
                    TypeName(finder), finder && finderModelField.IsValid()
                        ? Core::Metadata::Bind(finderModelField,finder).Get() : nullptr,
                    int(producerInstancesField.IsValid()),
                    int(producerTriggeredEffectsField.IsValid()),
                    int(producerFinderField.IsValid()), int(finderModelField.IsValid()));
            return false;
        };

        if (!instances || !triggeredEffects || !finder)
            return reject("missing-injected-value");

        if (std::strcmp(TypeName(finder), "GenericEffectEntityFinder`1") != 0)
            return reject("unexpected-finder-type");

        auto modelField=finderModelField;
        Obj* model=modelField.IsValid()?modelField[finder].Get():nullptr;
        if (!model)
            return reject("missing-finder-model");

        context.instances = instances;
        context.triggeredEffects = triggeredEffects;
        context.finder = finder;
        context.model = model;
        return true;
    }

    inline std::vector<StockTriggerContext> SnapshotStockTriggerContexts() {
        std::vector<std::shared_ptr<Core::ManagedRuntime::Root>> references;
        {
            std::lock_guard<std::mutex> guard(producerLock);
            producerReferences.erase(std::remove_if(producerReferences.begin(),producerReferences.end(),
                [](const auto& reference){return !reference->Get();}),producerReferences.end());
            references=producerReferences;
        }
        std::vector<StockTriggerContext> contexts;
        for(const auto& reference:references) {
            auto instanceField=producerInstancesField;
            Obj* system=reference->Get();
            Obj* instances=system && instanceField.IsValid()?instanceField[system].Get():nullptr;
            // Every TriggerSystem<T> in one DI scope shares these services.
            // Keep one coherent producer per scope instead of rooting and
            // validating every marker specialization on every enqueue.
            if(instances && std::any_of(contexts.begin(),contexts.end(),
                [&](const auto& c){return c.instances==instances;}))continue;
            auto lifetime=std::make_shared<Core::ManagedRuntime::Root>(reference->Get());
            StockTriggerContext candidate;
            candidate.lifetime=lifetime;candidate.triggerSystem=lifetime->Get();
            if(RefreshStockTriggerContext(candidate)) contexts.push_back(std::move(candidate));
        }
        return contexts;
    }
    inline StockTriggerContext* SelectedStockTriggerContext(const StockTriggerContext& context) {
        thread_local StockTriggerContext selected;
        selected=context;return &selected;
    }
    inline void RememberStockTriggerCandidate(Obj* triggerSystem) {
        if(!triggerSystem) return;
        std::lock_guard<std::mutex> guard(producerLock);
        for(const auto& reference:producerReferences) if(reference->Get()==triggerSystem) return;
        auto reference=std::make_shared<Core::ManagedRuntime::Root>(triggerSystem,true);
        if(reference->Get()) producerReferences.push_back(reference);
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
        if (!originalTriggerSystemRegisterHandlers)
            return;
        RememberStockTriggerCandidate(static_cast<Obj*>(self));
        static std::atomic<unsigned> samples{0};
        if (samples.fetch_add(1) < 3)
            LOG_INFO("CustomComponents/CustomTrigger: producer registration observed system=%p",
                self);
        originalTriggerSystemRegisterHandlers(self, router, method);
    }

    inline bool ModelContainsEntityId(Obj* model, int id);

    inline Obj* GetStateCheckInstances(void* stateCheckSystem) {
        if (!stateCheckSystem)
            return nullptr;

        Obj* effectAdder = stateCheckAdderField.IsValid()
            ? Core::Metadata::Bind(stateCheckAdderField,static_cast<Obj*>(stateCheckSystem)).Get() : nullptr;
        if (!effectAdder)
            return nullptr;

        return stateCheckAdderInstancesField.IsValid()
            ? Core::Metadata::Bind(stateCheckAdderInstancesField,effectAdder).Get() : nullptr;
    }

    inline StockTriggerContext* FindStockTriggerContextForEffect(
        Obj* effect,
        Obj* preferredInstances = nullptr
    ) {
        auto stockTriggerContexts=SnapshotStockTriggerContexts();
        const auto stockTriggerContextCount=stockTriggerContexts.size();
        if (!effect)
            return nullptr;

        const int effectEntityId =
            effectEffectEntityId.IsValid()
                ? Core::Metadata::Bind(effectEffectEntityId,effect).Get()
                : -1;
        const int targetId = Core::Metadata::Bind(effectTargetId,effect).Get();
        const int sourceId = Core::Metadata::Bind(effectSourceId,effect).Get();

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
                return SelectedStockTriggerContext(context);
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
                    return SelectedStockTriggerContext(stockTriggerContexts[i]);
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
                    return SelectedStockTriggerContext(stockTriggerContexts[i]);
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
                    return SelectedStockTriggerContext(stockTriggerContexts[i]);
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

    // A caller with an authoritative EntityModel must not guess the producer
    // from reused EffectEntity / card ids. In particular, discard-to-lane
    // already has the stock lane system's exact live model. Prediction clones
    // can contain every one of those ids while targeting a different queue.
    inline StockTriggerContext* FindStockTriggerContextForModel(
        Obj* expectedModel,
        Obj* preferredInstances = nullptr
    ) {
        auto stockTriggerContexts=SnapshotStockTriggerContexts();
        const auto stockTriggerContextCount=stockTriggerContexts.size();
        if (!expectedModel)
            return nullptr;

        for (std::size_t i = stockTriggerContextCount; i-- > 0;) {
            StockTriggerContext& candidate = stockTriggerContexts[i];
            if (!RefreshStockTriggerContext(candidate) ||
                candidate.model != expectedModel)
                continue;
            if (preferredInstances && candidate.instances != preferredInstances)
                continue;
            return SelectedStockTriggerContext(candidate);
        }

        LOG_ERR(
            "CustomComponents/CustomTrigger: exact-model producer missing model=%p activeInstances=%p contexts=%zu",
            expectedModel,
            preferredInstances,
            stockTriggerContextCount
        );
        return nullptr;
    }

    inline bool ModelContainsEntityId(Obj* model, int id) {
        if (!model || id < 0)
            return false;

        auto* entities = Core::Metadata::Bind(modelEntities,model).Get();
        if (!entities || !entities->items)
            return false;

        for (int i = 0; i < entities->size; ++i) {
            Obj* entity = entities->items->m_Items[i];
            if (entity && Core::Metadata::Bind(entityId,entity).Get() == id)
                return true;
        }
        return false;
    }

    inline Obj* FindEntityById(Obj* model, int id) {
        if (!model || id < 0)
            return nullptr;

        auto* entities = Core::Metadata::Bind(modelEntities,model).Get();
        if (!entities || !entities->items)
            return nullptr;

        for (int i = 0; i < entities->size; ++i) {
            Obj* entity = entities->items->m_Items[i];
            if (entity && Core::Metadata::Bind(entityId,entity).Get() == id)
                return entity;
        }
        return nullptr;
    }

