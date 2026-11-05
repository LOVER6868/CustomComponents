// Internal implementation fragment for ZoneTriggeredAbilityPresentationSystem.
// Animation-queue observation and post-animation frame flush.
// Included at its original declaration position inside the owning namespace.
// Not a standalone public header; hook installation remains in the parent header.

    // Read-only list inspection, including the queue's FrontAnimations list.
    // Never mutate List<T> or animation state in diagnostic hooks.
    struct QueueListSnapshot {
        int count = -1;
        bool containsTarget = false;
    };

    inline QueueListSnapshot InspectQueueList(Obj* queue, BNM::Field<Obj*>& field) {
        QueueListSnapshot snapshot{};
        if (!queue || !field.IsValid()) return snapshot;
        Obj* listObject = field[queue].Get();
        if (!listObject) return snapshot;
        auto* list = reinterpret_cast<BNM::Structures::Mono::List<Obj*>*>(listObject);
        snapshot.count = list->GetSize();
        if (snapshot.count < 0 || snapshot.count > 4096 || !list->items)
            return snapshot;
        Obj** entries = list->GetData();
        if (!entries) return snapshot;
        for (int i = 0; i < snapshot.count; ++i) {
            if (entries[i] == lifecycleTargetAnimation) {
                snapshot.containsTarget = true;
                break;
            }
        }
        return snapshot;
    }

    inline void LogQueueLifecycle(Obj* queue, const char* phase, bool force = false) {
        if (!queue || queue != lifecycleTargetQueue || !lifecycleTargetAnimation)
            return;
        const auto normal = InspectQueueList(queue, queueAnimations);
        const auto front = InspectQueueList(queue, queueFrontAnimations);
        const auto signature =
            (static_cast<std::uint64_t>(static_cast<std::uint32_t>(normal.count)) << 32) ^
            (static_cast<std::uint64_t>(static_cast<std::uint16_t>(front.count)) << 16) ^
            (normal.containsTarget ? 2ull : 0ull) ^
            (front.containsTarget ? 1ull : 0ull);
        if (!force && signature == lifecycleQueueLastSignature &&
            lifecycleQueueUpdates > 12)
            return;
        lifecycleQueueLastSignature = signature;
        const int started = effectIsStarted.IsValid()
            ? static_cast<int>(effectIsStarted[lifecycleTargetAnimation].Get()) : -1;
        const int cancelled = effectIsCancelled.IsValid()
            ? static_cast<int>(effectIsCancelled[lifecycleTargetAnimation].Get()) : -1;
        CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/LIFECYCLE: %s queue=%p update=%llu normal=%d front=%d target=%p inNormal=%d inFront=%d started=%d cancelled=%d",
            phase, queue, static_cast<unsigned long long>(lifecycleQueueUpdates),
            normal.count, front.count, lifecycleTargetAnimation,
            static_cast<int>(normal.containsTarget), static_cast<int>(front.containsTarget),
            started, cancelled);
    }

    static void CheckForCompletionHook(void* self, const void* method) {
        if (self != lifecycleTargetAnimation) {
            originalCheckForCompletion(self, method);
            return;
        }
        CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/LIFECYCLE: CheckForCompletion ENTER animation=%p", self);
        originalCheckForCompletion(self, method);
        CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/LIFECYCLE: CheckForCompletion EXIT animation=%p", self);
        LogQueueLifecycle(lifecycleTargetQueue, "AFTER completion check", true);
    }

    // These are stock-initiated calls only. Never invoke animation methods from
    // the picker, and never hold gameplay progression in a diagnostic hook.
    static void QueueAddAnimationHook(void* self, Obj* animation, const void* method) {
        ::CustomComponents::Presentation::Pick3VisualQueueGate::ObserveEnqueue(self, animation, false);
        ::CustomComponents::Presentation::Pick3VisualBarrierTracker::Enqueue(self, animation);
        LOG_INFO("CustomComponents/Pick3/ANIMATION_LIFECYCLE: event=ENQUEUE queue=%p animation=%p front=0", self, animation);
        originalQueueAddAnimation(self, animation, method);
    }
    static void QueueAddFrontAnimationHook(void* self, Obj* animation, const void* method) {
        ::CustomComponents::Presentation::Pick3VisualQueueGate::ObserveEnqueue(self, animation, true);
        ::CustomComponents::Presentation::Pick3VisualBarrierTracker::Enqueue(self, animation);
        LOG_INFO("CustomComponents/Pick3/ANIMATION_LIFECYCLE: event=ENQUEUE queue=%p animation=%p front=1", self, animation);
        originalQueueAddFrontAnimation(self, animation, method);
    }
    static void QueueStartAnimationHook(void* self, Obj* animation, const void* method) {
        ::CustomComponents::Presentation::Pick3VisualQueueGate::ObserveStart(self, animation);
        ::CustomComponents::Presentation::Pick3VisualBarrierTracker::Start(self, animation);
        LOG_INFO("CustomComponents/Pick3/ANIMATION_LIFECYCLE: event=START queue=%p animation=%p", self, animation);
        originalQueueStartAnimation(self, animation, method);
    }

    static void QueueUpdateHook(void* self, const void* method) {
        Obj* queue = static_cast<Obj*>(self);
        const auto queueKey = reinterpret_cast<std::uintptr_t>(self);
        // Identify the owner BEFORE stock Update may retire its animation.
        // Never infer ownership from an arbitrary empty queue.
        for (auto& entry : pendingIdleLaneFrames) {
            if (!entry.model) continue;
            if (!entry.queue) {
                if (QueueContainsAnimation(queue, entry)) {
                    entry.queue = queueKey;
                    entry.queueProbes = 0;
                } else if (++entry.queueProbes > 18000) {
                    entry = {};  // Never retain an unclaimed old animation forever.
                }
            } else if (entry.queue == queueKey && !entry.completed &&
                       ++entry.queueProbes > 1800) {
                entry = {};  // A canceled creation cannot fill the table forever.
            }
        }
        const bool traced = self == lifecycleTargetQueue && lifecycleTargetAnimation;
        if (traced) {
            ++lifecycleQueueUpdates;
            LogQueueLifecycle(queue, "UPDATE BEFORE");
        }
        const auto previousQueue = activeQueueUpdate;
        activeQueueUpdate = queueKey;
        // Snapshots are taken around stock Update; a decreasing count signals
        // queue retirement, not necessarily visual completion of that animation.
        const auto beforeNormal = InspectQueueList(queue, queueAnimations);
        const auto beforeFront = InspectQueueList(queue, queueFrontAnimations);
        originalQueueUpdate(self, method);
        const auto afterNormal = InspectQueueList(queue, queueAnimations);
        const auto afterFront = InspectQueueList(queue, queueFrontAnimations);
        // Passive identities across both stock queue lists. Disappearance only
        // signals LEFT_LIST, never completed animation or permission to open UI.
        std::vector<std::uintptr_t> observedIdentities;
        for (auto* field : {&queueAnimations, &queueFrontAnimations}) {
            if (!field->IsValid()) continue;
            Obj* listObj = (*field)[queue].Get();
            if (!listObj) continue;
            auto* list = reinterpret_cast<BNM::Structures::Mono::List<Obj*>*>(listObj);
            const int size = list->GetSize();
            if (size < 0 || size > 4096) continue;
            Obj** values = list->GetData();
            if (!values) continue;
            for (int i = 0; i < size; ++i)
                if (values[i]) observedIdentities.push_back(reinterpret_cast<std::uintptr_t>(values[i]));
        }
        ::CustomComponents::Presentation::Pick3VisualBarrierTracker::ObserveList(self, observedIdentities);
        for (const auto& change : ::CustomComponents::Presentation::Pick3VisualBarrierTracker::CollectTransitions(self))
            LOG_INFO("CustomComponents/Pick3/VISUAL_BARRIER: event=%s effect=%d group=%d candidateQueue=%p remaining=%d ownership=%s policy=OBSERVE_ONLY",
                change.remaining == 0 ? "BARRIER_LIST_DRAINED" : "BARRIER_PENDING",
                change.effect, change.group, self, change.remaining,
                change.ownershipVerified ? "VERIFIED" : "UNKNOWN");
        ::CustomComponents::Presentation::Pick3VisualQueueGate::ObserveStockUpdate();
        if (beforeNormal.count != afterNormal.count || beforeFront.count != afterFront.count) {
            if ((beforeNormal.count >= 0 && afterNormal.count >= 0 && afterNormal.count < beforeNormal.count) ||
                (beforeFront.count >= 0 && afterFront.count >= 0 && afterFront.count < beforeFront.count))
                ::CustomComponents::Presentation::Pick3VisualQueueGate::retireTransitions.fetch_add(1, std::memory_order_relaxed);
            LOG_INFO("CustomComponents/Pick3/ANIMATION_LIFECYCLE: event=QUEUE_TRANSITION queue=%p normal=%d->%d front=%d->%d",
                self, beforeNormal.count, afterNormal.count, beforeFront.count, afterFront.count);
        }
        ::CustomComponents::Presentation::Pick3VisualQueueGate::normalCount.store(afterNormal.count, std::memory_order_relaxed);
        ::CustomComponents::Presentation::Pick3VisualQueueGate::frontCount.store(afterFront.count, std::memory_order_relaxed);
        activeQueueUpdate = previousQueue;
        if (traced) LogQueueLifecycle(queue, "UPDATE AFTER");
        FlushCompletedIdleLaneFrames(queue);
        // CRASH FIX: Do not invoke CanYield during Queue.Update.
        // The matching PvP crash (libil2cpp.so+0x17C7EE0, null+0x38)
        // occurred through a BNM::TryInvoke callback on UnityMain while
        // opening the mulligan. Merely having a Method handle does not
        // establish that every queue instance/context is safe to invoke.
        // Disable this experimental visual gate until the stock active
        // animation queue can be observed at a verified safe boundary.
    }

    // Observe only calls initiated by stock. No extra native invocations,
    // no timed UI gate, and no dependency on mulligan queue initialization.
    static bool QueueCanYieldHook(void* self, const void* method) {
        const bool result = originalQueueCanYield(self, method);
        ::CustomComponents::Presentation::Pick3VisualQueueGate::ObserveStockYield(self, result);
        // lifecycleTargetQueue is an observed animation owner, NOT proof of
        // the globally active game/presentation queue.
        if (self == lifecycleTargetQueue && lifecycleTargetAnimation) {
            static void* lastQueue = nullptr;
            static int lastResult = -1;
            if (lastQueue != self || lastResult != static_cast<int>(result)) {
                lastQueue = self;
                lastResult = static_cast<int>(result);
                const auto normal = InspectQueueList(static_cast<Obj*>(self), queueAnimations);
                const auto front = InspectQueueList(static_cast<Obj*>(self), queueFrontAnimations);
                LOG_INFO("CustomComponents/Pick3/PASSIVE_VISUAL: event=STOCK_CAN_YIELD queue=%p result=%d normal=%d front=%d tracked=1",
                    self, static_cast<int>(result), normal.count, front.count);
            }
        }
        return result;
    }

    static bool QueueIsEmptyHook(void* self, const void* method) {
        const bool result = originalQueueIsEmpty(self, method);
        if (self == lifecycleTargetQueue && lifecycleTargetAnimation &&
            (lifecycleQueueUpdates <= 12)) {
            CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/LIFECYCLE: IsEmpty queue=%p result=%d", self, static_cast<int>(result));
        }
        return result;
    }

    static bool TriggeredCanYieldHook(void* self, const void* method) {
        const bool result = originalTriggeredCanYield(self, method);
        if (self == lifecycleTargetAnimation &&
            (lifecycleQueueUpdates <= 12)) {
            CUSTOMCOMPONENTS_ZTAP_TRACE("CustomComponents/ZoneTriggeredAbilityPresentation/LIFECYCLE: CanYield animation=%p result=%d", self, static_cast<int>(result));
        }
        return result;
    }

