// Internal implementation fragment for ZoneTriggeredAbilityPresentationSystem.
// Effect audit / diagnostic stock effect callbacks and registration.
// Included at its original declaration position inside the owning namespace.
// Not a standalone public header; hook installation remains in the parent header.

    // Read-only audit of off-board gameplay effects.
    // No frontend modifications, no persistent snapshot state, no gameplay writes.
    // The six hooks observe effective stats before and after a stock handler.
    inline BNM::Field<int> auditEffectTarget;
    inline BNM::Field<int> auditEffectSource;
    inline BNM::Field<int> auditEffectEntity;
    inline BNM::Field<Obj*> auditSystemModel[6];
    inline BNM::Field<int> auditGrantType;
    inline BNM::Field<int> auditRemoveGrantType;
    inline std::uint64_t auditSeen[6]{};
    inline std::uint64_t auditReported[6]{};
    inline constexpr std::uint64_t kAuditLogLimit = 240;
    using AuditProcessFn = void (*)(void*, Obj*, const void*);
    inline AuditProcessFn originalAuditProcess[6]{};

    struct AuditStats {
        int attack = -99999;
        int maxHealth = -99999;
        int damage = -99999;
        int cost = -99999;
        int componentCount = -1;
        std::uint64_t componentSignature = 1469598103934665603ull;
    };

    inline AuditStats CaptureAuditStats(Obj* entity) {
        AuditStats stats{};
        if (!entity) return stats;
        if (Obj* attack = TraceFindComponent(entity, "Attack");
            attack && componentCalculateAttack.IsValid())
            stats.attack = componentCalculateAttack[attack]();
        if (Obj* health = TraceFindComponent(entity, "Health"); health) {
            if (healthCurrentDamage.IsValid()) stats.damage = healthCurrentDamage[health].Get();
            if (healthMaxHealth.IsValid() && calculateModifiableValue.IsValid()) {
                Obj* value = healthMaxHealth[health].Get();
                if (value) stats.maxHealth = calculateModifiableValue(value);
            }
        }
        if (Obj* cost = TraceFindComponent(entity, "SunCost");
            cost && sunCostValue.IsValid() && calculateModifiableValue.IsValid()) {
            Obj* value = sunCostValue[cost].Get();
            if (value) stats.cost = calculateModifiableValue(value);
        }
        // Diagnostic component-type fingerprint. This detects added/removed
        // components, but not ability-value changes within an existing component.
        if (traceEntityComponents.IsValid()) {
            Obj* listObject = traceEntityComponents[entity].Get();
            if (listObject) {
                auto* components = reinterpret_cast<BNM::Structures::Mono::List<Obj*>*>(listObject);
                int count = components->GetSize();
                if (count >= 0 && count <= 256 && components->items) {
                    stats.componentCount = count;
                    Obj** elements = components->GetData();
                    if (elements) for (int i = 0; i < count; ++i) {
                        const char* name = TraceObjectType(elements[i]);
                        for (const unsigned char* c = reinterpret_cast<const unsigned char*>(name); *c; ++c) {
                            stats.componentSignature ^= *c;
                            stats.componentSignature *= 1099511628211ull;
                        }
                        stats.componentSignature ^= 0xff;
                        stats.componentSignature *= 1099511628211ull;
                    }
                }
            }
        }
        return stats;
    }

    // Existing hook owners (DiscardedSystem and GrantAbility) call this
    // around their OWN handler. No second inline hook is installed.
    template<class CallOriginal>
    inline void AuditOwnedEffect(int index, void* self, Obj* effect,
                                 CallOriginal&& invoke) {
        const auto seen = ++auditSeen[index];
        Obj* model = self && auditSystemModel[index].IsValid()
            ? auditSystemModel[index][static_cast<Obj*>(self)].Get() : nullptr;
        const int target = effect && auditEffectTarget.IsValid()
            ? auditEffectTarget[effect].Get() : -1;
        const int source = effect && auditEffectSource.IsValid()
            ? auditEffectSource[effect].Get() : -1;
        const int effectEntity = effect && auditEffectEntity.IsValid()
            ? auditEffectEntity[effect].Get() : -1;
        Obj* card = model && target >= 0 && entityExists.IsValid() && getEntityById.IsValid()
            && entityExists[model](target) ? getEntityById[model](target) : nullptr;
        // Require a card and an unambiguous current Deck/Discard membership.
        const bool isCard = card && TraceFindComponent(card, "Card");
        const bool hand = isCard && TraceFindComponent(card, "HandElement");
        const bool lane = isCard && TraceFindComponent(card, "LaneElement");
        const bool discard = isCard && TraceFindComponent(card, "DiscardPileElement");
        const bool deck = isCard && IsInMaterializedDeckPool(model, target);
        const bool offboard = isCard && !hand && !lane && (discard != deck);
        const char* zone = offboard ? (discard ? "discard" : "deck") : "stock";
        const bool report = offboard && auditReported[index] < kAuditLogLimit;
        const AuditStats before = report ? CaptureAuditStats(card) : AuditStats{};
        const int abilityType = effect && index == 2 && auditGrantType.IsValid()
            ? auditGrantType[effect].Get() :
            (effect && index == 3 && auditRemoveGrantType.IsValid()
                ? auditRemoveGrantType[effect].Get() : -1);
        invoke();
        if (!report) return;
        ++auditReported[index];
        const AuditStats after = entityExists[model](target)
            ? CaptureAuditStats(getEntityById[model](target)) : AuditStats{};
        // Record changes only if the handler entered in Deck/Discard.
        // A repeated off-board visit starts a new epoch after presentation.
        if (auto* delta = FindOffboardDelta(model, target, true)) {
            if (delta->compared) {
                const Obj* oldModel = delta->model;
                const int oldId = delta->cardId;
                *delta = {};
                delta->model = const_cast<Obj*>(oldModel);
                delta->cardId = oldId;
            }
            auto addValid = [&](int a, int b, int& accumulator) {
                if (a == -99999 || b == -99999) {
                    ++delta->invalidFields;
                    return;
                }
                const std::int64_t total = static_cast<std::int64_t>(accumulator) +
                    static_cast<std::int64_t>(b) - a;
                if (total < -100000 || total > 100000) {
                    ++delta->invalidFields;
                    return;
                }
                accumulator = static_cast<int>(total);
            };
            if (delta->events == 0) delta->firstSignature = before.componentSignature;
            addValid(before.attack, after.attack, delta->attack);
            addValid(before.maxHealth, after.maxHealth, delta->health);
            addValid(before.cost, after.cost, delta->cost);
            if (before.componentSignature != after.componentSignature)
                ++delta->traitObservations;
            delta->lastSignature = after.componentSignature;
            ++delta->events;
            if (delta->events <= 12)
                CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/OFFBOARD_DELTA: ACCUMULATE model=%p card=%d zone=%s event=%u attack=%d health=%d cost=%d traitsChanged=%u (read-only)",
                    model, target, zone, delta->events, delta->attack,
                    delta->health, delta->cost, delta->traitObservations);
        }
        static constexpr const char* names[] = {
            "Buff", "ModifySunCost", "GrantAbility", "RemoveGrantAbility",
            "RemoveBuff", "RemoveModifySunCost"
        };
        CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/OFFBOARD_AUDIT: %s event=%llu model=%p target=%d source=%d effectEntity=%d zoneAtEntry=%s abilityType=%d attack=%d->%d maxHealth=%d->%d damage=%d->%d cost=%d->%d components=%d->%d signature=%llx->%llx (read-only)",
            names[index], (unsigned long long)seen, model, target, source,
            effectEntity, zone, abilityType, before.attack, after.attack,
            before.maxHealth, after.maxHealth, before.damage, after.damage,
            before.cost, after.cost, before.componentCount, after.componentCount,
            (unsigned long long)before.componentSignature,
            (unsigned long long)after.componentSignature);
    }

    inline void AuditedStockEffect(int index, void* self, Obj* effect,
                                   const void* method) {
        auto original = originalAuditProcess[index];
        if (!original) return;
        AuditOwnedEffect(index, self, effect, [&] { original(self, effect, method); });
    }

    static void AuditBuffHook(void* s, Obj* e, const void* m) { AuditedStockEffect(0,s,e,m); }
    static void AuditCostHook(void* s, Obj* e, const void* m) { AuditedStockEffect(1,s,e,m); }
    static void AuditGrantHook(void* s, Obj* e, const void* m) { AuditedStockEffect(2,s,e,m); }
    static void AuditRemoveGrantHook(void* s, Obj* e, const void* m) { AuditedStockEffect(3,s,e,m); }
    static void AuditRemoveBuffHook(void* s, Obj* e, const void* m) { AuditedStockEffect(4,s,e,m); }
    static void AuditRemoveCostHook(void* s, Obj* e, const void* m) { AuditedStockEffect(5,s,e,m); }

    inline void InstallOffBoardAudit() {
        auto baseEffect = BNM::Class("PvZCards.Engine", "Effect");
        if (!baseEffect.IsValid()) return;
        auditEffectTarget = baseEffect.GetField("TargetId");
        auditEffectSource = baseEffect.GetField("SourceId");
        auditEffectEntity = baseEffect.GetField("EffectEntityId");
        if (!auditEffectTarget.IsValid() || !auditEffectSource.IsValid() ||
            !auditEffectEntity.IsValid() || !traceEntityComponents.IsValid() ||
            !componentCalculateAttack.IsValid()) {
            CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/OFFBOARD_AUDIT: unavailable base metadata");
            return;
        }
        const char* systems[] = { "BuffSystem", "ModifySunCostSystem", "GrantAbilitySystem",
            "RemoveGrantAbilitySystem", "RemoveBuffSystem", "RemoveModifySunCostSystem" };
        const char* modelFields[] = { "<Entities>k__BackingField", "<Entities>k__BackingField",
            "<Entities>k__BackingField", "<Model>k__BackingField",
            "<Model>k__BackingField", "<Model>k__BackingField" };
        AuditProcessFn hooks[] = { AuditBuffHook, AuditCostHook, AuditGrantHook,
            AuditRemoveGrantHook, AuditRemoveBuffHook, AuditRemoveCostHook };
        const std::uintptr_t expectedRvas[] = {0x21FD444, 0x21F10DC, 0x21F45EC,
            0x21F1C58, 0x21FD258, 0x21F1EA8};
        auto grant = BNM::Class("PvZCards.Engine.Effects", "GrantAbilityEffect");
        auto removeGrant = BNM::Class("PvZCards.Engine.Effects", "RemoveGrantAbilityEffect");
        if (grant.IsValid()) auditGrantType = grant.GetField("GrantableAbilityType");
        if (removeGrant.IsValid()) auditRemoveGrantType = removeGrant.GetField("GrantableAbilityType");
        for (int i = 0; i < 6; ++i) {
            // The stock buff hook belongs to DiscardedSystem; grant and
            // remove-grant hooks belong to GrantAbility. Those owners call
            // AuditOwnedEffect directly; never compete with their hooks.
            const bool ownedElsewhere = i == 0 || i == 2 || i == 3;
            if (ownedElsewhere) {
                CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/OFFBOARD_AUDIT: %s shared-owner (no duplicate native hook)", systems[i]);
            }
            auto klass = BNM::Class("PvZCards.Core.Systems", systems[i]);
            if (!klass.IsValid()) continue;
            auditSystemModel[i] = klass.GetField(modelFields[i]);
            auto method = klass.GetMethod("ProcessEffect", 1);
            if (!auditSystemModel[i].IsValid() || !method.IsValid() ||
                !nativeIl2cppBase ||
                static_cast<std::uintptr_t>(method.GetOffset()) != nativeIl2cppBase + expectedRvas[i]) {
                CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/OFFBOARD_AUDIT: %s skipped (metadata/RVA mismatch)", systems[i]);
                continue;
            }
            if (ownedElsewhere) continue;
            BNM::BasicHook(method, hooks[i], originalAuditProcess[i]);
            CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/OFFBOARD_AUDIT: %s hooked=%d rva=0x%llx",
                systems[i], int(originalAuditProcess[i] != nullptr),
                (unsigned long long)expectedRvas[i]);
        }
    }

