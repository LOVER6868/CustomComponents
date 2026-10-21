#include "diagnostics/DiagnosticLogging.hpp"
    // Read-only diagnostic helpers. The resolver field is declared
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
                if constexpr (false) CC_DIAG("CustomComponents/Discarded/GROUP: LISTENER-NEW card=%d effect=%p type=%s group=%d source=%d effectEntity=%d",
                    p.cardId, effect, type, group, source, entity);
        }
        // A strict three-effect chain is the ONLY new accelerated path.
        // Zero/multiple end effects, other effect types, truncated snapshots
        // or unknown identities are handled by the original v2 fallback.
        if (added != 3 || listener.startCount != 1 ||
            listener.buffCount != 1 || listener.endCount != 1 ||
            listener.otherCount != 0 || !listener.exactEndEffect ||
            p.seedGroupOverflow) listener.chainUnsafe = true;
        if constexpr (false) CC_DIAG("CustomComponents/Discarded/GROUP: LISTENER-SNAPSHOT card=%d queue=%p current=%d->%d next=%d->%d objects=%d->%d added=%d seedGroups=%d overflow=%d",
            p.cardId, p.queue, previous.currentGroup, after.currentGroup,
            previous.nextGroup, after.nextGroup, previous.count, after.count,
            added, p.seedGroupCount, p.seedGroupOverflow ? 1 : 0);
        CC_DIAG("CustomComponents/Discarded/CHAIN: CAPTURE card=%d listener=%d start=%d buff=%d end=%d other=%d exactEnd=%p group=%d eligible=%d",
            p.cardId, listener.effectEntityId, listener.startCount,
            listener.buffCount, listener.endCount, listener.otherCount,
            listener.exactEndEffect, listener.exactEndGroup,
            listener.chainUnsafe ? 0 : 1);
        p.lastSeededEffectCount = CountSeedGroupEffects(p);
        if (added > 12)
            if constexpr (false) CC_DIAG("CustomComponents/Discarded/GROUP: LISTENER-NEW-TRUNCATED card=%d omitted=%d",
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
            if constexpr (false) CC_DIAG("CustomComponents/Discarded/DIAG: %s model=%p card=%d seen=%p seenType=%s resolverQueue=%p queueType=%s same=%d hasNext=%d allResolved=%d active=%d responses=%d groupResponses=%d currentGroup=%d nextGroup=%d headGroup=%d seededQueued=%d",
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
                CC_DIAG("CustomComponents/Discarded: RETURN-LISTENER-RESOLVED model=%p card=%d listener=%d source=%d queue=%p",
                    model, cardId, effectEntity, sourceId, queue);
                if constexpr (false) CC_DIAG("CustomComponents/Discarded/DIAG: RESOLVER-QUEUE card=%d queue=%p type=%s active=%d responses=%d groupResponses=%d",
                    cardId, queue, DiagnosticType(queue),
                    DiagnosticListSize(queueEffects, queue),
                    DiagnosticListSize(queueResponses, queue),
                    DiagnosticListSize(queueGroupResponses, queue));
                break;
            }
        }
    }

    // Observe the stock TriggeredAbilityStart, Buff and
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
            CC_DIAG("CustomComponents/Discarded/CHAIN: %s-ENTER card=%d effect=%p type=%s source=%d target=%d effectEntity=%d group=%d seededGroup=%d current=%d next=%d active=%d responses=%d groupResponses=%d",
                name, p.cardId, effect, DiagnosticType(effect), source, target,
                entity, group, seededGroup ? 1 : 0, CurrentGroup(p.queue),
                NextGroup(p.queue), DiagnosticListSize(queueEffects, p.queue),
                DiagnosticListSize(queueResponses, p.queue),
                DiagnosticListSize(queueGroupResponses, p.queue));
            if (found >= static_cast<int>(probes.size())) {
                CC_DIAG("CustomComponents/Discarded/CHAIN: %s-PROBE-LIMIT card=%d (observed only)", name, p.cardId);
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
                    CC_DIAG("CustomComponents/Discarded/CHAIN: %s-QUEUED card=%d effect=%p type=%s source=%d target=%d effectEntity=%d group=%d",
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
            CC_DIAG("CustomComponents/Discarded/CHAIN: %s-EXIT card=%d queue=%p current=%d->%d next=%d->%d queued=%d snapshot=%d->%d active=%d responses=%d groupResponses=%d (sync handler only)",
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

    // A strictly matched Start -> Buff -> End stock chain, with
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
                CC_DIAG("CustomComponents/Discarded/CHAIN: RETURN-CHAIN-DRAINED model=%p card=%d queue=%p listeners=%d active=%d (exact completed End; shared queue not required to drain)",
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
                        CC_DIAG("CustomComponents/Discarded: RETURN-WAIT model=%p card=%d boundary=%s queue=%p active=%d responses=%d groupResponses=%d",
                            p.model, p.cardId, boundary, p.queue,
                            sizeOf(queueEffects), sizeOf(queueResponses),
                            sizeOf(queueGroupResponses));
                    }
                    continue;
                }
                if (count < static_cast<int>(cleanup.size())) {
                    CC_DIAG("CustomComponents/Discarded: RETURN-DRAINED model=%p card=%d boundary=%s queue=%p listeners=%d",
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
                    CC_DIAG("CustomComponents/Discarded/CHAIN: EXACT-END-SHIFT card=%d listener=%d queue=%p group=%d",
                        p.cardId, listener.effectEntityId, queue, shiftedGroup);
                    break;
                }
            }
            const int current = CurrentGroup(queue);
            const int seedEffects = CountSeedGroupEffects(p);
            if (p.lastSeededEffectCount > 0 && seedEffects == 0)
                if constexpr (false) CC_DIAG("CustomComponents/Discarded/GROUP: SEEDS-DRAINED card=%d queue=%p current=%d next=%d (observation only; later child groups may remain)",
                    p.cardId, queue, current, NextGroup(queue));
            p.lastSeededEffectCount = seedEffects;
            const bool changed = current != p.lastObservedGroup;
            // Retain transition reports even after the first 32 shifts.
            if (p.groupShiftReports < 32 ||
                (changed && p.groupShiftReports < 128)) {
                if constexpr (false) CC_DIAG("CustomComponents/Discarded/GROUP: SHIFT card=%d queue=%p shifted=%p shiftedGroup=%d headBefore=%d current=%d->%d next=%d newHeadGroup=%d seedsQueued=%d transitions=%d",
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
                    CC_DIAG("CustomComponents/Discarded/CHAIN: EXACT-END-COMPLETE card=%d listener=%d queue=%p shiftSerial=%llu",
                        p.cardId, listener.effectEntityId, effects,
                        static_cast<unsigned long long>(p.shiftSerial));
                }
            }
        }
        TraceProcessing(DiagnosticBoundary::CompleteExit, effects);
        // Do not remove here: another listener may still be resolving in the
        // enclosing ProcessEffects pass. Cleanup runs at processor exit.
    }

