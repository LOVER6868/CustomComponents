#include "diagnostics/DiagnosticLogging.hpp"
// Internal implementation fragment for ZoneTriggeredAbilityPresentationSystem.
// Card-frame bindings, return baselines, frame repair and post-return sync hooks.
// Included at its original declaration position inside the owning namespace.
// Not a standalone public header; hook installation remains in the parent header.

    // Only cards with a *suppressed* off-board buff are eligible for UI repair.
    struct PendingFrameSync { Obj* model = nullptr; int entityId = -1; };
    inline std::array<PendingFrameSync, 512> pendingFrameSyncs{};
    inline BNM::Field<int> cardViewEntityId;
    inline BNM::Field<Obj*> cardViewFrame;
    inline BNM::Field<int> frameCurrentAttack;
    inline BNM::Field<int> frameInitialAttack;
    inline BNM::Field<int> frameInitialHealth;
    inline BNM::Field<int> frameInitialSunCost;
    inline BNM::Method<void> frameBuffAttack;
    inline BNM::Method<int> componentCalculateAttack;
    // Generic hand-entry synchronization. Stock GrantAbilityPresentation remains
    // the sole contributor of custom abilities to SpecialAbilityChecker.
    inline BNM::Field<int> frameMaxHealth;
    inline BNM::Field<int> frameCurrentHealth;
    inline BNM::Field<int> frameCurrentSunCost;
    inline BNM::Field<int> healthCurrentDamage;
    inline BNM::Field<Obj*> healthMaxHealth;
    inline BNM::Field<Obj*> sunCostValue;
    inline BNM::Method<int> calculateModifiableValue;
    inline BNM::Method<void> frameBuffHealth;
    inline BNM::Method<void> frameModifySunCost;
    inline BNM::Method<void> frameSetSpecialAbilitySprites;
    inline BNM::Class stateClass;
    inline BNM::Class checkerClass;
    inline BNM::Method<void> stateCtor;
    inline BNM::Method<void> checkerCtor;
    inline BNM::Method<void> checkerAddSpecialAbilities;
    inline BNM::Field<Obj*> stateAbilities;
    inline bool traitSyncReady = false;
    // A minimal EntityState is constructed solely for its initialized Abilities
    // list. No factory injection, stat mutation or new hook is needed.

    using CardLoadedFn = void (*)(void*, Obj*, const void*);
    inline CardLoadedFn originalCardLoaded = nullptr;

    inline void RememberFrameSync(Obj* model, int id) {
        if (!model || id < 0) return;
        for (auto& e : pendingFrameSyncs) {
            if (e.model == model && e.entityId == id) return;
            if (!e.model) { e = {model, id}; return; }
        }
        LOG_ERR("CustomComponents/ZoneTriggeredAbilityPresentation/CARDSTAT_SYNC: tracking full id=%d", id);
    }

    // VIEWTRACE: diagnostic-only. Never apply a BuffAttack correction during
    // card loading; it is too early to establish the final view association.
    inline void TraceFrameForCard(Obj* view, const char* stage, int mappedId = -1, Obj* model = nullptr) {
        if (!view) return;
        const int cardId = cardViewEntityId.IsValid() ? cardViewEntityId[view].Get() : -1;
        const bool tracked = [&]() {
            for (const auto& e : pendingFrameSyncs)
                if (e.model && e.entityId == (mappedId >= 0 ? mappedId : cardId) &&
                    (!model || model == e.model)) return true;
            return false;
        }();
        Obj* frame = cardViewFrame.IsValid() ? cardViewFrame[view].Get() : nullptr;
        const int frameAttack = frame && frameCurrentAttack.IsValid() ? frameCurrentAttack[frame].Get() : -99999;
        int gameplayAttack = -99999;
        Obj* resolvedModel = model;
        const int id = mappedId >= 0 ? mappedId : cardId;
        if (!resolvedModel) for (const auto& e : pendingFrameSyncs)
            if (e.model && e.entityId == id) { resolvedModel = e.model; break; }
        if (resolvedModel && id >= 0 && entityExists.IsValid() && getEntityById.IsValid() &&
            entityExists[resolvedModel](id)) {
            Obj* entity = getEntityById[resolvedModel](id);
            Obj* attack = TraceFindComponent(entity, "Attack");
            if (attack && componentCalculateAttack.IsValid())
                gameplayAttack = componentCalculateAttack[attack]();
        }
        CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/VIEWTRACE: %s view=%p viewType=%s mappedId=%d cardId=%d tracked=%d model=%p frame=%p frameAttack=%d gameplayAttack=%d",
                 stage, view, TraceObjectType(view), mappedId, cardId, int(tracked), resolvedModel, frame, frameAttack, gameplayAttack);
    }

    static void CardLoadedSyncHook(void* self, Obj* data, const void* method) {
        Obj* view = static_cast<Obj*>(self);
        CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/VIEWTRACE: OnCardLoaded ENTER view=%p data=%p", self, data);
        if (view) TraceFrameForCard(view, "OnCardLoaded BEFORE");
        originalCardLoaded(self, data, method);
        if (view) TraceFrameForCard(view, "OnCardLoaded AFTER");
    }

    using AddViewObjectFn = void (*)(void*, int, Obj*, const void*);
    using ShowInHandFn = void (*)(void*, const void*);
    inline AddViewObjectFn originalAddViewObject = nullptr;
    inline ShowInHandFn originalShowInHand = nullptr;

    // Exact model/entity/view association, captured only after stock mapper registration.
    // This avoids guessing the EntityModel from a CardId shared across matches.
    struct FrameViewBinding { Obj* view = nullptr; Obj* model = nullptr; int id = -1; };
    inline std::array<FrameViewBinding, 256> frameViewBindings{};
    // A new EntityModel means a new match's CardViews. Never reuse pointers
    // or entity IDs bound to the previous scene.
    inline Obj* boundFrameModel = nullptr;

    // Deferred *visual-only* verification for a stock-created lane frame.
    // Return tickets exist only on the client that ran the custom transfer;
    // the opponent also needs a frame-to-model comparison after animation.
    // Keep numeric identities here: the scene-owned mapper is resolved through
    // CardViewLifecycleCoordinator's existing GC handle before any object access.
    struct PendingIdleLaneFrame {
        std::uintptr_t model = 0;
        std::uintptr_t mapper = 0;
        std::uintptr_t view = 0;
        std::uintptr_t animation = 0;
        std::uintptr_t queue = 0;
        int id = -1;
        bool completed = false;
        unsigned queueProbes = 0;
    };
    inline std::array<PendingIdleLaneFrame, 128> pendingIdleLaneFrames{};
    inline thread_local std::uintptr_t activeQueueUpdate = 0;

    inline void RememberIdleLaneFrame(Obj* animation, Obj* mapper, Obj* model,
                                      Obj* view, int id) {
        if (!animation || !mapper || !model || !view || id < 0) return;
        // A second ShowCard for the same animation must not schedule two buffs.
        for (auto& entry : pendingIdleLaneFrames) {
            if (entry.animation == reinterpret_cast<std::uintptr_t>(animation) ||
                (entry.model == reinterpret_cast<std::uintptr_t>(model) &&
                 entry.id == id)) {
                entry = {reinterpret_cast<std::uintptr_t>(model),
                         reinterpret_cast<std::uintptr_t>(mapper),
                         reinterpret_cast<std::uintptr_t>(view),
                         reinterpret_cast<std::uintptr_t>(animation),
                         activeQueueUpdate, id, false, 0};
                return;
            }
        }
        for (auto& entry : pendingIdleLaneFrames) {
            if (!entry.model) {
                entry = {reinterpret_cast<std::uintptr_t>(model),
                         reinterpret_cast<std::uintptr_t>(mapper),
                         reinterpret_cast<std::uintptr_t>(view),
                         reinterpret_cast<std::uintptr_t>(animation),
                         activeQueueUpdate, id, false, 0};
                return;
            }
        }
        LOG_ERR("CustomComponents/ZoneTriggeredAbilityPresentation/IDLE-STAT: pending table full id=%d", id);
    }

    inline void CompleteIdleLaneFrame(Obj* animation) {
        if (!animation) return;
        for (auto& entry : pendingIdleLaneFrames)
            if (entry.animation == reinterpret_cast<std::uintptr_t>(animation))
                entry.completed = true;
    }
    // Keep the pre-trigger lane baseline after its one-shot ticket is consumed.
    // Stock HandlePlayedToBoard may reset the same frame back to printed stats.
    struct LaneVisualBaseline {
        Obj* view = nullptr;
        Obj* model = nullptr;
        int id = -1;
        int attack = -99999;
        int maxHealth = -99999;
        int damage = -99999;
        int cost = -99999;
    };
    inline std::array<LaneVisualBaseline, 256> laneVisualBaselines{};

    // Hand/deck -> discard -> hand: capture provenance on the gameplay thread
    // while Discarded.Source still exists. The frontend consumes a ticket only
    // on a newly shown card view. This does not duplicate off-board buff deltas
    // or apply gameplay buffs; the model is the only source of target values.
    // Integer identity + numeric model address only; no cross-thread Unity access.
    struct HandDeckReturnTicket {
        std::uintptr_t model = 0;
        int cardId = -1;
        int source = 0;
        unsigned remaining = 0;
        std::uintptr_t lastView = 0;
        // Captured before ReturnedFromDiscardPileTrigger changes the model.
        int attackBeforeTrigger = -99999;
        int healthBeforeTrigger = -99999;
        int damageBeforeTrigger = -99999;
        int costBeforeTrigger = -99999;
        // SpecialAbilityChecker reports both stock and registry-driven traits.
        // Copy enum values only; never carry managed objects across threads.
        std::array<int, 32> traitsBeforeTrigger{};
        unsigned traitCountBeforeTrigger = 0;
        bool traitSnapshotValid = false;
    };
    inline std::mutex handDeckReturnMutex;
    inline std::array<HandDeckReturnTicket, 512> handDeckReturnTickets{};

    inline void RememberHandDeckReturn(Obj* model, int id, int source) {
        if (!model || id < 0 || (source != 1 && source != 2 && source != 3)) return;
        // This function is called after the live entity moves into hand but
        // BEFORE FireReturnedFromDiscardPile. Read gameplay stats only here.
        int attackValue = -99999, healthValue = -99999;
        int damageValue = -99999, costValue = -99999;
        std::array<int, 32> traitValues{};
        unsigned traitCount = 0;
        bool traitValid = false;
        if (entityExists.IsValid() && getEntityById.IsValid() &&
            entityExists[model](id)) {
            Obj* entity = getEntityById[model](id);
            Obj* attack = TraceFindComponent(entity, "Attack");
            Obj* health = TraceFindComponent(entity, "Health");
            Obj* cost = TraceFindComponent(entity, "SunCost");
            if (attack && componentCalculateAttack.IsValid())
                attackValue = componentCalculateAttack[attack]();
            if (health && healthMaxHealth.IsValid() && calculateModifiableValue.IsValid()) {
                Obj* value = healthMaxHealth[health].Get();
                if (value) healthValue = calculateModifiableValue(value);
            }
            if (health && healthCurrentDamage.IsValid())
                damageValue = healthCurrentDamage[health].Get();
            if (cost && sunCostValue.IsValid() && calculateModifiableValue.IsValid()) {
                Obj* value = sunCostValue[cost].Get();
                if (value) costValue = calculateModifiableValue(value);
                if (costValue != -99999 && costValue < 0) costValue = 0;
            }
            // Capture PRE-trigger traits using the existing registry bridge.
            // Its hook runs within SpecialAbilityChecker::AddSpecialAbilities.
            if (traitSyncReady && stateClass.IsValid() && checkerClass.IsValid()) {
                Obj* state = stateClass.CreateNewInstance();
                Obj* checker = checkerClass.CreateNewInstance();
                if (state && checker) {
                    stateCtor[state]();
                    checkerCtor[checker]();
                    Obj* abilities = stateAbilities[state].Get();
                    if (abilities) {
                        checkerAddSpecialAbilities[checker](abilities, entity);
                        // EntityState.Abilities is a managed List<enum> (int-backed).
                        auto* list = reinterpret_cast<BNM::Structures::Mono::List<int>*>(abilities);
                        if (list->size >= 0 && list->size <= static_cast<int>(traitValues.size()) &&
                            (list->size == 0 || list->items)) {
                            traitCount = static_cast<unsigned>(list->size);
                            for (unsigned i = 0; i < traitCount; ++i)
                                traitValues[i] = list->items->m_Items[i];
                            traitValid = true;
                        } else {
                            LOG_ERR("CustomComponents/ZoneTriggeredAbilityPresentation/RETURN-TRAIT: snapshot overflow id=%d count=%d", id, list->size);
                        }
                    }
                }
            }
        }
        std::lock_guard<std::mutex> lock(handDeckReturnMutex);
        HandDeckReturnTicket* empty = nullptr;
        const auto key = reinterpret_cast<std::uintptr_t>(model);
        for (auto& e : handDeckReturnTickets) {
            if (e.model == key && e.cardId == id) {
                if (e.remaining < 32) ++e.remaining;
                e.source = source;
                e.attackBeforeTrigger = attackValue;
                e.healthBeforeTrigger = healthValue;
                e.damageBeforeTrigger = damageValue;
                e.costBeforeTrigger = costValue;
                e.traitsBeforeTrigger = traitValues;
                e.traitCountBeforeTrigger = traitCount;
                e.traitSnapshotValid = traitValid;
                CC_DIAG("CustomComponents/ZoneTriggeredAbilityPresentation/RETURN-TRAIT: SNAPSHOT id=%d source=%d count=%u valid=%d (updated ticket)", id, source, traitCount, int(traitValid));
                CC_DIAG("CustomComponents/ZoneTriggeredAbilityPresentation/RETURN-STAT: SNAPSHOT id=%d source=%d atk=%d hp=%d damage=%d cost=%d (updated ticket)",
                    id, source, attackValue, healthValue, damageValue, costValue);
                return;
            }
            if (!e.model && !empty) empty = &e;
        }
        if (empty) {
            *empty = {key, id, source, 1, 0, attackValue, healthValue, damageValue, costValue};
            empty->traitsBeforeTrigger = traitValues;
            empty->traitCountBeforeTrigger = traitCount;
            empty->traitSnapshotValid = traitValid;
            CC_DIAG("CustomComponents/ZoneTriggeredAbilityPresentation/RETURN-TRAIT: SNAPSHOT id=%d source=%d count=%u valid=%d", id, source, traitCount, int(traitValid));
            CC_DIAG("CustomComponents/ZoneTriggeredAbilityPresentation/RETURN-STAT: SNAPSHOT id=%d source=%d atk=%d hp=%d damage=%d cost=%d",
                id, source, attackValue, healthValue, damageValue, costValue);
        } else LOG_ERR("CustomComponents/ZoneTriggeredAbilityPresentation/RETURN-STAT: ticket table full card=%d", id);
    }

    inline bool HasLaneReturnSnapshot(Obj* model, int id) {
        if (!model || id < 0) return false;
        std::lock_guard<std::mutex> lock(handDeckReturnMutex);
        const auto key = reinterpret_cast<std::uintptr_t>(model);
        for (const auto& entry : handDeckReturnTickets)
            if (entry.model == key && entry.cardId == id &&
                entry.source == 3 && entry.remaining) return true;
        return false;
    }

    inline bool HasHandDeckReturn(Obj* model, int id) {
        std::lock_guard<std::mutex> lock(handDeckReturnMutex);
        const auto key = reinterpret_cast<std::uintptr_t>(model);
        for (const auto& e : handDeckReturnTickets)
            if (e.model == key && e.cardId == id && e.remaining) return true;
        return false;
    }

    inline HandDeckReturnTicket ConsumeHandDeckReturnSnapshot(Obj* model, int id, Obj* view) {
        if (!view) return {};
        std::lock_guard<std::mutex> lock(handDeckReturnMutex);
        const auto key = reinterpret_cast<std::uintptr_t>(model);
        const auto viewKey = reinterpret_cast<std::uintptr_t>(view);
        for (auto& e : handDeckReturnTickets) {
            if (e.model != key || e.cardId != id || !e.remaining) continue;
            if (e.lastView == viewKey) return {};
            HandDeckReturnTicket result = e;
            --e.remaining;
            if (e.remaining) e.lastView = viewKey;
            else e = {};
            return result;
        }
        return {};
    }

    inline void BindFrameView(Obj* view, Obj* model, int id) {
        if (!view || !model || id < 0) return;
        if (boundFrameModel != model) {
            frameViewBindings.fill(FrameViewBinding{});
            pendingIdleLaneFrames.fill(PendingIdleLaneFrame{});
            laneVisualBaselines.fill(LaneVisualBaseline{});
            boundFrameModel = model;
        }
        // A returned entity gets a NEW CardView. Replace its previous
        // binding instead of keeping a dead pointer until the table fills.
        for (auto& binding : frameViewBindings) {
            if (binding.view == view ||
                (binding.model == model && binding.id == id)) {
                binding = {view, model, id};
                return;
            }
        }
        for (auto& binding : frameViewBindings) {
            if (!binding.view) {
                binding = {view, model, id};
                return;
            }
        }
        LOG_ERR("CustomComponents/ZoneTriggeredAbilityPresentation/CARDSTAT_SYNC: view binding table full id=%d", id);
    }

    inline void RememberLaneVisualBaseline(Obj* view, Obj* model, int id,
                                            const HandDeckReturnTicket& snapshot) {
        if (!view || !model || id < 0 || snapshot.source != 3) return;
        LaneVisualBaseline result{view, model, id, snapshot.attackBeforeTrigger,
                                  snapshot.healthBeforeTrigger,
                                  snapshot.damageBeforeTrigger,
                                  snapshot.costBeforeTrigger};
        for (auto& entry : laneVisualBaselines) {
            if (entry.view == view ||
                (entry.model == model && entry.id == id)) {
                entry = result;
                return;
            }
        }
        for (auto& entry : laneVisualBaselines) {
            if (!entry.view) { entry = result; return; }
        }
        LOG_ERR("CustomComponents/ZoneTriggeredAbilityPresentation/LANE-BASELINE: table full id=%d", id);
    }

    // Only restore a baseline if STOCK board entry visibly reset the frame
    // to its printed Initial* fields. The final model values are NOT replayed:
    // buffs triggered by the return still own their normal later animations.
    inline void RepairResetLaneFrame(Obj* view, const char* stage) {
        if (!view || !cardViewEntityId.IsValid() || !cardViewFrame.IsValid()) return;
        const int id = cardViewEntityId[view].Get();
        const LaneVisualBaseline* baseline = nullptr;
        for (const auto& item : laneVisualBaselines) {
            if (item.view == view && item.id == id) { baseline = &item; break; }
        }
        if (!baseline) return;
        bool bound = false;
        for (const auto& item : frameViewBindings) {
            if (item.view == view && item.model == baseline->model && item.id == id) {
                bound = true;
                break;
            }
        }
        if (!bound || !entityExists.IsValid() || !getEntityById.IsValid() ||
            !entityExists[baseline->model](id)) return;
        Obj* entity = getEntityById[baseline->model](id);
        if (!entity || !TraceFindComponent(entity, "LaneElement") ||
            TraceFindComponent(entity, "DiscardPileElement")) return;
        Obj* frame = cardViewFrame[view].Get();
        if (!frame) return;
        auto gap = [](int expected, int current) {
            const std::int64_t d = static_cast<std::int64_t>(expected) - current;
            return d >= -10000 && d <= 10000 ? static_cast<int>(d) : 0;
        };
        int atk = 0, hp = 0, cost = 0;
        if (baseline->attack != -99999 && frameCurrentAttack.IsValid() &&
            frameInitialAttack.IsValid() && frameBuffAttack.IsValid() &&
            frameCurrentAttack[frame].Get() == frameInitialAttack[frame].Get()) {
            atk = gap(baseline->attack, frameCurrentAttack[frame].Get());
            if (atk) frameBuffAttack[frame](atk);
        }
        if (baseline->maxHealth >= 0 && baseline->damage >= 0 &&
            baseline->damage <= baseline->maxHealth &&
            frameMaxHealth.IsValid() && frameCurrentHealth.IsValid() &&
            frameInitialHealth.IsValid() && frameBuffHealth.IsValid() &&
            frameMaxHealth[frame].Get() == frameInitialHealth[frame].Get() &&
            frameCurrentHealth[frame].Get() == frameMaxHealth[frame].Get() - baseline->damage) {
            hp = gap(baseline->maxHealth, frameMaxHealth[frame].Get());
            if (hp) frameBuffHealth[frame](hp);
        }
        if (baseline->cost >= 0 && frameCurrentSunCost.IsValid() &&
            frameInitialSunCost.IsValid() && frameModifySunCost.IsValid() &&
            frameCurrentSunCost[frame].Get() == frameInitialSunCost[frame].Get()) {
            cost = gap(baseline->cost, frameCurrentSunCost[frame].Get());
            if (cost) frameModifySunCost[frame](cost);
        }
        if (atk || hp || cost)
            CC_DIAG("CustomComponents/ZoneTriggeredAbilityPresentation/LANE-BASELINE: repaired reset stage=%s id=%d view=%p attack=%d health=%d cost=%d",
                     stage, id, view, atk, hp, cost);
    }

    // Diagnostic accumulation for a single off-board visit.
    // Entries are keyed by (EntityModel*, entityId), not by raw Entity*.
    // They are retired on first hand/lane comparison so subsequent returns do
    // not accidentally inherit the earlier trip's changes.
    struct OffboardDeltaEntry {
        Obj* model = nullptr;
        int cardId = -1;
        int attack = 0;
        int health = 0;
        int cost = 0;
        unsigned events = 0;
        unsigned invalidFields = 0;
        unsigned traitObservations = 0;
        std::uint64_t firstSignature = 0;
        std::uint64_t lastSignature = 0;
        Obj* lastComparedView = nullptr;
        bool compared = false;
    };
    inline std::array<OffboardDeltaEntry, 512> offboardDeltaEntries{};
    inline std::uint64_t offboardDeltaComparisons = 0;
    inline std::uint64_t offboardDeltaOverflow = 0;

    inline OffboardDeltaEntry* FindOffboardDelta(Obj* model, int id, bool create) {
        if (!model || id < 0) return nullptr;
        OffboardDeltaEntry* freeEntry = nullptr;
        for (auto& entry : offboardDeltaEntries) {
            if (entry.model == model && entry.cardId == id) return &entry;
            if (!entry.model && !freeEntry) freeEntry = &entry;
        }
        if (create && freeEntry) {
            *freeEntry = {};
            freeEntry->model = model;
            freeEntry->cardId = id;
            return freeEntry;
        }
        if (create && offboardDeltaOverflow++ < 8)
            LOG_ERR("CustomComponents/ZoneTriggeredAbilityPresentation/OFFBOARD_DELTA: table full model=%p card=%d (read-only)", model, id);
        return nullptr;
    }

    // Reports the recorded off-board deltas and the hypothetical full-value
    // gaps immediately before selective off-board correction.
    inline void CompareOffboardDelta(Obj* model, int id, Obj* view, Obj* frame,
                                      int gameplayAttack, const char* stage) {
        auto* entry = FindOffboardDelta(model, id, false);
        if (!entry || !frame || !frameCurrentAttack.IsValid()) return;
        if (entry->compared && entry->lastComparedView == view) return;
        const int beforeAttack = frameCurrentAttack[frame].Get();
        const std::int64_t fullAttack = static_cast<std::int64_t>(gameplayAttack) - beforeAttack;
        int fullHealth = -99999, fullCost = -99999;
        Obj* entity = entityExists.IsValid() && getEntityById.IsValid() && entityExists[model](id)
            ? getEntityById[model](id) : nullptr;
        if (entity) {
            Obj* health = TraceFindComponent(entity, "Health");
            if (health && healthMaxHealth.IsValid() && calculateModifiableValue.IsValid() && frameMaxHealth.IsValid()) {
                Obj* value = healthMaxHealth[health].Get();
                if (value) fullHealth = calculateModifiableValue(value) - frameMaxHealth[frame].Get();
            }
            Obj* cost = TraceFindComponent(entity, "SunCost");
            if (cost && sunCostValue.IsValid() && calculateModifiableValue.IsValid() && frameCurrentSunCost.IsValid()) {
                Obj* value = sunCostValue[cost].Get();
                if (value) {
                    int current = calculateModifiableValue(value);
                    if (current < 0) current = 0;
                    fullCost = current - frameCurrentSunCost[frame].Get();
                }
            }
        }
        if (++offboardDeltaComparisons <= 256)
            CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/OFFBOARD_DELTA: COMPARE stage=%s model=%p card=%d view=%p events=%u offboardAttack=%d offboardHealth=%d offboardCost=%d fullAttack=%lld fullHealth=%d fullCost=%d invalid=%u traitTypeChanges=%u traitSignature=%llx->%llx (Phase 2B selective sync)",
                stage, model, id, view, entry->events, entry->attack, entry->health,
                entry->cost, (long long)fullAttack, fullHealth, fullCost,
                entry->invalidFields, entry->traitObservations,
                (unsigned long long)entry->firstSignature,
                (unsigned long long)entry->lastSignature);
        entry->compared = true;
        entry->lastComparedView = view;
        // Keep the entry until a *new* off-board mutation arrives, at which
        // point the next off-board visit begins a fresh observation epoch.
    }

    // Only the gameplay deltas observed in Deck/Discard are
    // eligible for manual frame updates. A missing/invalid epoch means stock
    // presentation owns the result. Do not replay an epoch on the same view.
    inline int SelectOffboardFrameDelta(int pending, int gap) {
        // Never move the frame away from the currently authoritative gameplay
        // value. This also avoids replaying a delta stock has already shown.
        if (pending > 0 && gap > 0) return pending < gap ? pending : gap;
        if (pending < 0 && gap < 0) return pending > gap ? pending : gap;
        return 0;
    }

    inline void ReconcileVisibleAttack(Obj* view, const char* syncStage = "SHOW_IN_HAND") {
        if (!view || !cardViewEntityId.IsValid() || !cardViewFrame.IsValid() ||
            !frameCurrentAttack.IsValid() || !frameBuffAttack.IsValid() ||
            !componentCalculateAttack.IsValid() || !entityExists.IsValid() ||
            !getEntityById.IsValid()) return;
        const int id = cardViewEntityId[view].Get();
        Obj* model = nullptr;
        for (const auto& binding : frameViewBindings) {
            if (binding.view == view && binding.id == id) {
                model = binding.model;
                break;
            }
        }
        if (!model) return;
        bool tracked = false;
        for (const auto& pending : pendingFrameSyncs) {
            if (pending.model == model && pending.entityId == id) {
                tracked = true;
                break;
            }
        }
        const bool returnTicket = HasHandDeckReturn(model, id);
        if ((!tracked && !returnTicket) || !entityExists[model](id)) return;
        Obj* entity = getEntityById[model](id);
        if (!entity || !TraceFindComponent(entity, "Card")) return;
        Obj* attack = TraceFindComponent(entity, "Attack");
        Obj* frame = cardViewFrame[view].Get();
        if (!frame) return;
        // Only hand/deck-origin returned cards in an actual hand may use the
        // full model-to-frame gap. Lane->discard must remain stock-owned.
        const bool returnedToHand = returnTicket &&
            TraceFindComponent(entity, "HandElement") &&
            !TraceFindComponent(entity, "LaneElement") &&
            !TraceFindComponent(entity, "DiscardPileElement");
        // Source 3 is exclusively discard->lane. Consume the pre-trigger
        // snapshot on the newly mapped lane view; do not alter hand behavior.
        const bool returnedToLane = returnTicket &&
            TraceFindComponent(entity, "LaneElement") &&
            !TraceFindComponent(entity, "HandElement") &&
            !TraceFindComponent(entity, "DiscardPileElement") &&
            (IsDiscardToLaneOrigin(model, id) || HasLaneReturnSnapshot(model, id));
        if (!attack && !returnedToHand && !returnedToLane) return;
        const HandDeckReturnTicket snapshot = (returnedToHand || returnedToLane)
            ? ConsumeHandDeckReturnSnapshot(model, id, view) : HandDeckReturnTicket{};
        const int returnSource = snapshot.source;
        const bool fullReturn = returnSource == 1 || returnSource == 2 ||
            (returnSource == 3 && returnedToLane);
        // Two-stage return: restore ONLY the snapshot from before the
        // ReturnedFromDiscardPileTrigger. Any later trigger buff must be
        // presented by its own stock visual event, not by this correction.
        if (fullReturn) {
            if (returnSource == 3 && returnedToLane)
                RememberLaneVisualBaseline(view, model, id, snapshot);
            Obj* offboardEntity = entity;
            (void)offboardEntity;
            const int beforeAtk = frameCurrentAttack[frame].Get();
            const int beforeHp = frameMaxHealth.IsValid() ? frameMaxHealth[frame].Get() : -99999;
            const int beforeCost = frameCurrentSunCost.IsValid() ? frameCurrentSunCost[frame].Get() : -99999;
            int atkApplied = 0, hpApplied = 0, costApplied = 0;
            auto boundedGap = [](int target, int actual) -> int {
                if (target == -99999 || actual == -99999) return 0;
                const std::int64_t gap = static_cast<std::int64_t>(target) - actual;
                return gap >= -10000 && gap <= 10000 ? static_cast<int>(gap) : 0;
            };
            if (snapshot.attackBeforeTrigger != -99999 && frameBuffAttack.IsValid()) {
                atkApplied = boundedGap(snapshot.attackBeforeTrigger, beforeAtk);
                if (atkApplied) frameBuffAttack[frame](atkApplied);
            }
            if (snapshot.healthBeforeTrigger != -99999 &&
                snapshot.damageBeforeTrigger >= 0 &&
                snapshot.damageBeforeTrigger <= snapshot.healthBeforeTrigger &&
                frameMaxHealth.IsValid() && frameCurrentHealth.IsValid() &&
                frameBuffHealth.IsValid()) {
                const int current = frameCurrentHealth[frame].Get();
                // BuffHealth preserves current damage. Don't change a frame
                // whose damage state is inconsistent with the snapshot.
                if (current == beforeHp - snapshot.damageBeforeTrigger) {
                    hpApplied = boundedGap(snapshot.healthBeforeTrigger, beforeHp);
                    if (hpApplied) frameBuffHealth[frame](hpApplied);
                }
            }
            if (snapshot.costBeforeTrigger != -99999 &&
                frameCurrentSunCost.IsValid() && frameModifySunCost.IsValid()) {
                costApplied = boundedGap(snapshot.costBeforeTrigger, beforeCost);
                if (costApplied) frameModifySunCost[frame](costApplied);
            }
            CC_DIAG("CustomComponents/ZoneTriggeredAbilityPresentation/RETURN-STAT: PRETRIGGER_RESTORE id=%d source=%d view=%p snapshot=%d/%d cost=%d frameBefore=%d/%d cost=%d appliedAtk=%d appliedHp=%d appliedCost=%d frameAfter=%d/%d cost=%d (later triggers stock-owned)",
                id, returnSource, view, snapshot.attackBeforeTrigger,
                snapshot.healthBeforeTrigger, snapshot.costBeforeTrigger,
                beforeAtk, beforeHp, beforeCost, atkApplied, hpApplied, costApplied,
                frameCurrentAttack[frame].Get(),
                frameMaxHealth.IsValid() ? frameMaxHealth[frame].Get() : -99999,
                frameCurrentSunCost.IsValid() ? frameCurrentSunCost[frame].Get() : -99999);
            // Restore only abilities that existed BEFORE this return trigger.
            // Later GrantAbilityEffect presentation is still owned by stock.
            if (snapshot.traitSnapshotValid && traitSyncReady &&
                frameSetSpecialAbilitySprites.IsValid() && stateClass.IsValid()) {
                Obj* state = stateClass.CreateNewInstance();
                if (state) {
                    stateCtor[state]();
                    Obj* abilities = stateAbilities[state].Get();
                    if (abilities) {
                        auto add = BNM::Class(abilities).GetMethod("Add", 1).cast<void>();
                        if (add.IsValid()) {
                            for (unsigned i = 0; i < snapshot.traitCountBeforeTrigger; ++i)
                                add[abilities](snapshot.traitsBeforeTrigger[i]);
                            frameSetSpecialAbilitySprites[frame](state);
                            CC_DIAG("CustomComponents/ZoneTriggeredAbilityPresentation/RETURN-TRAIT: PRETRIGGER_RESTORE id=%d source=%d view=%p count=%u (later grants stock-owned)",
                                id, returnSource, view, snapshot.traitCountBeforeTrigger);
                        }
                    }
                }
            }
            // Old off-board deltas cannot be replayed onto another view.
            if (auto* oldDelta = FindOffboardDelta(model, id, false)) *oldDelta = {};
            return;
        }
        const int gameplay = attack ? componentCalculateAttack[attack]() : -99999;
        auto* offboard = FindOffboardDelta(model, id, false);
        const bool alreadyApplied = offboard && offboard->compared &&
            offboard->lastComparedView == view;
        const bool safeEpoch = !fullReturn && offboard && offboard->events > 0 &&
            offboard->invalidFields == 0 && !alreadyApplied;
        const int pendingAttack = safeEpoch ? offboard->attack : 0;
        const int pendingHealth = safeEpoch ? offboard->health : 0;
        const int pendingCost = safeEpoch ? offboard->cost : 0;
        if (attack) CompareOffboardDelta(model, id, view, frame, gameplay, syncStage);
        // This view consumed the entire authoritative gap. Retire any older
        // off-board mutation epoch now, so a future CardView for the same
        // entity (e.g. after playing to a lane) cannot replay it again.
        if (fullReturn && offboard) *offboard = {};
        // A return correction REPLACES selective off-board replay for this
        // view: never apply both paths. Use the remaining gap, not the total
        // historical buff amount. A second ShowInHand cannot replay the ticket.
        const int before = frameCurrentAttack[frame].Get();
        const std::int64_t wideDelta = attack
            ? static_cast<std::int64_t>(gameplay) - before : 0;
        if (wideDelta < -10000 || wideDelta > 10000) {
            LOG_ERR("CustomComponents/ZoneTriggeredAbilityPresentation/CARDSTAT_SYNC: INVALID_DELTA card=%d gameplay=%d frame=%d", id, gameplay, before);
            return;
        }
        const int delta = fullReturn ? static_cast<int>(wideDelta)
            : SelectOffboardFrameDelta(pendingAttack, static_cast<int>(wideDelta));
        if (attack && delta != 0) frameBuffAttack[frame](delta);
        if (fullReturn) CC_DIAG("CustomComponents/ZoneTriggeredAbilityPresentation/RETURN-STAT: stage=%s id=%d source=%d view=%p attack=%d frameBefore=%d applied=%d frameAfter=%d (single full-gap reconciliation)",
            syncStage, id, returnSource, view, gameplay, before, delta, frameCurrentAttack[frame].Get());
        CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/OFFBOARD_DELTA: APPLY stage=%s card=%d view=%p eligible=%d alreadyApplied=%d attackPending=%d attackGap=%lld attackApplied=%d healthPending=%d costPending=%d (stock owns all other changes)",
                 syncStage, id, view, int(safeEpoch), int(alreadyApplied), pendingAttack,
                 (long long)wideDelta, delta, pendingHealth, pendingCost);
        const int after = frameCurrentAttack[frame].Get();
        CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/CARDSTAT_SYNC: %s card=%d view=%p gameplayAttack=%d frameBefore=%d appliedDelta=%d frameAfter=%d corrected=%d",
                 syncStage, id, view, gameplay, before, delta, after, int(after == gameplay));

        // Numerical safety checks still use the authoritative active component
        // values; only recorded off-board deltas may change the frame.
        // BuffHealth adjusts BOTH MaxHealth and CurrentHealth by the same
        // delta, preserving damage already taken. Never set Health directly.
        CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/GENERIC_SYNC: NUMERIC_READY card=%d calc=%d healthFields=%d healthMethod=%d costFields=%d costMethod=%d",
                 id, int(calculateModifiableValue.IsValid()),
                 int(healthMaxHealth.IsValid() && healthCurrentDamage.IsValid() && frameMaxHealth.IsValid() && frameCurrentHealth.IsValid()),
                 int(frameBuffHealth.IsValid()),
                 int(sunCostValue.IsValid() && frameCurrentSunCost.IsValid()),
                 int(frameModifySunCost.IsValid()));
        Obj* health = TraceFindComponent(entity, "Health");
        if (health && healthMaxHealth.IsValid() && healthCurrentDamage.IsValid() &&
            calculateModifiableValue.IsValid() && frameMaxHealth.IsValid() &&
            frameCurrentHealth.IsValid() && frameBuffHealth.IsValid()) {
            Obj* value = healthMaxHealth[health].Get();
            if (value) {
                CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/GENERIC_SYNC: HEALTH_CALC_ENTER card=%d modInt=%p", id, value);
                const int authoritativeMax = calculateModifiableValue(value);
                CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/GENERIC_SYNC: HEALTH_CALC_EXIT card=%d max=%d", id, authoritativeMax);
                const int damage = healthCurrentDamage[health].Get();
                const int displayedMax = frameMaxHealth[frame].Get();
                const int displayedCurrent = frameCurrentHealth[frame].Get();
                const std::int64_t difference = static_cast<std::int64_t>(authoritativeMax) - displayedMax;
                // Don't risk healing/damaging unexpectedly if the frame's
                // health doesn't match the model's current-damage semantics.
                if (authoritativeMax >= 0 && damage >= 0 && damage <= authoritativeMax &&
                    difference >= -10000 && difference <= 10000 &&
                    displayedCurrent == displayedMax - damage) {
                    const int selective = fullReturn ? static_cast<int>(difference)
                        : SelectOffboardFrameDelta(pendingHealth, static_cast<int>(difference));
                    if (selective) frameBuffHealth[frame](selective);
                    CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/OFFBOARD_DELTA: HEALTH_APPLY card=%d fullGap=%lld pending=%d applied=%d", id, (long long)difference, pendingHealth, selective);
                    CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/GENERIC_SYNC: HEALTH card=%d max=%d damage=%d beforeMax=%d afterMax=%d afterCurrent=%d",
                        id, authoritativeMax, damage, displayedMax,
                        frameMaxHealth[frame].Get(), frameCurrentHealth[frame].Get());
                } else {
                    CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/GENERIC_SYNC: HEALTH_SKIP card=%d max=%d damage=%d frameMax=%d frameCurrent=%d",
                        id, authoritativeMax, damage, displayedMax, displayedCurrent);
                }
            }
        }
        if (!health) CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/GENERIC_SYNC: HEALTH_MISSING card=%d", id);
        Obj* cost = TraceFindComponent(entity, "SunCost");
        if (cost && sunCostValue.IsValid() && calculateModifiableValue.IsValid() &&
            frameCurrentSunCost.IsValid() && frameModifySunCost.IsValid()) {
            Obj* value = sunCostValue[cost].Get();
            if (value) {
                CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/GENERIC_SYNC: COST_CALC_ENTER card=%d modInt=%p", id, value);
                int authoritative = calculateModifiableValue(value);
                CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/GENERIC_SYNC: COST_CALC_EXIT card=%d cost=%d", id, authoritative);
                if (authoritative < 0) authoritative = 0;
                const int displayed = frameCurrentSunCost[frame].Get();
                const std::int64_t difference = static_cast<std::int64_t>(authoritative) - displayed;
                if (difference >= -10000 && difference <= 10000) {
                    const int selective = fullReturn ? static_cast<int>(difference)
                        : SelectOffboardFrameDelta(pendingCost, static_cast<int>(difference));
                    if (selective) frameModifySunCost[frame](selective);
                    CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/OFFBOARD_DELTA: COST_APPLY card=%d fullGap=%lld pending=%d applied=%d", id, (long long)difference, pendingCost, selective);
                    CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/GENERIC_SYNC: COST card=%d authoritative=%d before=%d after=%d",
                        id, authoritative, displayed, frameCurrentSunCost[frame].Get());
                }
            }
        }
        if (!cost) CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/GENERIC_SYNC: COST_MISSING card=%d", id);
        if (fullReturn) CC_DIAG("CustomComponents/ZoneTriggeredAbilityPresentation/RETURN-STAT: FINAL id=%d source=%d view=%p attack=%d healthMax=%d healthCurrent=%d sunCost=%d (gameplay unchanged)",
            id, returnSource, view, frameCurrentAttack[frame].Get(),
            frameMaxHealth.IsValid() ? frameMaxHealth[frame].Get() : -99999,
            frameCurrentHealth.IsValid() ? frameCurrentHealth[frame].Get() : -99999,
            frameCurrentSunCost.IsValid() ? frameCurrentSunCost[frame].Get() : -99999);
        // Trait rebuild uses precisely the hook that GrantAbilityPresentation
        // already extends; do NOT add/remove icons manually or touch star icon.
        if (traitSyncReady && frameSetSpecialAbilitySprites.IsValid()) {
            Obj* state = stateClass.CreateNewInstance();
            Obj* checker = checkerClass.CreateNewInstance();
            if (state && checker) {
                stateCtor[state]();
                checkerCtor[checker]();
                Obj* abilities = stateAbilities[state].Get();
                if (abilities) {
                    auto list = BNM::Class(abilities);
                    auto count = list.GetMethod("get_Count", 0).cast<int>();
                    const int beforeCount = count.IsValid() ? count[abilities]() : -1;
                    // Calls the GrantAbilityPresentation::AddSpecialAbilitiesHook,
                    // which incorporates active custom registry abilities.
                    checkerAddSpecialAbilities[checker](abilities, entity);
                    const int afterCount = count.IsValid() ? count[abilities]() : -1;
                    frameSetSpecialAbilitySprites[frame](state);
                    CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/GENERIC_SYNC: TRAITS card=%d listBefore=%d listAfter=%d nativeRebuild=1 grantBridge=1",
                        id, beforeCount, afterCount);
                }
            }
        }
    }

    static void AddViewObjectTraceHook(void* self, int entityId, Obj* view, const void* method) {
        Obj* mapper = static_cast<Obj*>(self);
        Obj* model = mapper && viewMapperEntityModel.IsValid() ? viewMapperEntityModel[mapper].Get() : nullptr;
        // I.10B: the frontend mapper is already active when stock views are
        // registered. Capture it here, before any discard-to-lane preflight;
        // waiting for CreateCardAnimation.OnStarted was too late. This uses
        // the existing read-only hook and a scene-scoped GC root.
        Core::SceneViewContext::Capture(mapper, model);
        CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/VIEWTRACE: AddViewObject ENTER mapper=%p model=%p entity=%d view=%p type=%s",
                 self, model, entityId, view, TraceObjectType(view));
        // Do not assume every ViewObject is CardViewComponent.
        if (view && std::strcmp(TraceObjectType(view), "CardViewComponent") == 0)
            TraceFrameForCard(view, "AddViewObject BEFORE", entityId, model);
        originalAddViewObject(self, entityId, view, method);
        CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/VIEWTRACE: AddViewObject EXIT mapper=%p entity=%d view=%p",
                 self, entityId, view);
        if (view && std::strcmp(TraceObjectType(view), "CardViewComponent") == 0) {
            BindFrameView(view, model, entityId);
            TraceFrameForCard(view, "AddViewObject AFTER", entityId, model);
        }
    }

    static void ShowInHandTraceHook(void* self, const void* method) {
        Obj* view = static_cast<Obj*>(self);
        if (view) TraceFrameForCard(view, "ShowInHand BEFORE");
        originalShowInHand(self, method);
        // Stock hand presentation completes first. Never change gameplay Attack.
        if (view) ReconcileVisibleAttack(view);
        if (view) TraceFrameForCard(view, "ShowInHand AFTER");
    }



