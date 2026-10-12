#pragma once
// Phase 5 / Stage 2.3C: experimental automatic Unity-thread presentation.
// EventSystem.Update RVA 0x374B368; LocalHandViewComponent.Awake RVA 0x1F870C4.
// Gameplay never calls Unity methods. It queues (handId, cardId); frontend
// HandViewComponent hooks consume the ticket on a frontend execution path.
// This is NOT a full history-record processor or CardView disposal pipeline.
// Stage 2.3E-B: guarded view-map retirement + Unity Destroy on frontend thread.
// Same gameplay entity; no synthetic DrawCardRecord in this diagnostic stage.
#include <BNM/Class.hpp>
#include <BNM/Field.hpp>
#include <BNM/Method.hpp>
#include <algorithm>
#include <cstdint>
#include <mutex>
#include <vector>
#include "log.h"
#include "systems/CardViewLifecycleTracer.hpp"

namespace CustomComponents::Systems::DiscardFromHandPresentationSystem {
    using Obj = BNM::IL2CPP::Il2CppObject;
    struct Ticket { int handId; int cardId; };
    inline std::mutex ticketMutex;
    inline std::vector<Ticket> pending;
    inline std::vector<Ticket> retired; // cleared at new local-hand Awake / model change
    inline std::uintptr_t activeModelToken = 0;
    inline Obj* activeLocalHand = nullptr; // observed on Unity main thread, scene-owned
    inline unsigned long session = 0;
    inline BNM::Method<int> getHandId;
    inline BNM::Method<void> removeFromHand;
    inline BNM::Method<Obj*> componentGetGameObject;
    inline BNM::Method<void> gameObjectSetActive;
    inline BNM::Method<bool> gameObjectIsActive;
    inline BNM::Method<void> destroyUnityObject;
    inline BNM::Method<Obj*> getMappedView;
    inline BNM::Method<void> removeMappedView;
    inline BNM::Field<Obj*> cardMapperViewMapper;
    inline BNM::Field<int> cardIdDataEntityId;
    using GetMappedFn = Obj* (*)(void*, int, const void*);
    inline GetMappedFn originalGetMapped = nullptr;
    // Observed only on the frontend, cleared at local-hand scene Awake.
    // Never use a mapper from a previous scene or across model changes.
    inline Obj* observedMapper = nullptr;
    static Obj* GetMappedHook(void* self, int id, const void* method) {
        Obj* view = originalGetMapped(self, id, method);
        if (view) observedMapper = static_cast<Obj*>(self);
        return view;
    }
    inline thread_local bool inReconcile = false;
    inline void Reconcile(Obj* hand, const void* method);
    // Frontend registration boundary: the synthetic DrawCardRecord may complete
    // in the same frame as a discard cleanup. The stock mapper supports an
    // explicit replacement path; use it only for a card tracked by this module.
    using MapCardViewFn = void (*)(void*, Obj*, Obj*, bool, const void*);
    inline MapCardViewFn originalMapCardView = nullptr;
    static void MapCardViewHook(void* self, Obj* newView, Obj* cardData,
                                bool replaceViewObject, const void* method) {
        if (!originalMapCardView) return;
        const int id = cardData && cardIdDataEntityId.IsValid()
            ? cardIdDataEntityId[cardData].Get() : -1;
        bool tracked = false;
        if (id >= 0) {
            std::lock_guard<std::mutex> lock(ticketMutex);
            tracked = std::any_of(pending.begin(), pending.end(),
                [id](Ticket t) { return t.cardId == id; }) ||
                std::any_of(retired.begin(), retired.end(),
                [id](Ticket t) { return t.cardId == id; });
        }
        if (tracked && activeLocalHand && !inReconcile)
            Reconcile(activeLocalHand, nullptr);
        Obj* mapper = self && cardMapperViewMapper.IsValid()
            ? cardMapperViewMapper[static_cast<Obj*>(self)].Get() : nullptr;
        Obj* existing = id >= 0 && mapper && getMappedView.IsValid()
            ? getMappedView[mapper](id) : nullptr;
        const bool useReplacement = replaceViewObject || (tracked && existing);
        CustomComponents::Systems::CardViewLifecycleTracer::MapBefore(self, newView, cardData, useReplacement);
        if (tracked) {
            LOG_INFO("CustomComponents/DiscardFromHand/MAP: card=%d old=%p new=%p pendingReturn=%d replace=%d->%d",
                id, existing, newView, (int)tracked, (int)replaceViewObject, (int)useReplacement);
        }
        originalMapCardView(self, newView, cardData, useReplacement, method);
        CustomComponents::Systems::CardViewLifecycleTracer::MapAfter(self, newView, cardData, useReplacement);
    }

