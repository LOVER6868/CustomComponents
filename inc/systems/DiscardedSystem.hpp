#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <mutex>

#include <BNM/BasicMonoStructures.hpp>
#include <BNM/Class.hpp>
#include <BNM/Field.hpp>
#include <BNM/Method.hpp>

#include "components/Discarded.hpp"
#include "core/CustomComponentRegistry.hpp"
#include "systems/CustomFieldBridgeSystem.hpp"
#include "systems/ZoneTriggeredAbilityPresentationSystem.hpp"
#include "log.h"

namespace CustomComponents::Systems::DiscardedSystem {

    using Obj = BNM::IL2CPP::Il2CppObject;
    template<typename T> using MonoList = BNM::Structures::Mono::List<T>;
    using Source = Components::DiscardSource;

    inline BNM::Field<MonoList<Obj*>*> modelEntities;
    inline BNM::Field<MonoList<Obj*>*> entityComponents;
    inline BNM::Field<int> entityId;
    inline BNM::Field<Obj*> resolverModel;
    inline BNM::Field<Obj*> resolverEffectQueue;
    inline BNM::Field<Obj*> triggeredEffectTriggerData;
    inline BNM::Field<int> triggerDataEffectEntityId;
    inline BNM::Field<int> triggerDataSourceId;
    inline BNM::Field<Obj*> triggerDataContext;
    inline BNM::Field<int> contextTargetId;
    inline BNM::Field<MonoList<int>*> discardPileIds;
    inline BNM::Field<int> discardPileAssociationId;
    inline BNM::Method<Obj*> entityGetOrAdd;
    inline BNM::Method<Obj*> entityGet;
    inline BNM::Method<Obj*> entityRemove;
    inline BNM::Method<bool> entityHas;
    inline BNM::Method<bool> queueIsEmpty;
    // The engine drives EffectCollectionProcessor via EffectQueue.HasNext().
    // Observe that boundary, not only ProcessEffects callbacks.
    inline BNM::Field<MonoList<Obj*>*> queueEffects;
    inline BNM::Field<MonoList<Obj*>*> queueResponses;
    inline BNM::Field<MonoList<Obj*>*> queueGroupResponses;
    // Effect-group fields verified against the ARM64 dump:
    // EffectQueue.CurrentGroupId +0x28, NextGroupId +0x2C;
    // Effect.EffectGroupId +0x3C. Observation only.
    inline BNM::Field<int> queueCurrentGroupId;
    inline BNM::Field<int> queueNextGroupId;
    inline BNM::Field<int> effectGroupId;
    inline BNM::Field<int> effectSourceId;
    inline BNM::Field<int> effectEntityId;
    inline BNM::Field<int> effectTargetId;
    // Injected EntityModel fields of the stock trigger/buff processors.
    inline BNM::Field<Obj*> startSystemModel;
    inline BNM::Field<Obj*> buffSystemModel;
    inline BNM::Field<Obj*> endSystemModel;
    inline bool groupMetadataReady = false;
    inline BNM::Method<bool> triggeredHasTriggers;
    inline BNM::Method<int> playerIdForCard;
    inline BNM::MonoType* discardedType = nullptr;
    inline BNM::Field<int> discardedSource;

    inline bool ready = false;

    inline Obj* FindEntity(Obj* model, int id) {
        if (!model || id < 0 || !modelEntities.IsValid()) return nullptr;
        auto* list = modelEntities[model].Get();
        if (!list || !list->items) return nullptr;
        for (int i = 0; i < list->size; ++i) {
            Obj* e = list->items->m_Items[i];
            if (e && entityId[e].Get() == id) return e;
        }
        return nullptr;
    }

    inline Obj* NamedComponent(Obj* entity, const char* name) {
        if (!entity || !name) return nullptr;
        auto* list = entityComponents[entity].Get();
        if (!list || !list->items) return nullptr;
        for (int i = 0; i < list->size; ++i) {
            Obj* item = list->items->m_Items[i];
            if (!item) continue;
            auto* klass = BNM::Class(item).GetClass();
            if (klass && klass->name && std::strcmp(klass->name, name) == 0)
                return item;
        }
        return nullptr;
    }

    inline bool InDiscard(Obj* model, int cardId) {
        if (!ready || !model || cardId < 0) return false;
        Obj* card = FindEntity(model, cardId);
        if (!card) return false;
        const int playerId = playerIdForCard[static_cast<Obj*>(nullptr)](card);
        Obj* player = FindEntity(model, playerId);
        Obj* assoc = NamedComponent(player, "DiscardPileAssociation");
        if (!assoc) return false;
        Obj* pile = FindEntity(model, discardPileAssociationId[assoc].Get());
        Obj* component = NamedComponent(pile, "DiscardPile");
        if (!component) return false;
        auto* ids = discardPileIds[component].Get();
        if (!ids || !ids->items) return false;
        for (int i = 0; i < ids->size; ++i)
            if (ids->items->m_Items[i] == cardId) return true;
        return false;
    }

    inline int GetSource(Obj* model, int cardId) {
        if (!ready) return 0;
        Obj* card = FindEntity(model, cardId);
        if (!card || !entityHas[card](discardedType)) return 0;
        Obj* marker = entityGet[card](discardedType);
        return marker ? discardedSource[marker].Get() : 0;
    }

    // Only tag a SUCCESSFUL transition into a real discard pile.
    inline bool Mark(Obj* model, int cardId, Source source) {
        if ((source != Source::Hand && source != Source::Deck) ||
            !ready || !InDiscard(model, cardId)) return false;
        Obj* card = FindEntity(model, cardId);
        Obj* marker = card ? entityGetOrAdd[card](discardedType) : nullptr;
        if (!marker) {
            LOG_ERR("CustomComponents/Discarded: add failed model=%p card=%d", model, cardId);
            return false;
        }
        discardedSource[marker] = static_cast<int>(source);
        LOG_INFO("CustomComponents/Discarded: SET model=%p card=%d Source=%d", model, cardId, static_cast<int>(source));
        return true;
    }

