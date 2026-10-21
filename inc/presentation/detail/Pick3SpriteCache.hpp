#pragma once
#include "diagnostics/DiagnosticLogging.hpp"
// Reusable DeckEditorCardSpriteCache-backed portrait loader.
// Hooked CardAssetsTask.Start publishes live injected services; construction,
// async requests, cancellation, and Unity Sprite reads run on the existing UI tick.
// A callback only signals (slot, ticket). Pending canceled requests retain their
// managed roots until callback acknowledgment (or process exit): never free a
// possibly callable delegate based on a timer. Bounded slot pool prevents growth.
#include <atomic>
#include <cstdint>
#include <BNM/Class.hpp>
#include <BNM/Method.hpp>
#include <BNM/BasicMonoStructures.hpp>
#include <BNM/Delegates.hpp>
#include <BNM/Exceptions.hpp>
#include "core/SceneViewContext.hpp"
#include "presentation/detail/Pick3NativeCacheReceiver.hpp"
#include "log.h"

namespace CustomComponents::Presentation::Pick3SpriteCache {
    using Obj = BNM::IL2CPP::Il2CppObject;
    namespace Callback = Pick3NativeCacheReceiver;

    struct Request {
        bool used = false;
        bool closed = false;
        bool attempted = false;
        bool requestEntered = false;
        bool requestReturned = false;
        bool constructed = false;
        bool completionLogged = false;
        bool spriteLogged[3]{};
        std::uint32_t ticket = 0;
        std::uint32_t cacheRoot = 0, idsRoot = 0;
        std::uint32_t receiverRoot = 0, delegateRoot = 0, cancelableRoot = 0;
        int ids[3]{};
        unsigned int ticksPending = 0;
        bool pendingLogged = false;
    };

    inline Request requests[Callback::kSlots]{};
    inline int activeSlot = -1;
    inline bool popupOpen = false;
    inline bool probeEnabled = false, requestEnabled = false, metadataReady = false;
    inline bool poolWarning = false;
    inline std::uint32_t nextTicket = 1;
    inline BNM::Class cacheClass, intClass, spriteClass, receiverClass, actionClass;
    inline BNM::Method<void> cacheConstructor, cacheCancel, cacheClear;
    inline BNM::Method<Obj*> cacheLoadAsync, cacheGetSprite;
    inline std::atomic<bool> wantsCapture{false}, captureClaimed{false}, capturePublished{false};
    inline std::uint32_t loaderRoot = 0, providerRoot = 0;

    inline std::uint32_t Root(Obj* obj) {
        return obj && Core::SceneViewContext::newGCHandle
            ? Core::SceneViewContext::newGCHandle(obj, false) : 0;
    }
    inline Obj* Target(std::uint32_t handle) {
        return handle && Core::SceneViewContext::getGCHandle
            ? Core::SceneViewContext::getGCHandle(handle) : nullptr;
    }
    inline void Free(std::uint32_t& handle) {
        if (handle && Core::SceneViewContext::freeGCHandle)
            Core::SceneViewContext::freeGCHandle(handle);
        handle = 0;
    }
    inline void ReportManagedError(const char* stage, const BNM::Exception& ex) {
        const auto cls = ex.ClassName(), msg = ex.Message();
        LOG_ERR("CustomComponents/Pick3/CACHE: EXCEPTION stage=%s class=%s message=%s",
                stage, cls.c_str(), msg.c_str());
    }

