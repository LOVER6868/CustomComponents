#include "diagnostics/DiagnosticLogging.hpp"
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
            CC_DIAG("CustomComponents/Discarded: KEEP model=%p card=%d reason=re-discarded", model, cardId);
            return;
        }
        Obj* card = FindEntity(model, cardId);
        if (!card || !entityHas[card](discardedType)) return;
        const int origin = GetSource(model, cardId);
        Obj* removed = entityRemove[card](discardedType);
        CC_DIAG("CustomComponents/Discarded: REMOVE model=%p card=%d Source=%d reason=%s removed=%p remaining=%d",
            model, cardId, origin, why ? why : "?", removed,
            entityHas[card](discardedType) ? 1 : 0);
    }

    inline void BeginReturn(Obj* model, int cardId, Obj* triggeredCollection) {
        if (!ready || !model || cardId < 0 || !GetSource(model, cardId)) return;
        std::lock_guard<std::mutex> lock(pendingMutex);
        for (auto& p : pending) {
            if (p.model == model && p.cardId == cardId) {
                CC_DIAG("CustomComponents/Discarded: replacing prior pending return model=%p card=%d", model, cardId);
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
                CC_DIAG("CustomComponents/Discarded: RETURN-FIRED model=%p card=%d Source=%d listeners=%d overflow=%d",
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