    // The event is broadcast to exact listener effect entities. Retain their
    // identities, not raw TriggerData pointers: stock copies/returns those to
    // the pool before its resolver runs.
    struct Listener {
        int effectEntityId = -1;
        int sourceId = -1;
        bool resolved = false;
        // An exact, simple stock ability chain may be cleaned up once its
        // end effect is dispatched AND the processor confirms completion.
        // More complicated chains retain the conservative whole-queue fallback.
        bool chainCaptured = false;
        bool chainUnsafe = false;
        int startCount = 0;
        int buffCount = 0;
        int endCount = 0;
        int otherCount = 0;
        Obj* exactEndEffect = nullptr; // compared only before stock pooling
        int exactEndGroup = -1;
        bool startHandled = false;
        bool buffHandled = false;
        bool endHandled = false;
        bool endShifted = false;
        bool endCompleted = false;
    };
    struct Pending {
        Obj* model = nullptr;
        Obj* queue = nullptr;
        Obj* triggeredCollection = nullptr;
        int cardId = -1;
        bool firingComplete = false;
        bool overflow = false;
        int count = 0;
        unsigned idleReports = 0;
        // Only limits read-only diagnostics; does not change cleanup state.
        std::array<unsigned, 10> diagReports{};
        unsigned groupShiftReports = 0;
        unsigned chainReports = 0; // stock-handler trace budget
        int lastShiftedEndListener = -1; // only valid until the next Shift
        std::uint64_t shiftSerial = 0;
        int lastObservedGroup = -1;
        int lastSeededEffectCount = -1;
        int seedGroupCount = 0;
        bool seedGroupOverflow = false;
        std::array<int, 24> seedGroups{};
        std::array<Listener, 64> listeners{};
    };
    inline std::array<Pending, 128> pending{};
    inline std::mutex pendingMutex;
    // HasNext() is a very hot game-engine method. Avoid the bookkeeping mutex
    // unless there is at least one outstanding return to observe.
    inline std::atomic<bool> hasPendingReturn{false};

    inline void UpdatePendingFlagLocked() {
        bool active = false;
        for (const auto& p : pending) {
            if (p.model) { active = true; break; }
        }
        hasPendingReturn.store(active, std::memory_order_release);
    }

    inline void Remove(Obj* model, int cardId, const char* why) {
        if (!ready) return;
        // If a returned card was discarded again while its old trigger chain
        // was resolving, this marker describes the NEW discard. Keep it.
        if (InDiscard(model, cardId)) {
            LOG_INFO("CustomComponents/Discarded: KEEP model=%p card=%d reason=re-discarded", model, cardId);
            return;
        }
        Obj* card = FindEntity(model, cardId);
        if (!card || !entityHas[card](discardedType)) return;
        const int origin = GetSource(model, cardId);
        Obj* removed = entityRemove[card](discardedType);
        LOG_INFO("CustomComponents/Discarded: REMOVE model=%p card=%d Source=%d reason=%s removed=%p remaining=%d",
            model, cardId, origin, why ? why : "?", removed,
            entityHas[card](discardedType) ? 1 : 0);
    }

    inline void BeginReturn(Obj* model, int cardId, Obj* triggeredCollection) {
        if (!ready || !model || cardId < 0 || !GetSource(model, cardId)) return;
        std::lock_guard<std::mutex> lock(pendingMutex);
        for (auto& p : pending) {
            if (p.model == model && p.cardId == cardId) {
                LOG_INFO("CustomComponents/Discarded: replacing prior pending return model=%p card=%d", model, cardId);
                p = {};
                p.model = model;
                p.cardId = cardId;
                p.triggeredCollection = triggeredCollection;
                hasPendingReturn.store(true, std::memory_order_release);
                return;
            }
        }
        for (auto& p : pending) {
            if (!p.model) {
                p.model = model;
                p.cardId = cardId;
                p.triggeredCollection = triggeredCollection;
                hasPendingReturn.store(true, std::memory_order_release);
                return;
            }
        }
        LOG_ERR("CustomComponents/Discarded: pending return table full model=%p card=%d (marker retained)", model, cardId);
    }

    inline void ReturnListenerQueued(Obj* model, int cardId, int effectEntityId, int listenerSourceId) {
        std::lock_guard<std::mutex> lock(pendingMutex);
        for (auto& p : pending) {
            if (p.model != model || p.cardId != cardId || p.firingComplete) continue;
            if (p.count >= static_cast<int>(p.listeners.size())) {
                p.overflow = true;
                return;
            }
            p.listeners[p.count++] = {effectEntityId, listenerSourceId, false};
            return;
        }
    }

    inline void FinishReturn(Obj* model, int cardId) {
        bool removeNow = false;
        {
            std::lock_guard<std::mutex> lock(pendingMutex);
            for (auto& p : pending) {
                if (p.model != model || p.cardId != cardId) continue;
                p.firingComplete = true;
                LOG_INFO("CustomComponents/Discarded: RETURN-FIRED model=%p card=%d Source=%d listeners=%d overflow=%d",
                    model, cardId, GetSource(model, cardId), p.count, p.overflow ? 1 : 0);
                if (!p.overflow && p.count == 0) {
                    p = {};
                    UpdatePendingFlagLocked();
                    removeNow = true;
                }
                break;
            }
        }
        if (removeNow) Remove(model, cardId, "no-return-listeners");
    }

    using ProcessFn = void (*)(void*, Obj*, const void*);
    inline ProcessFn originalResolver = nullptr;
    inline ProcessFn originalProcessEffects = nullptr;
    inline ProcessFn originalCompleteEffect = nullptr;

    // Attempt #3: read-only diagnostic helpers.  The resolver field is declared
    // EffectCollection (interface), so do not read concrete EffectQueue fields
    // unless the runtime class actually identifies itself as EffectQueue.
    inline const char* DiagnosticType(Obj* object) {
        if (!object) return "null";
        auto* klass = BNM::Class(object).GetClass();
        return klass && klass->name ? klass->name : "unknown";
    }

    inline int DiagnosticListSize(BNM::Field<MonoList<Obj*>*>& field,
                                  Obj* candidate) {
        if (!candidate || !field.IsValid() ||
            std::strcmp(DiagnosticType(candidate), "EffectQueue") != 0)
            return -1; // unknown: interface implementation not verified
        auto* list = field[candidate].Get();
        return list ? list->size : -1;
    }

    // Snapshot queue contents immediately before a listener resolves. We only
    // compare object identities during this one synchronous resolver call:
    // effects may be returned to the game's pool after processing.
    struct QueueSnapshot {
        std::array<Obj*, 384> objects{};
        int count = 0;
        int currentGroup = -1;
        int nextGroup = -1;
        bool overflow = false;
    };

    inline bool CanInspectGroups(Obj* queue) {
        return groupMetadataReady && queue &&
            std::strcmp(DiagnosticType(queue), "EffectQueue") == 0;
    }

    inline int CurrentGroup(Obj* queue) {
        return CanInspectGroups(queue) ? queueCurrentGroupId[queue].Get() : -1;
    }

    inline int NextGroup(Obj* queue) {
        return CanInspectGroups(queue) ? queueNextGroupId[queue].Get() : -1;
    }

    inline int EffectGroup(Obj* effect) {
        return effect && groupMetadataReady ? effectGroupId[effect].Get() : -1;
    }

    inline Obj* QueueHead(Obj* queue) {
        if (!CanInspectGroups(queue)) return nullptr;
        auto* list = queueEffects[queue].Get();
        return list && list->items && list->size > 0
            ? list->items->m_Items[0] : nullptr;
    }