    // Called by the stock Start() hook, never invokes loaders or creates sprites.
    inline void Capture(Obj* loader, Obj* provider) {
        if (!probeEnabled || !metadataReady || !loader || !provider ||
            !wantsCapture.load(std::memory_order_acquire)) return;
        if (captureClaimed.exchange(true, std::memory_order_acq_rel)) return;
        loaderRoot = Root(loader);
        providerRoot = Root(provider);
        if (!loaderRoot || !providerRoot) {
            LOG_ERR("CustomComponents/Pick3/CACHE: CAPTURE_FAILED roots=%u/%u", loaderRoot, providerRoot);
            Free(loaderRoot); Free(providerRoot);
            // Another stock task may offer the services on a later frame.
            captureClaimed.store(false, std::memory_order_release);
            return;
        }
        capturePublished.store(true, std::memory_order_release);
        CC_DIAG("CustomComponents/Pick3/CACHE: CAPTURE_OK loaderRoot=%u providerRoot=%u", loaderRoot, providerRoot);
    }
    inline void ReleaseCapture() {
        wantsCapture.store(false, std::memory_order_release);
        capturePublished.store(false, std::memory_order_release);
        Free(loaderRoot); Free(providerRoot);
        captureClaimed.store(false, std::memory_order_release);
    }

    inline bool Initialize(bool enable, bool request) {
        probeEnabled = enable;
        requestEnabled = enable && request;
        if (!enable) return false;
        cacheClass = BNM::Class("PvZCards.DeckEditor", "DeckEditorCardSpriteCache");
        intClass = BNM::Class("System", "Int32");
        spriteClass = BNM::Class("UnityEngine", "Sprite");
        receiverClass = BNM::Class("Sprig.Pick3", "NativeSpriteCacheReceiver");
        actionClass = BNM::Class("System", "Action");
        if (cacheClass.IsValid()) {
            cacheConstructor = cacheClass.GetMethod(".ctor", 2).cast<void>();
            cacheLoadAsync = cacheClass.GetMethod("LoadSpritesAsync", 2).cast<Obj*>();
            cacheGetSprite = cacheClass.GetMethod("GetSprite", 1).cast<Obj*>();
            cacheCancel = cacheClass.GetMethod("CancelOutstandingLoads", 0).cast<void>();
            cacheClear = cacheClass.GetMethod("ClearCache", 0).cast<void>();
        }
        const auto* ctor = cacheConstructor.IsValid() ? cacheConstructor.GetInfo() : nullptr;
        const auto* load = cacheLoadAsync.IsValid() ? cacheLoadAsync.GetInfo() : nullptr;
        const auto* get = cacheGetSprite.IsValid() ? cacheGetSprite.GetInfo() : nullptr;
        const bool ctorOk = ctor && ctor->parameters_count == 2 && ctor->methodPointer;
        const bool loadOk = load && load->parameters_count == 2 && load->methodPointer;
        const bool getOk = get && get->parameters_count == 1 && get->methodPointer;
        metadataReady = cacheClass.IsValid() && intClass.IsValid() && spriteClass.IsValid() &&
            ctorOk && getOk && cacheCancel.IsValid() && cacheClear.IsValid() &&
            Core::SceneViewContext::newGCHandle && Core::SceneViewContext::getGCHandle &&
            Core::SceneViewContext::freeGCHandle &&
            (!requestEnabled || (loadOk && receiverClass.IsValid() && actionClass.IsValid() &&
                                 receiverClass.GetMethod("OnLoaded", 0).IsValid()));
        CC_DIAG("CustomComponents/Pick3/CACHE: SETUP ready=%d mode=%s cancel=%d clear=%d slots=%d",
                 metadataReady ? 1 : 0, requestEnabled ? "REUSABLE_ART" : "CONSTRUCT_ONLY",
                 cacheCancel.IsValid() ? 1 : 0, cacheClear.IsValid() ? 1 : 0, Callback::kSlots);
        if (!metadataReady) {
            probeEnabled = false;
            LOG_ERR("CustomComponents/Pick3/CACHE: disabled, missing native metadata");
        } else {
            // Some CardAssetsTask.Start() calls occur BEFORE the popup opens.
            // Arm at initialization so the first match can use those concrete
            // services even if no further stock card tasks start afterward.
            wantsCapture.store(true, std::memory_order_release);
            CC_DIAG("CustomComponents/Pick3/CACHE: EARLY_CAPTURE armed before popup");
        }
        return probeEnabled;
    }

