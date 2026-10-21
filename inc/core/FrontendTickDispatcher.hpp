#pragma once
// Single owner of the Unity frontend tick hook.
//
// Ordering contract (preserved from DiscardFromHandPresentationSystem):
//   1. Reconcile pending local/opponent hand-discard views.
//   2. Process deferred CardView creations after confirmed retirement.
//   3. Call the original Unity EventSystem.Update.
//
// Keep callbacks feature-owned: the dispatcher never manipulates game views
// or calls into gameplay code itself. Install only once, on module startup.
#include <BNM/Class.hpp>
#include <BNM/Method.hpp>
#include "log.h"

namespace CustomComponents::Core::FrontendTickDispatcher {
    using FrontendTask = void (*)();
    using UnityTickFn = void (*)(void*, const void*);

    inline UnityTickFn originalEventSystemUpdate = nullptr;
    inline FrontendTask reconcileHandDiscards = nullptr;
    inline FrontendTask processDeferredCardViews = nullptr;
    inline FrontendTask optionalPreviewTick = nullptr; // UI-only test; runs after stock Update

    inline bool IsAvailable() {
        auto eventSystem = BNM::Class("UnityEngine.EventSystems", "EventSystem");
        return eventSystem.IsValid() && eventSystem.GetMethod("Update", 0).IsValid();
    }

    static void EventSystemUpdateHook(void* self, const void* method) {
        // Do not reorder: retiring hand views first may unblock deferred views.
        if (reconcileHandDiscards) reconcileHandDiscards();
        if (processDeferredCardViews) processDeferredCardViews();
        originalEventSystemUpdate(self, method);
        if (optionalPreviewTick) optionalPreviewTick();
    }

    inline bool RegisterOptionalPreviewTick(FrontendTask task) {
        if (!task || !originalEventSystemUpdate || optionalPreviewTick) return false;
        optionalPreviewTick = task;
        return true;
    }

    inline bool Initialize(FrontendTask handReconcile, FrontendTask deferredCardViews) {
        if (!handReconcile || !deferredCardViews) {
            LOG_ERR("CustomComponents/FrontendTick: required callbacks missing");
            return false;
        }
        if (originalEventSystemUpdate) {
            // One native hook per stock method. Do not install a second hook.
            return reconcileHandDiscards == handReconcile &&
                   processDeferredCardViews == deferredCardViews;
        }
        auto eventSystem = BNM::Class("UnityEngine.EventSystems", "EventSystem");
        if (!eventSystem.IsValid()) {
            LOG_ERR("CustomComponents/FrontendTick: EventSystem class missing");
            return false;
        }
        auto update = eventSystem.GetMethod("Update", 0);
        if (!update.IsValid()) {
            LOG_ERR("CustomComponents/FrontendTick: EventSystem.Update missing");
            return false;
        }
        reconcileHandDiscards = handReconcile;
        processDeferredCardViews = deferredCardViews;
        BNM::BasicHook(update, EventSystemUpdateHook, originalEventSystemUpdate);
        if (!originalEventSystemUpdate) {
            reconcileHandDiscards = nullptr;
            processDeferredCardViews = nullptr;
            LOG_ERR("CustomComponents/FrontendTick: EventSystem.Update hook failed");
            return false;
        }
        return true;
    }
} // namespace CustomComponents::Core::FrontendTickDispatcher