    inline QueueSnapshot CaptureQueue(Obj* queue) {
        QueueSnapshot result{};
        if (!CanInspectGroups(queue)) return result;
        result.currentGroup = CurrentGroup(queue);
        result.nextGroup = NextGroup(queue);
        BNM::Field<MonoList<Obj*>*>* lists[] = {
            &queueEffects, &queueResponses, &queueGroupResponses
        };
        for (auto* field : lists) {
            if (!field->IsValid()) continue;
            auto* list = (*field)[queue].Get();
            if (!list || !list->items) continue;
            for (int i = 0; i < list->size; ++i) {
                if (result.count == static_cast<int>(result.objects.size())) {
                    result.overflow = true;
                    break;
                }
                result.objects[result.count++] = list->items->m_Items[i];
            }
        }
        return result;
    }

    inline bool WasInSnapshot(const QueueSnapshot& snapshot, Obj* effect) {
        for (int i = 0; i < snapshot.count; ++i)
            if (snapshot.objects[i] == effect) return true;
        return false;
    }

    inline void AddSeedGroup(Pending& pendingReturn, int group) {
        if (group < 0) return;
        for (int i = 0; i < pendingReturn.seedGroupCount; ++i)
            if (pendingReturn.seedGroups[i] == group) return;
        if (pendingReturn.seedGroupCount <
            static_cast<int>(pendingReturn.seedGroups.size()))
            pendingReturn.seedGroups[pendingReturn.seedGroupCount++] = group;
        else pendingReturn.seedGroupOverflow = true;
    }

    inline int CountSeedGroupEffects(const Pending& p) {
        if (!CanInspectGroups(p.queue) || p.seedGroupCount == 0) return -1;
        int count = 0;
        BNM::Field<MonoList<Obj*>*>* lists[] = {
            &queueEffects, &queueResponses, &queueGroupResponses
        };
        for (auto* field : lists) {
            if (!field->IsValid()) continue;
            auto* list = (*field)[p.queue].Get();
            if (!list || !list->items) continue;
            for (int i = 0; i < list->size; ++i) {
                Obj* effect = list->items->m_Items[i];
                if (!effect) continue;
                const int group = EffectGroup(effect);
                for (int j = 0; j < p.seedGroupCount; ++j) {
                    if (p.seedGroups[j] == group) { ++count; break; }
                }
            }
        }
        return count;
    }

    inline void RecordNewListenerEffects(Pending& p,
                                         const QueueSnapshot& previous,
                                         Listener& listener) {
        if (!CanInspectGroups(p.queue)) return;
        const QueueSnapshot after = CaptureQueue(p.queue);
        listener.chainCaptured = true;
        listener.chainUnsafe = previous.overflow || after.overflow;
        int added = 0, printed = 0;
        for (int i = 0; i < after.count; ++i) {
            Obj* effect = after.objects[i];
            if (!effect || WasInSnapshot(previous, effect)) continue;
            ++added;
            const int group = EffectGroup(effect);
            AddSeedGroup(p, group);
            const int source = effectSourceId[effect].Get();
            const int entity = effectEntityId[effect].Get();
            const char* type = DiagnosticType(effect);
            // The group alone is NOT ownership: an unrelated effect was
            // observed in the same group in attempt #5.
            if (source != listener.sourceId || entity != listener.effectEntityId) {
                listener.chainUnsafe = true;
            } else if (std::strcmp(type, "TriggeredAbilityStartEffect") == 0) {
                ++listener.startCount;
            } else if (std::strcmp(type, "BuffEffect") == 0) {
                ++listener.buffCount;
            } else if (std::strcmp(type, "TriggeredAbilityEndEffect") == 0) {
                ++listener.endCount;
                listener.exactEndEffect = effect;
                listener.exactEndGroup = group;
            } else {
                ++listener.otherCount;
            }
            if (printed++ < 12)
                LOG_INFO("CustomComponents/Discarded/GROUP: LISTENER-NEW card=%d effect=%p type=%s group=%d source=%d effectEntity=%d",
                    p.cardId, effect, type, group, source, entity);
        }
        // A strict three-effect chain is the ONLY new accelerated path.
        // Zero/multiple end effects, other effect types, truncated snapshots
        // or unknown identities are handled by the original v2 fallback.
        if (added != 3 || listener.startCount != 1 ||
            listener.buffCount != 1 || listener.endCount != 1 ||
            listener.otherCount != 0 || !listener.exactEndEffect ||
            p.seedGroupOverflow) listener.chainUnsafe = true;
        LOG_INFO("CustomComponents/Discarded/GROUP: LISTENER-SNAPSHOT card=%d queue=%p current=%d->%d next=%d->%d objects=%d->%d added=%d seedGroups=%d overflow=%d",
            p.cardId, p.queue, previous.currentGroup, after.currentGroup,
            previous.nextGroup, after.nextGroup, previous.count, after.count,
            added, p.seedGroupCount, p.seedGroupOverflow ? 1 : 0);
        LOG_INFO("CustomComponents/Discarded/CHAIN: CAPTURE card=%d listener=%d start=%d buff=%d end=%d other=%d exactEnd=%p group=%d eligible=%d",
            p.cardId, listener.effectEntityId, listener.startCount,
            listener.buffCount, listener.endCount, listener.otherCount,
            listener.exactEndEffect, listener.exactEndGroup,
            listener.chainUnsafe ? 0 : 1);
        p.lastSeededEffectCount = CountSeedGroupEffects(p);
        if (added > 12)
            LOG_INFO("CustomComponents/Discarded/GROUP: LISTENER-NEW-TRUNCATED card=%d omitted=%d",
                p.cardId, added - 12);
    }

    enum class DiagnosticBoundary : unsigned {
        ProcessorEnter = 0,
        ProcessorExit = 1,
        CompleteEnter = 2,
        CompleteExit = 3,
        HasNext = 4
    };