    inline void BeginSession(const int ids[3]) {
        if (!probeEnabled || !metadataReady || popupOpen) return;
        popupOpen = true;
        activeSlot = -1;
        poolWarning = false;
        // Reuse only same-match injected services; ResetForMatch() releases
        // them on scene exit, and another stock task can supply fresh ones.
        if (!capturePublished.load(std::memory_order_acquire))
            wantsCapture.store(true, std::memory_order_release);
        CC_DIAG("CustomComponents/Pick3/CACHE: SESSION_OPEN ids=%d,%d,%d", ids[0], ids[1], ids[2]);
    }

    inline void Cleanup(int index) {
        Request& r = requests[index];
        if (!r.used) return;
        // This is ONLY called when no async call was entered, or its callback
        // completed. An unresolved canceled callback keeps all roots alive.
        Obj* cache = Target(r.cacheRoot);
        if (cache && r.constructed && cacheClear.IsValid()) {
            const auto ex = BNM::TryInvoke([&] { cacheClear[cache](); });
            if (ex.IsValid()) ReportManagedError("ClearCache", ex);
        }
        Free(r.cancelableRoot); Free(r.delegateRoot); Free(r.receiverRoot);
        Free(r.idsRoot); Free(r.cacheRoot);
        Callback::Disarm(index);
        CC_DIAG("CustomComponents/Pick3/CACHE: RELEASE slot=%d ticket=%u", index, r.ticket);
        r = Request{};
    }

    inline void EndSession() {
        if (!probeEnabled || !popupOpen) return;
        popupOpen = false;
        wantsCapture.store(false, std::memory_order_release);
        // Keep the bounded shared service roots until match exit. Another
        // popup in this match might not trigger any new CardAssetsTask.Start.
        if (activeSlot < 0) return;
        const int index = activeSlot;
        activeSlot = -1;
        Request& r = requests[index];
        if (!r.used) return;
        r.closed = true;
        if (!r.requestEntered || Callback::Finished(index, r.ticket)) {
            Cleanup(index);
            return;
        }
        Obj* cache = Target(r.cacheRoot);
        if (cache && cacheCancel.IsValid()) {
            const auto ex = BNM::TryInvoke([&] { cacheCancel[cache](); });
            if (ex.IsValid()) ReportManagedError("CancelOutstandingLoads", ex);
            else CC_DIAG("CustomComponents/Pick3/CACHE: CANCEL slot=%d ticket=%u", index, r.ticket);
        }
        // Cancellation may suppress the callback entirely. Cannot infer from
        // this API that the callback is impossible: quarantine all roots until
        // callback confirmation, never release by timeout.
        CC_DIAG("CustomComponents/Pick3/CACHE: RETIRED_PENDING slot=%d ticket=%u", index, r.ticket);
    }

    inline void ResetForMatch() {
        if (popupOpen) EndSession();
        // Retired callbacks retain their own cache roots until settled.
        ReleaseCapture();
        // Start observing stock tasks NOW, not on the delayed UI popup open.
        // On the third reported match, every stock Start occurred before
        // SESSION_OPEN, so the old code could never capture any services.
        if (probeEnabled && metadataReady)
            wantsCapture.store(true, std::memory_order_release);
        CC_DIAG("CustomComponents/Pick3/CACHE: MATCH_RESET services released; early capture rearmed; pending requests quarantined");
    }

    inline int AllocateSlot(const int ids[3]) {
        for (int i = 0; i < Callback::kSlots; ++i) {
            Request& r = requests[i];
            if (r.used) continue;
            r = Request{};
            r.used = true;
            r.ticket = nextTicket++;
            if (r.ticket == 0) r.ticket = nextTicket++;
            for (int j = 0; j < 3; ++j) r.ids[j] = ids[j];
            Callback::Arm(i, r.ticket);
            CC_DIAG("CustomComponents/Pick3/CACHE: SLOT_NEW slot=%d ticket=%u ids=%d,%d,%d",
                     i, r.ticket, ids[0], ids[1], ids[2]);
            return i;
        }
        if (!poolWarning) {
            poolWarning = true;
            LOG_ERR("CustomComponents/Pick3/CACHE: SLOT_POOL_FULL (canceled callbacks unresolved); using screenshot fallback");
        }
        return -1;
    }

