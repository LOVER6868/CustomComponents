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
        originalQueueUpdate(self, method);
        activeQueueUpdate = previousQueue;
        if (traced) LogQueueLifecycle(queue, "UPDATE AFTER");
        FlushCompletedIdleLaneFrames(queue);
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