    // Track which *actual collection instance* was processed/completed, not
    // just whether the resolver's saved queue is empty. Rate limited per return.
    inline void TraceProcessing(DiagnosticBoundary boundary, Obj* observed,
                                int hasNext = -1) {
        if (!ready || !hasPendingReturn.load(std::memory_order_acquire)) return;
        static constexpr const char* names[] = {
            "PROCESS-ENTER", "PROCESS-EXIT", "COMPLETE-ENTER",
            "COMPLETE-EXIT", "HASNEXT"
        };
        std::lock_guard<std::mutex> lock(pendingMutex);
        for (auto& p : pending) {
            if (!p.model || !p.firingComplete || !p.queue || p.count == 0)
                continue;
            const bool same = observed == p.queue;
            // Separate budgets for matching and nonmatching collections.
            const unsigned index = static_cast<unsigned>(boundary) * 2 + (same ? 0U : 1U);
            const unsigned cap = same ? 8U : 3U;
            if (p.diagReports[index]++ >= cap) continue;
            bool allResolved = !p.overflow;
            for (int i = 0; i < p.count; ++i)
                if (!p.listeners[i].resolved) allResolved = false;
            LOG_INFO("CustomComponents/Discarded/DIAG: %s model=%p card=%d seen=%p seenType=%s resolverQueue=%p queueType=%s same=%d hasNext=%d allResolved=%d active=%d responses=%d groupResponses=%d currentGroup=%d nextGroup=%d headGroup=%d seededQueued=%d",
                names[static_cast<unsigned>(boundary)], p.model, p.cardId,
                observed, DiagnosticType(observed), p.queue, DiagnosticType(p.queue),
                same ? 1 : 0, hasNext, allResolved ? 1 : 0,
                DiagnosticListSize(queueEffects, p.queue),
                DiagnosticListSize(queueResponses, p.queue),
                DiagnosticListSize(queueGroupResponses, p.queue),
                CurrentGroup(p.queue), NextGroup(p.queue),
                EffectGroup(QueueHead(p.queue)), CountSeedGroupEffects(p));
        }
    }

    static void ResolverHook(void* self, Obj* effect, const void* method) {
        Obj* model = self && resolverModel.IsValid()
            ? resolverModel[static_cast<Obj*>(self)].Get() : nullptr;
        Obj* queue = self && resolverEffectQueue.IsValid()
            ? resolverEffectQueue[static_cast<Obj*>(self)].Get() : nullptr;
        Obj* data = effect && triggeredEffectTriggerData.IsValid()
            ? triggeredEffectTriggerData[effect].Get() : nullptr;
        const int effectEntity = data ? triggerDataEffectEntityId[data].Get() : -1;
        const int sourceId = data ? triggerDataSourceId[data].Get() : -1;
        Obj* context = data ? triggerDataContext[data].Get() : nullptr;
        const int cardId = context ? contextTargetId[context].Get() : -1;

        const QueueSnapshot before =
            (hasPendingReturn.load(std::memory_order_acquire) && cardId >= 0)
                ? CaptureQueue(queue) : QueueSnapshot{};
        originalResolver(self, effect, method);

        if (!model || !queue || cardId < 0) return;
        std::lock_guard<std::mutex> lock(pendingMutex);
        for (auto& p : pending) {
            if (p.model != model || p.cardId != cardId || !p.firingComplete) continue;
            for (int i = 0; i < p.count; ++i) {
                auto& listener = p.listeners[i];
                if (listener.resolved || listener.effectEntityId != effectEntity ||
                    listener.sourceId != sourceId) continue;
                listener.resolved = true;
                p.queue = queue;
                RecordNewListenerEffects(p, before, listener);
                LOG_INFO("CustomComponents/Discarded: RETURN-LISTENER-RESOLVED model=%p card=%d listener=%d source=%d queue=%p",
                    model, cardId, effectEntity, sourceId, queue);
                LOG_INFO("CustomComponents/Discarded/DIAG: RESOLVER-QUEUE card=%d queue=%p type=%s active=%d responses=%d groupResponses=%d",
                    cardId, queue, DiagnosticType(queue),
                    DiagnosticListSize(queueEffects, queue),
                    DiagnosticListSize(queueResponses, queue),
                    DiagnosticListSize(queueGroupResponses, queue));
                break;
            }
        }
    }

    // Attempt #5: trace the ACTUAL stock TriggeredAbilityStart, Buff and
    // TriggeredAbilityEnd handlers. The game dump identifies the methods as
    // Core.Systems.{TriggeredAbilityStart,Buff,TriggeredAbilityEnd}System.
    // Their processing is distinct from the collection's CompleteEffect and
    // from frontend animation completion. All probes are read-only.
    using StockHandlerFn = void (*)(void*, Obj*, const void*);
    inline StockHandlerFn originalStartHandler = nullptr;
    inline StockHandlerFn originalBuffHandler = nullptr;
    inline StockHandlerFn originalEndHandler = nullptr;

    struct HandlerProbe {
        Obj* model = nullptr;
        Obj* queue = nullptr;
        int cardId = -1;
        int sourceId = -1;
        int effectEntityId = -1;
        QueueSnapshot before{};
    };

    inline bool MatchesReturnEffect(const Pending& p, int source, int entity,
                                    int group) {
        if (!p.model || !p.firingComplete || p.overflow || !p.queue ||
            p.count == 0 || source != p.cardId) return false;
        bool listenerEntity = false;
        for (int i = 0; i < p.count; ++i)
            if (p.listeners[i].resolved && p.listeners[i].effectEntityId == entity &&
                p.listeners[i].sourceId == source) listenerEntity = true;
        bool seededGroup = false;
        for (int i = 0; i < p.seedGroupCount; ++i)
            if (p.seedGroups[i] == group) seededGroup = true;
        // Require both identities if they are known. If a descendant is
        // assigned another group, that must be investigated explicitly.
        return listenerEntity || seededGroup;
    }

    // The queue snapshot is valid only across the SAME synchronous call.
    // The engine pools Effect instances, so never retain those identities
    // beyond the handler's return or use them for automatic cleanup.
    inline int BeginHandlerProbes(const char* name, Obj* model, Obj* effect,
                                  std::array<HandlerProbe, 2>& probes) {
        if (!ready || !model || !effect || !groupMetadataReady ||
            !hasPendingReturn.load(std::memory_order_acquire)) return 0;
        const int source = effectSourceId[effect].Get();
        const int target = effectTargetId[effect].Get();
        const int entity = effectEntityId[effect].Get();
        const int group = EffectGroup(effect);
        int found = 0;
        std::lock_guard<std::mutex> lock(pendingMutex);
        for (auto& p : pending) {
            if (p.model != model || !MatchesReturnEffect(p, source, entity, group))
                continue;
            if (p.chainReports++ >= 36) continue;
            bool seededGroup = false;
            for (int i = 0; i < p.seedGroupCount; ++i)
                if (p.seedGroups[i] == group) seededGroup = true;
            LOG_INFO("CustomComponents/Discarded/CHAIN: %s-ENTER card=%d effect=%p type=%s source=%d target=%d effectEntity=%d group=%d seededGroup=%d current=%d next=%d active=%d responses=%d groupResponses=%d",
                name, p.cardId, effect, DiagnosticType(effect), source, target,
                entity, group, seededGroup ? 1 : 0, CurrentGroup(p.queue),
                NextGroup(p.queue), DiagnosticListSize(queueEffects, p.queue),
                DiagnosticListSize(queueResponses, p.queue),
                DiagnosticListSize(queueGroupResponses, p.queue));
            if (found >= static_cast<int>(probes.size())) {
                LOG_INFO("CustomComponents/Discarded/CHAIN: %s-PROBE-LIMIT card=%d (observed only)", name, p.cardId);
                continue;
            }
            auto& probe = probes[found++];
            probe.model = p.model;
            probe.cardId = p.cardId;
            probe.sourceId = source;
            probe.effectEntityId = entity;
            probe.queue = p.queue;
            probe.before = CaptureQueue(p.queue);
        }
        return found;
    }