    inline void StartRequest(int index) {
        Request& r = requests[index];
        if (!requestEnabled || r.attempted || !r.constructed) return;
        r.attempted = true;
        for (int id : r.ids) if (id < 0) {
            LOG_ERR("CustomComponents/Pick3/CACHE: LOAD_SKIP negative cardId"); return;
        }
        auto* ids = intClass.NewList<std::int32_t>();
        if (!ids) { LOG_ERR("CustomComponents/Pick3/CACHE: LOAD_SKIP List<int> unavailable"); return; }
        r.idsRoot = Root(reinterpret_cast<Obj*>(ids));
        if (!r.idsRoot) return;
        for (int id : r.ids) ids->Add(id);
        auto* receiver = reinterpret_cast<Callback::Receiver*>(receiverClass.CreateNewInstance());
        r.receiverRoot = Root(reinterpret_cast<Obj*>(receiver));
        if (!r.receiverRoot) return;
        receiver->slot = index;
        receiver->ticket = r.ticket;
        auto callbackMethod = receiverClass.GetMethod("OnLoaded", 0);
        if (!callbackMethod.IsValid()) {
            LOG_ERR("CustomComponents/Pick3/CACHE: LOAD_SKIP invalid callback method");
            return;
        }
        callbackMethod.SetInstance(reinterpret_cast<Obj*>(receiver));
        auto* shell = reinterpret_cast<BNM::DelegateBase*>(actionClass.CreateNewInstance());
        auto* delegate = shell ? shell->Create(callbackMethod) : nullptr;
        if (!delegate) {
            LOG_ERR("CustomComponents/Pick3/CACHE: LOAD_SKIP delegate construction failed");
            return;
        }
        r.delegateRoot = Root(reinterpret_cast<Obj*>(delegate));
        if (!r.delegateRoot) return;
        Obj* cache = Target(r.cacheRoot);
        if (!cache || !BNM::IsA(cache, cacheClass) || !cacheLoadAsync.IsValid()) {
            LOG_ERR("CustomComponents/Pick3/CACHE: LOAD_SKIP invalid cache instance/method");
            return;
        }
        // A throw after invocation begins might still have scheduled work.
        // Mark first so the associated managed roots are quarantined safely.
        r.requestEntered = true;
        CC_DIAG("CustomComponents/Pick3/CACHE: LOAD_ENTER slot=%d ids=%d,%d,%d", index, r.ids[0], r.ids[1], r.ids[2]);
        Obj* cancelable = nullptr;
        const auto ex = BNM::TryInvoke([&] {
            cancelable = cacheLoadAsync[cache](reinterpret_cast<Obj*>(ids), reinterpret_cast<Obj*>(delegate));
        });
        if (ex.IsValid()) {
            ReportManagedError("LoadSpritesAsync", ex);
            return;
        }
        r.requestReturned = true;
        r.cancelableRoot = Root(cancelable);
        CC_DIAG("CustomComponents/Pick3/CACHE: LOAD_RETURN slot=%d cancelable=%p callbackSeen=%d",
                 index, cancelable, Callback::Finished(index, r.ticket) ? 1 : 0);
    }