    inline BNM::Field<Obj*> handDragModel;
    inline BNM::Field<int> dragCardId;
    inline BNM::Method<void> clearDragState;
    using FindFn = Obj* (*)(void*, int, const void*);
    using RepositionFn = void (*)(void*, const void*);
    using DragOutFn = void (*)(void*, const void*);
    inline FindFn originalFind = nullptr;
    inline RepositionFn originalReposition = nullptr;
    inline DragOutFn originalDragOut = nullptr;
    using UnityTickFn = void (*)(void*, const void*);
    inline UnityTickFn originalEventSystemUpdate = nullptr;
    using LocalAwakeFn = void (*)(void*, const void*);
    inline LocalAwakeFn originalLocalHandAwake = nullptr;
    inline BNM::Field<Obj*> controllerDragModel;

    inline bool Same(Ticket t, int handId, int cardId) {
        return t.handId == handId && t.cardId == cardId;
    }
    inline void Queue(int handId, int cardId, std::uintptr_t modelToken) {
        if (handId < 0 || cardId < 0 || !modelToken) return;
        std::lock_guard<std::mutex> lock(ticketMutex);
        if (activeModelToken != modelToken) {
            pending.clear();
            retired.clear();
            activeModelToken = modelToken;
            ++session;
            LOG_INFO("CustomComponents/DiscardFromHand/PRESENT: MODEL SWITCH token=%p session=%lu", (void*)modelToken, session);
        }
        auto it = std::find_if(pending.begin(), pending.end(), [&](Ticket t) {return Same(t,handId,cardId);});
        if (it == pending.end()) pending.push_back({handId, cardId});
        LOG_INFO("CustomComponents/DiscardFromHand/PRESENT: QUEUED hand=%d card=%d session=%lu",handId,cardId,session);
    }
    static void LocalHandAwakeHook(void* self, const void* method) {
        originalLocalHandAwake(self, method);
        // Called on the Unity scene lifecycle; do not reuse the previous match's hand.
        {
            std::lock_guard<std::mutex> lock(ticketMutex);
            activeLocalHand = static_cast<Obj*>(self);
            observedMapper = nullptr;
            pending.clear();
            retired.clear();
            activeModelToken = 0;
            ++session;
        }
        CustomComponents::Systems::CardViewLifecycleTracer::OnSceneAwake();
        LOG_INFO("CustomComponents/DiscardFromHand/PRESENT: NEW LOCAL HAND hand=%p session=%lu", self, session);
    }
    static void EventSystemUpdateHook(void* self, const void* method) {
        // Unity EventSystem.Update is the per-frame frontend thread boundary.
        // Only access the observed scene-owned hand here, never on gameplay worker threads.
        Obj* hand = nullptr;
        {
            std::lock_guard<std::mutex> lock(ticketMutex);
            if (!pending.empty()) hand = activeLocalHand;
        }
        if (hand && !inReconcile) Reconcile(hand, nullptr);
        // Frontend-only: retire conflicting board views via stock death playback;
        // only then permit held CreateCardAsync requests to begin.
        CustomComponents::Systems::CardViewLifecycleTracer::ProcessDeferredOnFrontendTick();
        originalEventSystemUpdate(self, method);
    }
    inline bool WasRetired(int cardId) {
        std::lock_guard<std::mutex> lock(ticketMutex);
        return std::any_of(retired.begin(), retired.end(), [=](Ticket t){return t.cardId == cardId;});
    }
    inline void Reconcile(Obj* hand, const void* method) {
        if (inReconcile || !hand || !getHandId.IsValid() || !originalFind || !removeFromHand.IsValid()) return;
        const int handId = getHandId[hand]();
        std::vector<Ticket> work;
        {
            std::lock_guard<std::mutex> lock(ticketMutex);
            for (auto t : pending) if (t.handId == handId) work.push_back(t);
        }
        if (work.empty()) return;
        inReconcile = true;
        for (const auto t : work) {
            // Only touch views that the expected hand actually owns.
            Obj* cardView = originalFind(hand, t.cardId, method);
            if (!cardView) {
                LOG_INFO("CustomComponents/DiscardFromHand/PRESENT: WAIT hand=%d card=%d view missing",handId,t.cardId);
                continue;
            }
            Obj* drag = handDragModel.IsValid() ? handDragModel[hand].Get() : nullptr;
            if (drag && dragCardId.IsValid() && clearDragState.IsValid() &&
                dragCardId[drag].Get() == t.cardId) {
                clearDragState[drag]();
                LOG_INFO("CustomComponents/DiscardFromHand/PRESENT: CLEARED DRAG card=%d",t.cardId);
            }
            // CardViewComponent derives from UnityEngine.Component via View/MonoBehaviour.
            // Resolve its GameObject BEFORE detaching it from the HandView.
            Obj* visualGO = componentGetGameObject.IsValid() ? componentGetGameObject[cardView]() : nullptr;
            if (!visualGO) {
                LOG_ERR("CustomComponents/DiscardFromHand/PRESENT: NO GAMEOBJECT hand=%d card=%d; keeping hand untouched",handId,t.cardId);
                continue;
            }
            // Stock removal updates HandPositioner + OwnedCards and repositions.
            removeFromHand[hand](cardView);
            const bool gone = originalFind(hand, t.cardId, method) == nullptr;
            if (gone) {
                // First quarantine the view so it cannot be interacted with.
                gameObjectSetActive[visualGO](false);
                const bool hidden = gameObjectIsActive.IsValid() ? !gameObjectIsActive[visualGO]() : true;
                Obj* mapper = observedMapper;
                Obj* mapped = mapper && getMappedView.IsValid() ? getMappedView[mapper](t.cardId) : nullptr;
                LOG_INFO("CustomComponents/DiscardFromHand/CLEANUP: CHECK card=%d mapper=%p mapped=%p expected=%p hidden=%d",
                         t.cardId, mapper, mapped, cardView, (int)hidden);
                if (mapper && mapped == cardView && hidden && removeMappedView.IsValid() && destroyUnityObject.IsValid()) {
                    removeMappedView[mapper](t.cardId);
                    Obj* after = getMappedView[mapper](t.cardId);
                    if (!after)
                        CustomComponents::Systems::CardViewLifecycleTracer::ConfirmRetirement(t.cardId, cardView, "hand-cleanup");
                    LOG_INFO("CustomComponents/DiscardFromHand/CLEANUP: UNMAPPED card=%d after=%p",t.cardId,after);
                    if (!after) {
                        // Unity's Destroy is deferred until the end of the frame.
                        // No direct OnDestroy call and no gameplay entity deletion.
                        destroyUnityObject[static_cast<Obj*>(nullptr)](visualGO);
                        LOG_INFO("CustomComponents/DiscardFromHand/CLEANUP: DESTROY_REQUESTED card=%d go=%p",t.cardId,visualGO);
                    } else {
                        LOG_ERR("CustomComponents/DiscardFromHand/CLEANUP: REMOVE_NOT_CONFIRMED card=%d; no Destroy",t.cardId);
                    }
                } else {
                    // Fail closed: retain the hidden view and its map entry.
                    LOG_ERR("CustomComponents/DiscardFromHand/CLEANUP: SKIP card=%d (mapper missing/mismatch or incomplete hide)",t.cardId);
                }
            } else {
                LOG_ERR("CustomComponents/DiscardFromHand/PRESENT: REMOVE FAILED hand=%d card=%d; GameObject not hidden",handId,t.cardId);
            }
            if (gone) {
                std::lock_guard<std::mutex> lock(ticketMutex);
                pending.erase(std::remove_if(pending.begin(),pending.end(),[&](Ticket p){return Same(p,t.handId,t.cardId);}), pending.end());
                if (!std::any_of(retired.begin(),retired.end(),[&](Ticket p){return Same(p,t.handId,t.cardId);})) retired.push_back(t);
            }
        }
        inReconcile = false;
    }
    static Obj* FindHook(void* self, int cardId, const void* method) {
        if (!inReconcile) Reconcile(static_cast<Obj*>(self), method);
        return originalFind(self,cardId,method);
    }
    static void RepositionHook(void* self, const void* method) {
        if (!inReconcile) Reconcile(static_cast<Obj*>(self), method);
        originalReposition(self,method);
    }
    static void DragOutHook(void* self, const void* method) {
        Obj* controller = static_cast<Obj*>(self);
        Obj* drag = controller && controllerDragModel.IsValid() ? controllerDragModel[controller].Get() : nullptr;
        const int cardId = drag && dragCardId.IsValid() ? dragCardId[drag].Get() : -1;
        if (cardId >= 0 && WasRetired(cardId)) {
            // The entity ID can be legitimately reused when stock DrawCardRecord
            // creates a fresh visual for the SAME gameplay entity. A retired ID
            // alone must never permanently prevent dragging that new view.
            // Only trust the current scene's hand plus its currently mapped view.
            Obj* localHand = activeLocalHand;
            const int localHandId = localHand && getHandId.IsValid()
                ? getHandId[localHand]() : -1;
            Obj* liveView = localHand && originalFind
                ? originalFind(localHand, cardId, nullptr) : nullptr;
            Obj* mappedView = liveView && observedMapper && getMappedView.IsValid()
                ? getMappedView[observedMapper](cardId) : nullptr;
            bool matchesRetirement = false;
            {
                std::lock_guard<std::mutex> lock(ticketMutex);
                matchesRetirement = std::any_of(retired.begin(), retired.end(),
                    [&](Ticket t) { return Same(t, localHandId, cardId); });
            }
            if (matchesRetirement && liveView && mappedView == liveView) {
                {
                    std::lock_guard<std::mutex> lock(ticketMutex);
                    retired.erase(std::remove_if(retired.begin(), retired.end(),
                        [&](Ticket t) { return Same(t, localHandId, cardId); }), retired.end());
                }
                LOG_INFO("CustomComponents/DiscardFromHand/PRESENT: REARMED DRAG card=%d hand=%d view=%p mapped=%p",
                    cardId, localHandId, liveView, mappedView);
                originalDragOut(self, method);
                return;
            }
            if (drag && clearDragState.IsValid()) clearDragState[drag]();
            LOG_INFO("CustomComponents/DiscardFromHand/PRESENT: BLOCKED STALE DRAG card=%d hand=%d view=%p mapped=%p ticket=%d",
                cardId, localHandId, liveView, mappedView, (int)matchesRetirement);
            return;
        }
        originalDragOut(self,method);
    }
    inline bool Initialize() {
        auto hand = BNM::Class("PvZCards.Game","HandViewComponent");
        auto drag = BNM::Class("PvZCards.Game","DragModel");
        auto controller = BNM::Class("PvZCards.Game","CardInHandController");
        auto component = BNM::Class("UnityEngine", "Component");
        auto gameObject = BNM::Class("UnityEngine", "GameObject");
        auto unityObject = BNM::Class("UnityEngine", "Object");
        auto mapperType = BNM::Class("PvZCards.Game", "EntityViewMapper");
        auto cardMapperType = BNM::Class("PvZCards.Game", "CardViewMapper");
        auto cardIdDataType = BNM::Class("PvZCards.Engine.Records", "CardIdData");
        if (!hand.IsValid() || !drag.IsValid() || !controller.IsValid() || !component.IsValid() || !gameObject.IsValid() || !unityObject.IsValid() || !mapperType.IsValid() || !cardMapperType.IsValid() || !cardIdDataType.IsValid()) return false;
        getHandId = hand.GetMethod("GetHandId",0).cast<int>();
        removeFromHand = hand.GetMethod("RemoveFromHand",1).cast<void>();
        componentGetGameObject = component.GetMethod("get_gameObject",0).cast<Obj*>();
        gameObjectSetActive = gameObject.GetMethod("SetActive",1).cast<void>();
        gameObjectIsActive = gameObject.GetMethod("get_activeSelf",0).cast<bool>();
        destroyUnityObject = unityObject.GetMethod("Destroy", {unityObject.GetCompileTimeClass()}).cast<void>();
        getMappedView = mapperType.GetMethod("GetViewObject", 1).cast<Obj*>();
        removeMappedView = mapperType.GetMethod("RemoveViewObject", 1).cast<void>();
        auto getMapped = mapperType.GetMethod("GetViewObject", 1);
        cardMapperViewMapper = cardMapperType.GetField("<ViewMapper>k__BackingField");
        cardIdDataEntityId = cardIdDataType.GetField("EntityId");
        auto mapCardView = cardMapperType.GetMethod("MapCardView", 3);
        handDragModel = hand.GetField("<DragModel>k__BackingField");
        dragCardId = drag.GetField("CardEntityId");
        clearDragState = drag.GetMethod("ClearDragState",0).cast<void>();
        controllerDragModel = controller.GetField("<DragModel>k__BackingField");
        auto find = hand.GetMethod("FindCardInHand",1);
        auto reposition = hand.GetMethod("RepositionCards",0);
        auto dragOut = controller.GetMethod("DragCardOutOfHand",0);
        auto eventSystem = BNM::Class("UnityEngine.EventSystems", "EventSystem");
        auto localHand = BNM::Class("PvZCards.Game", "LocalHandViewComponent");
        auto tick = eventSystem.GetMethod("Update",0);
        auto awake = localHand.GetMethod("Awake",0);
        if (!getHandId.IsValid() || !removeFromHand.IsValid() ||
            !componentGetGameObject.IsValid() || !gameObjectSetActive.IsValid() || !gameObjectIsActive.IsValid() ||
            !destroyUnityObject.IsValid() || !getMappedView.IsValid() || !removeMappedView.IsValid() || !getMapped.IsValid() ||
            !cardMapperViewMapper.IsValid() || !cardIdDataEntityId.IsValid() || !mapCardView.IsValid() ||
            !handDragModel.IsValid() || !dragCardId.IsValid() ||
            !clearDragState.IsValid() || !controllerDragModel.IsValid() ||
            !find.IsValid() || !reposition.IsValid() || !dragOut.IsValid() ||
            !eventSystem.IsValid() || !localHand.IsValid() ||
            !tick.IsValid() || !awake.IsValid()) {
            LOG_ERR("CustomComponents/DiscardFromHand/PRESENT: required frontend API missing");
            return false;
        }
        BNM::BasicHook(getMapped,GetMappedHook,originalGetMapped);
        if (!originalGetMapped) {
            LOG_ERR("CustomComponents/DiscardFromHand/CLEANUP: GetViewObject hook failed");
            return false;
        }
        BNM::BasicHook(mapCardView,MapCardViewHook,originalMapCardView);
        if (!originalMapCardView) {
            LOG_ERR("CustomComponents/DiscardFromHand/MAP: MapCardView hook failed");
            return false;
        }
        BNM::BasicHook(find,FindHook,originalFind);
        BNM::BasicHook(reposition,RepositionHook,originalReposition);
        BNM::BasicHook(dragOut,DragOutHook,originalDragOut);
        BNM::BasicHook(tick,EventSystemUpdateHook,originalEventSystemUpdate);
        BNM::BasicHook(awake,LocalHandAwakeHook,originalLocalHandAwake);
        const bool ok = originalFind && originalReposition && originalDragOut &&
                        originalEventSystemUpdate && originalLocalHandAwake;
        const bool tracerOk = CustomComponents::Systems::CardViewLifecycleTracer::Initialize();
        LOG_INFO("CustomComponents/DiscardFromHand/MAP: stock replacement preflight enabled; tracer=%d", (int)tracerOk);
        LOG_INFO("CustomComponents/DiscardFromHand/PRESENT: hooks find=%d reposition=%d dragOut=%d tick=%d awake=%d; automatic frontend reconcile",(int)(originalFind!=nullptr),(int)(originalReposition!=nullptr),(int)(originalDragOut!=nullptr),(int)(originalEventSystemUpdate!=nullptr),(int)(originalLocalHandAwake!=nullptr));
        return ok;
    }
} // namespace CustomComponents::Systems::DiscardFromHandPresentationSystem