    inline void EndHandlerProbes(const char* name,
                                const std::array<HandlerProbe, 2>& probes,
                                int count) {
        if (!ready || !groupMetadataReady) return;
        std::lock_guard<std::mutex> lock(pendingMutex);
        for (int i = 0; i < count; ++i) {
            const auto& probe = probes[i];
            bool stillTracked = false;
            for (const auto& p : pending)
                if (p.model == probe.model && p.cardId == probe.cardId &&
                    p.queue == probe.queue) { stillTracked = true; break; }
            if (!stillTracked) continue;
            const QueueSnapshot after = CaptureQueue(probe.queue);
            int added = 0, shown = 0;
            for (int j = 0; j < after.count; ++j) {
                Obj* candidate = after.objects[j];
                if (!candidate || WasInSnapshot(probe.before, candidate)) continue;
                ++added;
                if (shown++ < 8)
                    LOG_INFO("CustomComponents/Discarded/CHAIN: %s-QUEUED card=%d effect=%p type=%s source=%d target=%d effectEntity=%d group=%d",
                        name, probe.cardId, candidate, DiagnosticType(candidate),
                        effectSourceId[candidate].Get(), effectTargetId[candidate].Get(),
                        effectEntityId[candidate].Get(), EffectGroup(candidate));
            }
            for (auto& p : pending) {
                if (p.model != probe.model || p.cardId != probe.cardId ||
                    p.queue != probe.queue) continue;
                for (int k = 0; k < p.count; ++k) {
                    auto& listener = p.listeners[k];
                    if (!listener.resolved ||
                        listener.effectEntityId != probe.effectEntityId ||
                        listener.sourceId != probe.sourceId) continue;
                    if (added || probe.before.overflow || after.overflow)
                        listener.chainUnsafe = true;
                    if (std::strcmp(name, "START") == 0) listener.startHandled = true;
                    if (std::strcmp(name, "BUFF") == 0) listener.buffHandled = true;
                    if (std::strcmp(name, "END") == 0) listener.endHandled = true;
                }
            }
            LOG_INFO("CustomComponents/Discarded/CHAIN: %s-EXIT card=%d queue=%p current=%d->%d next=%d->%d queued=%d snapshot=%d->%d active=%d responses=%d groupResponses=%d (sync handler only)",
                name, probe.cardId, probe.queue,
                probe.before.currentGroup, after.currentGroup,
                probe.before.nextGroup, after.nextGroup, added,
                probe.before.count, after.count,
                DiagnosticListSize(queueEffects, probe.queue),
                DiagnosticListSize(queueResponses, probe.queue),
                DiagnosticListSize(queueGroupResponses, probe.queue));
        }
    }

    static void StartHandlerHook(void* self, Obj* effect, const void* method) {
        std::array<HandlerProbe, 2> probes{};
        Obj* model = self && startSystemModel.IsValid()
            ? startSystemModel[static_cast<Obj*>(self)].Get() : nullptr;
        const int count = BeginHandlerProbes("START", model, effect, probes);
        originalStartHandler(self, effect, method);
        if (count) EndHandlerProbes("START", probes, count);
    }

    static void BuffHandlerHook(void* self, Obj* effect, const void* method) {
        std::array<HandlerProbe, 2> probes{};
        Obj* model = self && buffSystemModel.IsValid()
            ? buffSystemModel[static_cast<Obj*>(self)].Get() : nullptr;
        const int count = BeginHandlerProbes("BUFF", model, effect, probes);
        ZoneTriggeredAbilityPresentationSystem::AuditOwnedEffect(0, self, effect, [&] {
            originalBuffHandler(self, effect, method);
        });
        if (count) EndHandlerProbes("BUFF", probes, count);
    }

    static void EndHandlerHook(void* self, Obj* effect, const void* method) {
        std::array<HandlerProbe, 2> probes{};
        Obj* model = self && endSystemModel.IsValid()
            ? endSystemModel[static_cast<Obj*>(self)].Get() : nullptr;
        const int count = BeginHandlerProbes("END", model, effect, probes);
        originalEndHandler(self, effect, method);
        if (count) EndHandlerProbes("END", probes, count);
    }

    // The exact listener's other effects (including deferred responses) must
    // also be gone; unrelated effects in the same group are ignored.
    inline bool HasQueuedListenerWork(const Pending& p, const Listener& listener) {
        if (!CanInspectGroups(p.queue) || !queueEffects.IsValid() ||
            !queueResponses.IsValid() || !queueGroupResponses.IsValid())
            return true; // unknown is not safe
        BNM::Field<MonoList<Obj*>*>* lists[] = {
            &queueEffects, &queueResponses, &queueGroupResponses
        };
        for (auto* field : lists) {
            auto* list = (*field)[p.queue].Get();
            if (!list) return true;
            if (!list->items) {
                if (list->size != 0) return true;
                continue;
            }
            for (int i = 0; i < list->size; ++i) {
                Obj* effect = list->items->m_Items[i];
                if (effect && effectSourceId[effect].Get() == listener.sourceId &&
                    effectEntityId[effect].Get() == listener.effectEntityId)
                    return true;
            }
        }
        return false;
    }

    // Attempt #6: a STRICTLY matched Start -> Buff -> End stock chain, with
    // no synchronously spawned effects, can finish once the engine has called
    // CompleteEffect for its exact End and returned from ProcessEffects.
    // Unknown/complex chains continue using the original whole-queue check.
    inline void CheckFinishedSimpleChains(Obj* observedQueue) {
        if (!ready || !observedQueue || !originalCompleteEffect ||
            !hasPendingReturn.load(std::memory_order_acquire)) return;
        struct ToRemove { Obj* model; int cardId; };
        std::array<ToRemove, 128> cleanup{};
        int count = 0;
        {
            std::lock_guard<std::mutex> lock(pendingMutex);
            for (auto& p : pending) {
                if (!p.model || p.queue != observedQueue || !p.firingComplete ||
                    p.overflow || p.count <= 0 || !CanInspectGroups(p.queue)) continue;
                bool finished = true;
                for (int i = 0; i < p.count; ++i) {
                    const auto& l = p.listeners[i];
                    if (!l.resolved || !l.chainCaptured || l.chainUnsafe ||
                        l.startCount != 1 || l.buffCount != 1 || l.endCount != 1 ||
                        l.otherCount != 0 || !l.startHandled || !l.buffHandled ||
                        !l.endHandled || !l.endShifted || !l.endCompleted ||
                        HasQueuedListenerWork(p, l))
                        finished = false;
                }
                if (!finished) continue;
                LOG_INFO("CustomComponents/Discarded/CHAIN: RETURN-CHAIN-DRAINED model=%p card=%d queue=%p listeners=%d active=%d (exact completed End; shared queue not required to drain)",
                    p.model, p.cardId, p.queue, p.count,
                    DiagnosticListSize(queueEffects, p.queue));
                if (count < static_cast<int>(cleanup.size())) {
                    cleanup[count++] = {p.model, p.cardId};
                    p = {};
                }
            }
            UpdatePendingFlagLocked();
        }
        for (int i = 0; i < count; ++i)
            Remove(cleanup[i].model, cleanup[i].cardId,
                   "return-listener-end-completed");
    }