    // Only called on the frontend tick. Retired requests that acknowledged
    // completion can safely release their GC handles and cache resources.
    inline void Tick(bool isOpen, const int cardIds[3]) {
        if (!probeEnabled || !metadataReady) return;
        for (int i = 0; i < Callback::kSlots; ++i) {
            Request& r = requests[i];
            if (!r.used) continue;
            if (r.closed && Callback::Finished(i, r.ticket)) {
                CC_DIAG("CustomComponents/Pick3/CACHE: LATE_CALLBACK_ACK slot=%d ticket=%u", i, r.ticket);
                Cleanup(i);
            }
            else if (r.closed && r.requestEntered && !r.pendingLogged && ++r.ticksPending >= 600) {
                r.pendingLogged = true;
                CC_DIAG("CustomComponents/Pick3/CACHE: CANCEL_PENDING slot=%d roots retained", i);
            }
        }
        if (!isOpen || !popupOpen) return;
        if (activeSlot < 0) {
            if (!capturePublished.load(std::memory_order_acquire)) return;
            activeSlot = AllocateSlot(cardIds);
            if (activeSlot < 0) return;
        }
        Request& r = requests[activeSlot];
        if (!r.constructed && !r.attempted) {
            // Construct once per popup, not per process.
            Obj* loader = Target(loaderRoot);
            Obj* provider = Target(providerRoot);
            if (!loader || !provider) { LOG_ERR("CustomComponents/Pick3/CACHE: CONSTRUCT_SKIP missing dependencies"); return; }
            Obj* cache = cacheClass.CreateNewInstance();
            if (!cache || !BNM::IsA(cache, cacheClass) || !cacheConstructor.IsValid()) {
                LOG_ERR("CustomComponents/Pick3/CACHE: CONSTRUCT_SKIP invalid cache instance/method");
                r.attempted = true;
                return;
            }
            r.cacheRoot = Root(cache); // root before invoking managed constructor
            if (!r.cacheRoot) {
                LOG_ERR("CustomComponents/Pick3/CACHE: CONSTRUCT_FAIL allocation/root");
                r.attempted = true; return;
            }
            CC_DIAG("CustomComponents/Pick3/CACHE: CONSTRUCT_ENTER slot=%d cache=%p", activeSlot, cache);
            const auto ex = BNM::TryInvoke([&] { cacheConstructor[cache](loader, provider); });
            if (ex.IsValid()) {
                ReportManagedError("ctor", ex);
                r.attempted = true;
                // Leave rooted object to be dropped when session ends.
                return;
            }
            r.constructed = true;
            CC_DIAG("CustomComponents/Pick3/CACHE: CONSTRUCT_OK slot=%d cacheRoot=%u", activeSlot, r.cacheRoot);
            // Keep two bounded dependency roots until match exit so another
            // popup in this match can construct a fresh cache without waiting
            // for another CardAssetsTask.Start(). The cache also holds refs.
            wantsCapture.store(false, std::memory_order_release);
        }
        if (!r.attempted && requestEnabled) StartRequest(activeSlot);
        if (r.requestEntered && !Callback::Finished(activeSlot, r.ticket) &&
            !r.pendingLogged && ++r.ticksPending >= 600) {
            r.pendingLogged = true;
            CC_DIAG("CustomComponents/Pick3/CACHE: LOAD_PENDING slot=%d roots retained", activeSlot);
        }
    }

    inline bool Completed() {
        if (activeSlot < 0 || !popupOpen || !requestEnabled) return false;
        Request& r = requests[activeSlot];
        const bool yes = r.used && r.requestReturned && Callback::Finished(activeSlot, r.ticket);
        if (yes && !r.completionLogged) {
            r.completionLogged = true;
            CC_DIAG("CustomComponents/Pick3/CACHE: CALLBACK_COMPLETE slot=%d ticket=%u", activeSlot, r.ticket);
        }
        return yes;
    }
    inline Obj* GetSprite(int index, int cardId) {
        if (index < 0 || index >= 3 || !Completed()) return nullptr;
        Request& r = requests[activeSlot];
        if (r.spriteLogged[index] || r.ids[index] != cardId) return nullptr;
        r.spriteLogged[index] = true;
        Obj* cache = Target(r.cacheRoot);
        Obj* sprite = nullptr;
        if (!cache) return nullptr;
        const auto ex = BNM::TryInvoke([&] { sprite = cacheGetSprite[cache](cardId); });
        if (ex.IsValid()) { ReportManagedError("GetSprite", ex); return nullptr; }
        const bool typeOk = sprite && BNM::IsA(sprite, spriteClass);
        CC_DIAG("CustomComponents/Pick3/CACHE: SPRITE option=%d cardId=%d sprite=%p typeOk=%d",
                 index + 1, cardId, sprite, typeOk ? 1 : 0);
        return typeOk ? sprite : nullptr;
    }
}