    // A return listener may resolve inside a processing pass, while response
    // effects are still queued. The FALLBACK waits for all queue groups.
    // Important: never remove while a resolver/processor still has work queued.
    inline void CheckFinishedReturns(Obj* observedQueue, const char* boundary,
                                      bool reportBlocked) {
        if (!ready || !queueIsEmpty.IsValid() ||
            !hasPendingReturn.load(std::memory_order_acquire)) return;
        struct ToRemove { Obj* model; int cardId; };
        std::array<ToRemove, 128> cleanup{};
        int count = 0;
        {
            std::lock_guard<std::mutex> lock(pendingMutex);
            for (auto& p : pending) {
                if (!p.model || !p.firingComplete || p.overflow || !p.queue ||
                    (observedQueue && observedQueue != p.queue)) continue;
                bool allResolved = p.count > 0;
                for (int i = 0; i < p.count; ++i)
                    if (!p.listeners[i].resolved) allResolved = false;
                if (!allResolved) continue;

                if (!queueIsEmpty[p.queue]()) {
                    if (reportBlocked && p.idleReports++ < 3) {
                        const auto sizeOf = [&](BNM::Field<MonoList<Obj*>*>& field) -> int {
                            if (!field.IsValid()) return -1;
                            auto* list = field[p.queue].Get();
                            return list ? list->size : -1;
                        };
                        LOG_INFO("CustomComponents/Discarded: RETURN-WAIT model=%p card=%d boundary=%s queue=%p active=%d responses=%d groupResponses=%d",
                            p.model, p.cardId, boundary, p.queue,
                            sizeOf(queueEffects), sizeOf(queueResponses),
                            sizeOf(queueGroupResponses));
                    }
                    continue;
                }
                if (count < static_cast<int>(cleanup.size())) {
                    LOG_INFO("CustomComponents/Discarded: RETURN-DRAINED model=%p card=%d boundary=%s queue=%p listeners=%d",
                        p.model, p.cardId, boundary, p.queue, p.count);
                    cleanup[count++] = {p.model, p.cardId};
                    p = {};
                }
            }
            UpdatePendingFlagLocked();
        }
        // Component mutation happens outside the bookkeeping lock.
        for (int i = 0; i < count; ++i)
            Remove(cleanup[i].model, cleanup[i].cardId, "return-effects-drained");
    }

    using ShiftFn = Obj* (*)(void*, const void*);
    inline ShiftFn originalQueueShift = nullptr;

    static Obj* QueueShiftHook(void* self, const void* method) {
        // Shift() is called for every effect. Avoid reflection altogether
        // when no return is pending (same hot-path principle as HasNext).
        if (!hasPendingReturn.load(std::memory_order_acquire))
            return originalQueueShift(self, method);
        Obj* queue = static_cast<Obj*>(self);
        Obj* formerHead = QueueHead(queue);
        const int formerGroup = EffectGroup(formerHead);
        const int formerCurrent = CurrentGroup(queue);
        Obj* shifted = originalQueueShift(self, method);
        if (!hasPendingReturn.load(std::memory_order_acquire)) return shifted;
        std::lock_guard<std::mutex> lock(pendingMutex);
        for (auto& p : pending) {
            if (!p.model || p.queue != queue || !p.firingComplete) continue;
            ++p.shiftSerial;
            p.lastShiftedEndListener = -1;
            // Check the exact effect IN THE SAME Shift call; the engine
            // returns effects to its pool, so addresses must never be
            // consulted later without live identity validation.
            if (shifted && groupMetadataReady) {
                const int shiftedSource = effectSourceId[shifted].Get();
                const int shiftedEntity = effectEntityId[shifted].Get();
                const int shiftedGroup = EffectGroup(shifted);
                const char* shiftedType = DiagnosticType(shifted);
                for (int k = 0; k < p.count; ++k) {
                    auto& listener = p.listeners[k];
                    if (listener.chainUnsafe || !listener.chainCaptured ||
                        shifted != listener.exactEndEffect ||
                        shiftedSource != listener.sourceId ||
                        shiftedEntity != listener.effectEntityId ||
                        shiftedGroup != listener.exactEndGroup ||
                        std::strcmp(shiftedType, "TriggeredAbilityEndEffect") != 0)
                        continue;
                    listener.endShifted = true;
                    p.lastShiftedEndListener = k;
                    LOG_INFO("CustomComponents/Discarded/CHAIN: EXACT-END-SHIFT card=%d listener=%d queue=%p group=%d",
                        p.cardId, listener.effectEntityId, queue, shiftedGroup);
                    break;
                }
            }
            const int current = CurrentGroup(queue);
            const int seedEffects = CountSeedGroupEffects(p);
            if (p.lastSeededEffectCount > 0 && seedEffects == 0)
                LOG_INFO("CustomComponents/Discarded/GROUP: SEEDS-DRAINED card=%d queue=%p current=%d next=%d (observation only; later child groups may remain)",
                    p.cardId, queue, current, NextGroup(queue));
            p.lastSeededEffectCount = seedEffects;
            const bool changed = current != p.lastObservedGroup;
            // Retain transition reports even after the first 32 shifts.
            if (p.groupShiftReports < 32 ||
                (changed && p.groupShiftReports < 128)) {
                LOG_INFO("CustomComponents/Discarded/GROUP: SHIFT card=%d queue=%p shifted=%p shiftedGroup=%d headBefore=%d current=%d->%d next=%d newHeadGroup=%d seedsQueued=%d transitions=%d",
                    p.cardId, queue, shifted, EffectGroup(shifted),
                    formerGroup, formerCurrent, current, NextGroup(queue),
                    EffectGroup(QueueHead(queue)), seedEffects,
                    changed ? 1 : 0);
            }
            ++p.groupShiftReports;
            p.lastObservedGroup = current;
        }
        return shifted;
    }

    using HasNextFn = bool (*)(void*, const void*);
    inline HasNextFn originalQueueHasNext = nullptr;

    static bool QueueHasNextHook(void* self, const void* method) {
        const bool hasNext = originalQueueHasNext(self, method);
        TraceProcessing(DiagnosticBoundary::HasNext,
                        static_cast<Obj*>(self), hasNext ? 1 : 0);
        // ProcessEffects loops while HasNext is true. Its final false result
        // occurs AFTER processing the last effect in this queue, even if no
        // further ProcessEffects call follows. Confirm IsEmpty as well, so a
        // deferred response group never loses access to Discarded.Source.
        if (!hasNext && self)
            CheckFinishedReturns(static_cast<Obj*>(self), "queue-has-next-false", true);
        return hasNext;
    }

    static void ProcessEffectsHook(void* self, Obj* effects, const void* method) {
        TraceProcessing(DiagnosticBoundary::ProcessorEnter, effects);
        originalProcessEffects(self, effects, method);
        TraceProcessing(DiagnosticBoundary::ProcessorExit, effects);
        // The enclosing stock processor has returned, so completed listener
        // End effects are no longer executing on this call stack.
        CheckFinishedSimpleChains(effects);
        // Fallback remains unchanged for zero/unknown/complex effect chains.
        CheckFinishedReturns(nullptr, "processor-exit", false);
    }

    static void CompleteEffectHook(void* self, Obj* effects, const void* method) {
        TraceProcessing(DiagnosticBoundary::CompleteEnter, effects);
        // Verified in stock ARM64: EffectCollectionProcessor.CompleteEffect
        // invokes EffectCollection.CompleteEffect, which calls Shift() DURING
        // this callback, not before it. Capture the shift serial before/after
        // to associate completion with precisely the effect removed here.
        struct CompletionToken {
            Obj* model;
            int cardId;
            std::uint64_t shiftBefore;
        };
        std::array<CompletionToken, 128> tokens{};
        int count = 0;
        if (effects && hasPendingReturn.load(std::memory_order_acquire)) {
            std::lock_guard<std::mutex> lock(pendingMutex);
            for (const auto& p : pending) {
                if (!p.model || !p.firingComplete || p.queue != effects ||
                    p.overflow || count >= static_cast<int>(tokens.size())) continue;
                tokens[count++] = {p.model, p.cardId, p.shiftSerial};
            }
        }
        originalCompleteEffect(self, effects, method);
        if (count) {
            std::lock_guard<std::mutex> lock(pendingMutex);
            for (int i = 0; i < count; ++i) {
                const auto& token = tokens[i];
                for (auto& p : pending) {
                    if (p.model != token.model || p.cardId != token.cardId ||
                        p.queue != effects || p.shiftSerial != token.shiftBefore + 1 ||
                        p.lastShiftedEndListener < 0) continue;
                    const int k = p.lastShiftedEndListener;
                    if (k >= p.count) continue;
                    auto& listener = p.listeners[k];
                    if (listener.chainUnsafe || !listener.endShifted ||
                        !listener.endHandled || listener.endCompleted) continue;
                    listener.endCompleted = true;
                    LOG_INFO("CustomComponents/Discarded/CHAIN: EXACT-END-COMPLETE card=%d listener=%d queue=%p shiftSerial=%llu",
                        p.cardId, listener.effectEntityId, effects,
                        static_cast<unsigned long long>(p.shiftSerial));
                }
            }
        }
        TraceProcessing(DiagnosticBoundary::CompleteExit, effects);
        // Do not remove here: another listener may still be resolving in the
        // enclosing ProcessEffects pass. Cleanup runs at processor exit.
    }

    inline bool Initialize() {
        if (ready) return true;
        if (!CustomComponentRegistry::FindByName("PvZCards.Engine.Components", "Discarded")) {
            if (!CustomComponentRegistry::Register({
                    "Discarded", "PvZCards.Engine.Components", "EngineLib.dll",
                    "MarkerComponent", "PvZCards.Engine.Components", "EngineLib.dll"})) {
                LOG_ERR("CustomComponents/Discarded: component registration failed");
                return false;
            }
        }
        if (!CustomFieldBridgeSystem::Register(
                "PvZCards.Engine.Components", "Discarded",
                "PvZCards.Engine.Components", "MarkerComponent",
                {{"Source", CustomFieldBridgeSystem::FieldKind::Int32}},
                false)) return false;

        auto model = BNM::Class("PvZCards.Engine", "EntityModel");
        auto entity = BNM::Class("PvZCards.Engine", "Entity");
        auto discardPile = BNM::Class("PvZCards.Engine.Components", "DiscardPile");
        auto discardAssoc = BNM::Class("PvZCards.Engine.Components", "DiscardPileAssociation");
        auto discarded = BNM::Class("PvZCards.Engine.Components", "Discarded");
        auto resolver = BNM::Class("PvZCards.Core.Systems", "TriggeredEffectResolverSystem");
        auto startSystem = BNM::Class("PvZCards.Core.Systems", "TriggeredAbilityStartSystem");
        auto buffSystem = BNM::Class("PvZCards.Core.Systems", "BuffSystem");
        auto endSystem = BNM::Class("PvZCards.Core.Systems", "TriggeredAbilityEndSystem");
        auto processor = BNM::Class("PvZCards.Engine", "EffectCollectionProcessor");
        auto triggered = BNM::Class("PvZCards.Engine.Effects", "TriggeredEffect");
        auto triggerData = BNM::Class("PvZCards.Engine", "TriggerData");
        auto context = BNM::Class("PvZCards.Engine", "EffectContext");
        auto queue = BNM::Class("PvZCards.Engine", "EffectQueue");
        auto effectClass = BNM::Class("PvZCards.Engine", "Effect");
        auto collection = BNM::Class("PvZCards.Engine", "TriggeredEffectCollection");
        auto playerUtils = BNM::Class("PvZCards.Core.Utilities", "PlayerUtils");
        if (!model.IsValid() || !entity.IsValid() ||
            !discardPile.IsValid() || !discardAssoc.IsValid() || !discarded.IsValid() ||
            !resolver.IsValid() || !processor.IsValid() ||
            !triggered.IsValid() || !triggerData.IsValid() || !context.IsValid() ||
            !queue.IsValid() || !collection.IsValid() || !playerUtils.IsValid()) return false;

        modelEntities = model.GetField("Entities");
        entityComponents = entity.GetField("Components");
        entityId = entity.GetField("<Id>k__BackingField");
        resolverModel = resolver.GetField("<Model>k__BackingField");
        resolverEffectQueue = resolver.GetField("<EffectQueue>k__BackingField");
        triggeredEffectTriggerData = triggered.GetField("TriggerData");
        triggerDataEffectEntityId = triggerData.GetField("EffectEntityId");
        triggerDataSourceId = triggerData.GetField("SourceId");
        triggerDataContext = triggerData.GetField("Context");
        contextTargetId = context.GetField("TargetId");
        discardPileIds = discardPile.GetField("CardIds");
        discardPileAssociationId = discardAssoc.GetField("DiscardPileId");
        discardedSource = discarded.GetField("Source");
        discardedType = discarded.GetMonoType();
        entityGetOrAdd = entity.GetMethod("GetOrAddComponent", 1).cast<Obj*>();
        entityGet = entity.GetMethod("GetComponent", 1).cast<Obj*>();
        entityRemove = entity.GetMethod("RemoveComponent", 1).cast<Obj*>();
        entityHas = entity.GetMethod("HasComponent", 1).cast<bool>();
        queueIsEmpty = queue.GetMethod("IsEmpty", 0).cast<bool>();
        queueEffects = queue.GetField("Effects");
        queueResponses = queue.GetField("ResponseEffects");
        queueGroupResponses = queue.GetField("GroupResponseEffects");
        if (effectClass.IsValid()) {
            queueCurrentGroupId = queue.GetField("CurrentGroupId");
            queueNextGroupId = queue.GetField("NextGroupId");
            effectGroupId = effectClass.GetField("EffectGroupId");
            effectSourceId = effectClass.GetField("SourceId");
            effectEntityId = effectClass.GetField("EffectEntityId");
            effectTargetId = effectClass.GetField("TargetId");
            groupMetadataReady = queueCurrentGroupId.IsValid() &&
                queueNextGroupId.IsValid() && effectGroupId.IsValid() &&
                effectSourceId.IsValid() && effectEntityId.IsValid() &&
                effectTargetId.IsValid();
        }
        triggeredHasTriggers = collection.GetMethod("HasTriggers", 0).cast<bool>();
        playerIdForCard = playerUtils.GetMethod("GetPlayerIdFromCard", 1).cast<int>();

        if (!modelEntities.IsValid() || !entityComponents.IsValid() || !entityId.IsValid() ||
            !resolverModel.IsValid() ||
            !resolverEffectQueue.IsValid() || !triggeredEffectTriggerData.IsValid() ||
            !triggerDataEffectEntityId.IsValid() || !triggerDataSourceId.IsValid() ||
            !triggerDataContext.IsValid() || !contextTargetId.IsValid() ||
            !discardPileIds.IsValid() || !discardPileAssociationId.IsValid() ||
            !discardedSource.IsValid() || !discardedType || !entityGetOrAdd.IsValid() ||
            !entityGet.IsValid() || !entityRemove.IsValid() || !entityHas.IsValid() ||
            !queueIsEmpty.IsValid() || !triggeredHasTriggers.IsValid() ||
            !playerIdForCard.IsValid()) {
            LOG_ERR("CustomComponents/Discarded: required metadata missing");
            return false;
        }

        // New stock handlers are OPTIONAL: cleanup v2 remains unchanged if
        // their metadata is absent or hooking cannot be installed.
        if (startSystem.IsValid())
            startSystemModel = startSystem.GetField("<Entities>k__BackingField");
        if (buffSystem.IsValid())
            buffSystemModel = buffSystem.GetField("<Entities>k__BackingField");
        if (endSystem.IsValid())
            endSystemModel = endSystem.GetField("<Model>k__BackingField");
        auto startMethod = startSystem.IsValid()
            ? startSystem.GetMethod("ProcessEffect", 1) : BNM::MethodBase{};
        auto buffMethod = buffSystem.IsValid()
            ? buffSystem.GetMethod("ProcessEffect", 1) : BNM::MethodBase{};
        auto endMethod = endSystem.IsValid()
            ? endSystem.GetMethod("ProcessEffect", 1) : BNM::MethodBase{};
        auto resolverMethod = resolver.GetMethod("ProcessEffect", 1);
        auto hasNextMethod = queue.GetMethod("HasNext", 0);
        auto shiftMethod = queue.GetMethod("Shift", 0);
        auto processEffectsMethod = processor.GetMethod("ProcessEffects", 1);
        auto completeEffectMethod = processor.GetMethod("CompleteEffect", 1);
        BNM::BasicHook(resolverMethod, ResolverHook, originalResolver);
        BNM::BasicHook(hasNextMethod, QueueHasNextHook, originalQueueHasNext);
        // Shift is diagnostic-only and optional; the existing cleanup behavior
        // is unchanged if this entry point cannot be hooked.
        if (groupMetadataReady && shiftMethod.IsValid())
            BNM::BasicHook(shiftMethod, QueueShiftHook, originalQueueShift);
        BNM::BasicHook(processEffectsMethod, ProcessEffectsHook, originalProcessEffects);
        // An optional *read-only* hook to expose the processor's completion
        // boundary. If unavailable the base v2 lifecycle still initializes.
        if (completeEffectMethod.IsValid())
            BNM::BasicHook(completeEffectMethod, CompleteEffectHook, originalCompleteEffect);
        if (groupMetadataReady && startSystemModel.IsValid() && startMethod.IsValid())
            BNM::BasicHook(startMethod, StartHandlerHook, originalStartHandler);
        if (groupMetadataReady && buffSystemModel.IsValid() && buffMethod.IsValid())
            BNM::BasicHook(buffMethod, BuffHandlerHook, originalBuffHandler);
        if (groupMetadataReady && endSystemModel.IsValid() && endMethod.IsValid())
            BNM::BasicHook(endMethod, EndHandlerHook, originalEndHandler);
        LOG_INFO("CustomComponents/Discarded/CHAIN: stock handlers START=%d BUFF=%d END=%d (read-only; RVAs 0x21FC764 / 0x21FD444 / 0x21F2034)",
            originalStartHandler ? 1 : 0,
            originalBuffHandler ? 1 : 0,
            originalEndHandler ? 1 : 0);
        LOG_INFO("CustomComponents/Discarded/DIAG: ProcessEffects hooked=%d CompleteEffect hooked=%d HasNext hooked=%d",
            originalProcessEffects ? 1 : 0,
            originalCompleteEffect ? 1 : 0,
            originalQueueHasNext ? 1 : 0);
        if (!originalResolver || !originalQueueHasNext || !originalProcessEffects) {
            LOG_ERR("CustomComponents/Discarded: stock hook installation failed");
            return false;
        }
        ready = true;
        LOG_INFO("CustomComponents/Discarded/GROUP: metadata=%d Shift hooked=%d (RVA 0x227FBDC; read-only group lineage diagnostics)",
            groupMetadataReady ? 1 : 0, originalQueueShift ? 1 : 0);
        LOG_INFO("CustomComponents/Discarded: ready (Hand/Deck only; attempt 6 exact stock Start/Buff/End completion, v2 fallback)");
        return true;
    }
} // namespace CustomComponents::Systems::DiscardedSystem
