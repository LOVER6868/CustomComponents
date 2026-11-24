#include "core/HookRegistry.hpp"
#pragma once
#include "presentation/DiscardCardAnimation.hpp"
#include "presentation/history/RecordedAnimation.hpp"
#include "engine/CustomRecords.hpp"
#include "presentation/CardReadiness.hpp"
#include "diagnostics/DiagnosticLogging.hpp"
// Unity-thread presentation for hand discards.
// The shared FrontendTickDispatcher owns EventSystem.Update.
// LocalHandViewComponent.Awake RVA 0x1F870C4.
// Gameplay never calls Unity methods. It queues (handId, cardId); frontend
// HandViewComponent hooks consume the ticket on a frontend execution path.
// This is not a generic history-record processor or CardView disposal pipeline.
// Guarded view-map retirement and Unity Destroy run on the frontend thread.
// Preserve the gameplay entity; do not synthesize a DrawCardRecord.
#include <BNM/Class.hpp>
#include <BNM/Field.hpp>
#include <BNM/Method.hpp>
#include <algorithm>
#include <cstdint>
#include <mutex>
#include <vector>
#include "log.h"
#include "core/CardViewMappingCoordinator.hpp"
#include "core/FrontendTickDispatcher.hpp"
#include "core/AnimationQueuePostUpdate.hpp"
#include "core/SceneViewContext.hpp"
#include "engine/EntityAccess.hpp"
#include "engine/FrontendServices.hpp"
#include "core/CardViewLifecycleCoordinator.hpp"
#include "core/CardTimelineStartCoordinator.hpp"
#include "presentation/DiscardLifecycle.hpp"
// This is a forward declaration, not a simulation call. Releasing the
// deferred mapper is done only after frontend hand cleanup has begun.
namespace CustomComponents::Presentation::DiscardLifecycle {
inline void FinishVisualRecord(int,BNM::IL2CPP::Il2CppObject*);
inline bool HasDeferredVisual(BNM::IL2CPP::Il2CppObject*,int);
inline bool AwaitingVisualHistory(BNM::IL2CPP::Il2CppObject*,int);
inline void AfterAnimationQueueUpdate(bool);
}

namespace CustomComponents::Systems::DiscardFromHandPresentationSystem {
    using Obj = BNM::IL2CPP::Il2CppObject;
    struct Ticket { int handId; int cardId; unsigned idleUpdates = 0;
        std::uintptr_t modelToken=0,viewIdentity=0;
        unsigned long long serial=0;
        std::uint64_t recordSerial=0; // authoritative custom history record
    };
    inline std::atomic<unsigned long long> nextTicket{0};
    inline std::mutex ticketMutex;
    inline std::vector<Ticket> pending;
    // Discard updates the simulation hand immediately, but its view must
    // not be retired during that processing pass. Defer until the stock
    // presentation queue has finished its queued animations.
    inline std::vector<Ticket> waitingForPresentation;
    inline std::vector<Ticket> readyDiscard;
    inline std::vector<Ticket> retired; // cleared at new local-hand Awake / model change
    inline std::uintptr_t activeModelToken = 0;
    inline Obj* activeLocalHand = nullptr; // observed on Unity main thread, scene-owned
    // Discovered through the ALREADY hooked HandViewComponent methods. Never
    // hook the tiny OpponentHandViewComponent.Awake forwarding stub.
    inline Obj* activeOpponentHand = nullptr;
    inline BNM::Field<int> handViewId;
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
    inline BNM::Field<Obj*> viewMapperModel;
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
    // Reuse the GC-rooted scene mapper captured by the existing lane/view
    // registration tracer. A GetViewObject call is not guaranteed to precede
    // an opponent hand discard, so observedMapper alone is insufficient.
    inline Obj* CurrentSceneMapper() {
        std::uintptr_t modelToken = 0;
        {
            std::lock_guard<std::mutex> lock(ticketMutex);
            modelToken = activeModelToken;
        }
        if (!modelToken || !viewMapperModel.IsValid()) return nullptr;
        if (Obj* mapper = Core::SceneViewContext::MapperForModel(modelToken))
            return mapper;
        // The fallback is also verified against the ticket's model.
        if (observedMapper && reinterpret_cast<std::uintptr_t>(
                viewMapperModel[observedMapper].Get()) == modelToken)
            return observedMapper;
        return nullptr;
    }
    inline thread_local bool inReconcile = false;
    inline void Reconcile(Obj* hand, const void* method);
    inline void ObserveHandFromCommonHook(Obj* hand) {
        if (!hand || !handViewId.IsValid()) return;
        // ViewId is a viewpoint-relative enum: 6 = AllyHand, 7 = OpponentHand.
        // Run ONLY on Unity's frontend through stock hand methods.
        const int kind = handViewId[hand].Get();
        if (kind != 7) return;
        bool discovered = false;
        {
            std::lock_guard<std::mutex> lock(ticketMutex);
            if (activeLocalHand && hand != activeLocalHand && activeOpponentHand != hand) {
                activeOpponentHand = hand;
                discovered = true;
            }
        }
        if (discovered)
            LOG_INFO("CustomComponents/DiscardFromHand/PRESENT: OBSERVED OPPONENT HAND view=%p handId=%d",
                     hand, getHandId.IsValid() ? getHandId[hand]() : -1);
    }
    // Feature-owned replacement decision invoked by the shared mapping owner.
    // A synthetic DrawCardRecord may finish in the same frame as a discard
    // cleanup; use stock replacement only for our tracked hand tickets.
    static bool ChooseHandDiscardReplacement(void* self, Obj* newView,
                                             Obj* cardData, bool replaceViewObject) {
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
        if (tracked) {
            LOG_INFO("CustomComponents/DiscardFromHand/MAP: card=%d old=%p new=%p pendingReturn=%d replace=%d->%d",
                id, existing, newView, (int)tracked, (int)replaceViewObject, (int)useReplacement);
        }
        return useReplacement;
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
    using LocalAwakeFn = void (*)(void*, const void*);
    inline LocalAwakeFn originalLocalHandAwake = nullptr;
    inline BNM::Field<Obj*> controllerDragModel;

    inline bool Same(Ticket t, int handId, int cardId) {
        return t.handId == handId && t.cardId == cardId;
    }
    inline void Queue(int handId, int cardId, std::uintptr_t modelToken,
                      std::uint64_t recordSerial=0) {
        if (handId < 0 || cardId < 0 || !modelToken ||
            Core::SceneViewContext::ModelIdentity() != modelToken) return;
        std::lock_guard<std::mutex> lock(ticketMutex);
        if (Core::SceneViewContext::ModelIdentity() != modelToken) return;
        if (activeModelToken != modelToken) {
            pending.clear();
            waitingForPresentation.clear();
            readyDiscard.clear();
            retired.clear();
            activeModelToken = modelToken;
            ++session;
        }
        const auto same = [&](Ticket ticket) { return Same(ticket, handId, cardId); };
        if (std::none_of(waitingForPresentation.begin(), waitingForPresentation.end(), same) &&
            std::none_of(pending.begin(), pending.end(), same))
        {
            const auto serial=++nextTicket;
            waitingForPresentation.push_back({handId,cardId,0,modelToken,0,serial,recordSerial});
            LOG_INFO("CustomComponents/DiscardFromHand/TICKET: queued=%llu record=%llu card=%d hand=%d model=%p",
                serial,(unsigned long long)recordSerial,cardId,handId,reinterpret_cast<void*>(modelToken));
        }
    }
    inline void QueueDeferred(int handId, int cardId, std::uintptr_t modelToken) {
        Queue(handId, cardId, modelToken);
    }
    struct RecordVisual {std::shared_ptr<Core::ManagedRuntime::Root> view;bool retiring=false;};
    inline std::map<std::uint64_t,RecordVisual> recordVisuals;
    inline Obj* HandFor(int handId){
        for(Obj* hand:{activeLocalHand,activeOpponentHand})if(hand && getHandId.IsValid() && getHandId[hand]()==handId)return hand;
        static auto type=BNM::Class("PvZCards.Game","OpponentHandViewComponent");
        static auto find=BNM::Class("UnityEngine","Object").GetMethod("FindObjectOfType",1).cast<Obj*>();
        Obj* hand=nullptr;
        if(type.IsValid() && find.IsValid())BNM::TryInvoke([&]{hand=find[static_cast<Obj*>(nullptr)](type.GetMonoType());});
        if(hand && getHandId.IsValid() && getHandId[hand]()==handId){activeOpponentHand=hand;return hand;}
        return nullptr;
    }
    inline void CancelDiscard(const History::Record& record);
    inline Presentation::HistoryAnimations::Progress StepDiscard(const History::Record& record,bool start){
        using Progress=Presentation::HistoryAnimations::Progress;
        if(start){
            Queue(record.fromZoneId,record.entityId,record.model,record.serial);
            CC_DIAG("CustomComponents/DiscardFromHand/RECORD_START: serial=%llu card=%d hand=%d anchor=%d",
                static_cast<unsigned long long>(record.serial),record.entityId,record.fromZoneId,record.stockHistoryAnchor);
        }
        Obj* mapper=CurrentSceneMapper();Obj* hand=HandFor(record.fromZoneId);
        Obj* queue=Engine::FrontendServices::Provide(BNM::Class("PvZCards.Game","EffectAnimationQueue"),Engine::FrontendServices::Game());
        if(!mapper || !hand || !queue)return Progress::Failed;
        auto& visual=recordVisuals[record.serial];
        if(start){
            Obj* view=getMappedView.IsValid()?getMappedView[mapper](record.entityId):nullptr;
            if(!view && originalFind)view=originalFind(hand,record.entityId,nullptr);
            if(!view){
                CancelDiscard(record);
                History::Transition(record.serial,History::Delivery::SkippedNoVisual,History::Reason::MissingView);
                return Progress::Complete;
            }
            visual.view=std::make_shared<Core::ManagedRuntime::Root>(view);
        }
        if(!visual.retiring){
            if(Core::CardTimelineStartCoordinator::Read(record.entityId).running) return Progress::Running;
            Obj* current=getMappedView[mapper](record.entityId);
            if(!current && originalFind)current=originalFind(hand,record.entityId,nullptr);
            if(!visual.view || current!=visual.view->Get()){History::Fail(record.serial,History::Reason::ViewReplaced);return Progress::Failed;}
            if(!Presentation::DiscardCardAnimation::BeforeRetire(queue,mapper,record.entityId,hand,true,current))return Progress::Running;
            Ticket ticket{};bool found=false;
            {std::lock_guard<std::mutex> guard(ticketMutex);
                for(auto t:waitingForPresentation)if(t.recordSerial==record.serial){ticket=t;found=true;break;}
                if(found){ticket.viewIdentity=reinterpret_cast<std::uintptr_t>(visual.view->Get());pending.push_back(ticket);readyDiscard.push_back(ticket);
                    std::erase_if(waitingForPresentation,[&](auto t){return t.recordSerial==record.serial;});}
            }
            if(!found)return Progress::Failed;
            Reconcile(hand,nullptr);
            if(originalFind(hand,record.entityId,nullptr) || getMappedView[mapper](record.entityId))return Progress::Failed;
            visual.retiring=true;
        }
        // Unity Destroy is deferred. The record completes only after this exact
        // retired view has actually lost its native Unity object.
        static auto alive=BNM::Class("UnityEngine","Object").GetMethod("op_Implicit",1).cast<bool>();
        if(!alive.IsValid())return Progress::Failed;
        if(visual.view && visual.view->Get() && alive[static_cast<Obj*>(nullptr)](visual.view->Get()))return Progress::Running;
        recordVisuals.erase(record.serial);return Progress::Complete;
    }
    inline void CancelDiscard(const History::Record& record){
        auto it=Presentation::DiscardCardAnimation::animations.find(record.entityId);
        if(it!=Presentation::DiscardCardAnimation::animations.end()){
            auto* animation=it->second.root?it->second.root->Get():nullptr;
            Presentation::NativeAnimation::Call(animation,"Cancel");Presentation::DiscardCardAnimation::animations.erase(it);
        }
        recordVisuals.erase(record.serial);
        std::lock_guard<std::mutex> guard(ticketMutex);
        auto same=[&](auto t){return t.recordSerial==record.serial;};
        std::erase_if(waitingForPresentation,same);std::erase_if(pending,same);std::erase_if(readyDiscard,same);
    }

    inline BNM::Method<bool> animationIsEmpty;
    inline BNM::Method<bool> animationCanYield;
    inline BNM::Field<Obj*> frontAnimationList;
    // Invoked after the stock Update by ZoneTriggeredAbilityPresentationSystem.
    // Do NOT install a second native hook on EffectAnimationQueue.Update.
    static void AfterQueueUpdate(void* queue, const void* /*method*/){
        Obj* self = static_cast<Obj*>(queue);
        if(!self)return;
        static std::shared_ptr<Core::ManagedRuntime::Root> ownerRoot;
        static std::uintptr_t ownerScene=0;
        auto scene=Core::SceneViewContext::ModelIdentity();
        if(scene!=ownerScene){ownerRoot.reset();ownerScene=scene;}
        if(!ownerRoot || !ownerRoot->Get()){
            Obj* found=Engine::FrontendServices::Provide(BNM::Class("PvZCards.Game","EffectAnimationQueue"),Engine::FrontendServices::Game());
            if(found)ownerRoot=std::make_shared<Core::ManagedRuntime::Root>(found,true);
        }
        Obj* owner=ownerRoot?ownerRoot->Get():nullptr;
        if(owner!=self)return;
        // CanYield means an animation can be interrupted; it is NOT evidence
        // that any particular card has completed its presentation.
        const bool normalEmpty=animationIsEmpty.IsValid() && Core::Metadata::Bind(animationIsEmpty,self)();
        Obj* rawFront=frontAnimationList.IsValid()?Core::Metadata::Bind(frontAnimationList,self).Get():nullptr;
        const auto* front=reinterpret_cast<BNM::Structures::Mono::List<Obj*>*>(rawFront);
        const bool frontEmpty=front && front->GetSize()==0;
        const bool idle=normalEmpty && frontEmpty && Presentation::StatQueue::CaughtUp(self);
        const bool canYield=animationCanYield.IsValid() && Core::Metadata::Bind(animationCanYield,self)();
        CustomComponents::Presentation::DiscardLifecycle::AfterAnimationQueueUpdate(idle);
        std::lock_guard<std::mutex> lock(ticketMutex);
        if(!activeLocalHand || waitingForPresentation.empty())return;
        Obj* mapper=Core::SceneViewContext::RootedMapper();
        if(!mapper || !viewMapperModel.IsValid() ||
           reinterpret_cast<std::uintptr_t>(viewMapperModel[mapper].Get())!=activeModelToken)return;
        std::vector<Ticket> stillWaiting;
        for(auto ticket:waitingForPresentation){
            if(ticket.recordSerial){stillWaiting.push_back(ticket);continue;} // queue-owned CustomHistory path
            // Marking and view retirement are distinct boundaries. Prevent
            // later playability refreshes from lighting a retiring card again.
            Obj* retiring=nullptr;
            BNM::TryInvoke([&]{retiring=Core::Metadata::Bind(getMappedView,mapper)(ticket.cardId);});
            if(retiring)Presentation::CardReadiness::Dim(retiring);
            if(retiring && !ticket.viewIdentity)ticket.viewIdentity=reinterpret_cast<std::uintptr_t>(retiring);
            if(retiring && ticket.viewIdentity!=reinterpret_cast<std::uintptr_t>(retiring)){
                LOG_WARN("CustomComponents/DiscardFromHand/VIEW_REPLACED: ticket=%llu card=%d original=%p replacement=%p; refusing to retire replacement",
                    ticket.serial,ticket.cardId,reinterpret_cast<void*>(ticket.viewIdentity),retiring);
                continue;
            }
            Obj* entity=Engine::EntityAccess::Find(viewMapperModel[mapper].Get(),ticket.cardId);
            const bool leftHand=!entity || !Engine::EntityAccess::Component(entity,"HandElement");
            if(ticket.idleUpdates++==0)LOG_INFO(
                "CustomComponents/DiscardFromHand/BOUNDARY: ticket=%llu card=%d hand=%d view=%p leftHand=%d normalEmpty=%d frontEmpty=%d canYield=%d queueCaughtUp=%d deferredHistory=%d",
                ticket.serial,ticket.cardId,ticket.handId,retiring,int(leftHand),int(normalEmpty),int(frontEmpty),int(canYield),
                int(Presentation::StatQueue::CaughtUp(self)),
                int(Presentation::DiscardLifecycle::AwaitingVisualHistory(mapper,ticket.cardId)));
            // A deferred mapper removal only says the model has moved on. Keep
            // the hand view until stock has consumed all queued history records.
            if(!idle || !leftHand || CustomComponents::Presentation::DiscardLifecycle::AwaitingVisualHistory(mapper,ticket.cardId)){
                stillWaiting.push_back(ticket);
                continue;
            }
            // The queue is only a safe point to *start our own card-bound*
            // presentation. It is NOT this ticket's completion boundary.
            // BeforeRetire waits for both native Lift/Scale IsCompleted calls.
            Obj* hand=activeLocalHand;
            if(!hand || Core::Metadata::Bind(getHandId,hand)()!=ticket.handId)hand=activeOpponentHand;
            if(!hand){
                static auto type=BNM::Class("PvZCards.Game","OpponentHandViewComponent");
                static auto find=BNM::Class("UnityEngine","Object").GetMethod("FindObjectOfType",1).cast<Obj*>();
                if(type.IsValid() && find.IsValid()){
                    BNM::TryInvoke([&]{hand=Core::Metadata::Bind(find,static_cast<Obj*>(nullptr))(type.GetMonoType());});
                    if(hand && Core::Metadata::Bind(getHandId,hand)()==ticket.handId)activeOpponentHand=hand;
                }
            }
            if(hand && Core::Metadata::Bind(getHandId,hand)()!=ticket.handId)hand=nullptr;
            if(!Presentation::DiscardCardAnimation::BeforeRetire(self,mapper,ticket.cardId,hand)){
                stillWaiting.push_back(ticket);
                continue;
            }
            LOG_INFO("CustomComponents/DiscardFromHand/RETIRE_READY: ticket=%llu card=%d view=%p hand=%p",ticket.serial,ticket.cardId,
                reinterpret_cast<void*>(ticket.viewIdentity),hand);
            if(std::none_of(pending.begin(),pending.end(),[&](Ticket v){return Same(v,ticket.handId,ticket.cardId);}))pending.push_back(ticket);
            if(std::none_of(readyDiscard.begin(),readyDiscard.end(),[&](Ticket v){return Same(v,ticket.handId,ticket.cardId);}))readyDiscard.push_back(ticket);

        }
        waitingForPresentation.swap(stillWaiting);
    }
    static void LocalHandAwakeHook(void* self, const void* method) {
        originalLocalHandAwake(self, method);
        // Called on the Unity scene lifecycle; do not reuse the previous match's hand.
        {
            std::lock_guard<std::mutex> lock(ticketMutex);
            activeLocalHand = static_cast<Obj*>(self);
            activeOpponentHand = nullptr;
            observedMapper = nullptr;
            pending.clear();
            waitingForPresentation.clear();
            readyDiscard.clear();
            retired.clear();
            activeModelToken = 0;
            ++session;
        }
        CustomComponents::Core::CardViewLifecycleCoordinator::OnSceneAwake();
        CC_DIAG("CustomComponents/DiscardFromHand/PRESENT: NEW LOCAL HAND hand=%p session=%lu", self, session);
    }
    // Called by FrontendTickDispatcher on the Unity frontend, before deferred
    // CardView creation and before stock EventSystem.Update. Preserve this order.
    static void ReconcilePendingOnFrontendTick() {
        // Frontend fallback when the game has no additional stock records:
        // the same stock-history cursor must authorize the custom record.
        Presentation::StatQueue::PumpCustomRecords();
        Obj* local = nullptr;
        Obj* opponent = nullptr;
        {
            std::lock_guard<std::mutex> lock(ticketMutex);
            if (!pending.empty()) {
                local = activeLocalHand;
                opponent = activeOpponentHand;
            }
        }
        if (local && !inReconcile) Reconcile(local, nullptr);
        if (opponent && opponent != local && !inReconcile) Reconcile(opponent, nullptr);
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
            Obj* mapper = CurrentSceneMapper();
            if (!mapper || !viewMapperModel.IsValid() || !getMappedView.IsValid()) continue;
            Obj* model = viewMapperModel[mapper].Get();
            if (!model) continue;
            Obj* entity = Engine::EntityAccess::Find(model, t.cardId);
            if (!t.recordSerial && entity && Engine::EntityAccess::Component(entity, "HandElement")) continue;

            Obj* cardView = originalFind(hand, t.cardId, method);
            Obj* mappedBefore = getMappedView[mapper](t.cardId);
            const bool wasOwned=cardView!=nullptr;
            if(!cardView && t.recordSerial && recordVisuals.contains(t.recordSerial) && mappedBefore &&
               reinterpret_cast<std::uintptr_t>(mappedBefore)==t.viewIdentity)cardView=mappedBefore;
            if(t.modelToken!=reinterpret_cast<std::uintptr_t>(model) ||
               (t.viewIdentity && reinterpret_cast<std::uintptr_t>(mappedBefore?mappedBefore:cardView)!=t.viewIdentity)){
                LOG_WARN("CustomComponents/DiscardFromHand/RETIRE_IDENTITY_MISMATCH: ticket=%llu card=%d model=%p expectedModel=%p view=%p expectedView=%p",
                    t.serial,t.cardId,model,reinterpret_cast<void*>(t.modelToken),mappedBefore,
                    reinterpret_cast<void*>(t.viewIdentity));
                continue;
            }
            if (!cardView && !mappedBefore &&
                !CustomComponents::Presentation::DiscardLifecycle::HasDeferredVisual(mapper, t.cardId)) {
                // Stock already removed this view. There is nothing left to detach.
                if(t.recordSerial && !recordVisuals.contains(t.recordSerial))Engine::CustomRecords::MarkVisualComplete(t.recordSerial);
                std::lock_guard<std::mutex> lock(ticketMutex);
                const auto matches = [&](Ticket v) { return Same(v, t.handId, t.cardId); };
                pending.erase(std::remove_if(pending.begin(), pending.end(), matches), pending.end());
                readyDiscard.erase(std::remove_if(readyDiscard.begin(), readyDiscard.end(), matches), readyDiscard.end());
                continue;
            }
            // Leave views mapped elsewhere to the stock presentation owner.
            if (!cardView || (mappedBefore && mappedBefore != cardView)) continue;
            Obj* drag = handDragModel.IsValid() ? handDragModel[hand].Get() : nullptr;
            if (drag && dragCardId.IsValid() && clearDragState.IsValid() &&
                dragCardId[drag].Get() == t.cardId) {
                clearDragState[drag]();
                CC_DIAG("CustomComponents/DiscardFromHand/PRESENT: CLEARED DRAG card=%d",t.cardId);
            }
            // CardViewComponent derives from UnityEngine.Component via View/MonoBehaviour.
            // Resolve its GameObject BEFORE detaching it from the HandView.
            Obj* visualGO = componentGetGameObject.IsValid() ? componentGetGameObject[cardView]() : nullptr;
            if (!visualGO) {
                LOG_ERR("CustomComponents/DiscardFromHand/PRESENT: NO GAMEOBJECT hand=%d card=%d; keeping hand untouched",handId,t.cardId);
                continue;
            }
            // Stock removal updates HandPositioner + OwnedCards and repositions.
            if(wasOwned)removeFromHand[hand](cardView);
            const bool gone = originalFind(hand, t.cardId, method) == nullptr;
            if (gone) {
                // First quarantine the view so it cannot be interacted with.
                gameObjectSetActive[visualGO](false);
                const bool hidden = gameObjectIsActive.IsValid() ? !gameObjectIsActive[visualGO]() : true;
                Obj* mapped = mapper && getMappedView.IsValid() ? getMappedView[mapper](t.cardId) : nullptr;
                LOG_INFO("CustomComponents/DiscardFromHand/CLEANUP: CHECK card=%d mapper=%p mapped=%p expected=%p hidden=%d",
                         t.cardId, mapper, mapped, cardView, (int)hidden);
                if (mapper && (!mapped || mapped == cardView) && hidden &&
                    removeMappedView.IsValid() && destroyUnityObject.IsValid()) {
                    if (mapped == cardView) removeMappedView[mapper](t.cardId);
                    // The Discard mapper hook deferred this exact removal.
                    // Release it ONLY now, on the frontend, after HandView has
                    // detached/hidden the card and requested view unmapping.
                    bool discardReady=false;
                    {
                        std::lock_guard<std::mutex> lock(ticketMutex);
                        discardReady=std::any_of(readyDiscard.begin(),readyDiscard.end(),
                            [&](Ticket v){return Same(v,t.handId,t.cardId);});
                    }
                    if(discardReady)
                        CustomComponents::Presentation::DiscardLifecycle::FinishVisualRecord(t.cardId,mapper);
                    Obj* after = getMappedView[mapper](t.cardId);
                    LOG_INFO("CustomComponents/DiscardFromHand/CLEANUP: UNMAPPED card=%d after=%p",t.cardId,after);
                    if (!after) {
                        if(t.recordSerial && !recordVisuals.contains(t.recordSerial))Engine::CustomRecords::MarkVisualComplete(t.recordSerial);
                        {std::lock_guard<std::mutex> lock(ticketMutex);
                         readyDiscard.erase(std::remove_if(readyDiscard.begin(),readyDiscard.end(),
                             [&](Ticket v){return Same(v,t.handId,t.cardId);}),readyDiscard.end());}
                        CustomComponents::Core::CardViewLifecycleCoordinator::ConfirmRetirement(t.cardId, cardView, "hand-cleanup");
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
        if (!inReconcile) {
            ObserveHandFromCommonHook(static_cast<Obj*>(self));
            Reconcile(static_cast<Obj*>(self), method);
        }
        return originalFind(self,cardId,method);
    }
    static void RepositionHook(void* self, const void* method) {
        if (!inReconcile) {
            ObserveHandFromCommonHook(static_cast<Obj*>(self));
            Reconcile(static_cast<Obj*>(self), method);
        }
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
                CC_DIAG("CustomComponents/DiscardFromHand/PRESENT: REARMED DRAG card=%d hand=%d view=%p mapped=%p",
                    cardId, localHandId, liveView, mappedView);
                originalDragOut(self, method);
                return;
            }
            if (drag && clearDragState.IsValid()) clearDragState[drag]();
            CC_DIAG("CustomComponents/DiscardFromHand/PRESENT: BLOCKED STALE DRAG card=%d hand=%d view=%p mapped=%p ticket=%d",
                cardId, localHandId, liveView, mappedView, (int)matchesRetirement);
            return;
        }
        originalDragOut(self,method);
    }
    inline bool Initialize() {
        if(!Presentation::DiscardLifecycle::Initialize())return false;
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
        handViewId = hand.GetField("ViewId");
        removeFromHand = hand.GetMethod("RemoveFromHand",1).cast<void>();
        componentGetGameObject = component.GetMethod("get_gameObject",0).cast<Obj*>();
        gameObjectSetActive = gameObject.GetMethod("SetActive",1).cast<void>();
        gameObjectIsActive = gameObject.GetMethod("get_activeSelf",0).cast<bool>();
        destroyUnityObject = unityObject.GetMethod("Destroy", {unityObject.GetCompileTimeClass()}).cast<void>();
        getMappedView = mapperType.GetMethod("GetViewObject", {std::string_view("entityId")}).cast<Obj*>();
        removeMappedView = mapperType.GetMethod("RemoveViewObject", 1).cast<void>();
        auto getMapped = mapperType.GetMethod("GetViewObject", {std::string_view("entityId")});
        cardMapperViewMapper = cardMapperType.GetField("<ViewMapper>k__BackingField");
        viewMapperModel = mapperType.GetField("<EntityModel>k__BackingField");
        cardIdDataEntityId = cardIdDataType.GetField("EntityId");
        auto mapCardView = cardMapperType.GetMethod("MapCardView", 3);
        handDragModel = hand.GetField("<DragModel>k__BackingField");
        dragCardId = drag.GetField("CardEntityId");
        clearDragState = drag.GetMethod("ClearDragState",0).cast<void>();
        controllerDragModel = controller.GetField("<DragModel>k__BackingField");
        auto find = hand.GetMethod("FindCardInHand",1);
        auto reposition = hand.GetMethod("RepositionCards",0);
        auto dragOut = controller.GetMethod("DragCardOutOfHand",0);
        auto localHand = BNM::Class("PvZCards.Game", "LocalHandViewComponent");
        auto awake = localHand.GetMethod("Awake",0);
        auto animationQueue=BNM::Class("PvZCards.Game","EffectAnimationQueue");
        animationIsEmpty=animationQueue.GetMethod("IsEmpty",0).cast<bool>();
        animationCanYield=animationQueue.GetMethod("CanYield",0).cast<bool>();
        frontAnimationList=animationQueue.GetField("FrontAnimations").cast<Obj*>();
        auto updateAnimations=animationQueue.GetMethod("Update",0);
        if (!getHandId.IsValid() || !handViewId.IsValid() || !removeFromHand.IsValid() ||
            !componentGetGameObject.IsValid() || !gameObjectSetActive.IsValid() || !gameObjectIsActive.IsValid() ||
            !destroyUnityObject.IsValid() || !getMappedView.IsValid() || !removeMappedView.IsValid() || !getMapped.IsValid() ||
            !cardMapperViewMapper.IsValid() || !viewMapperModel.IsValid() ||
            !cardIdDataEntityId.IsValid() || !mapCardView.IsValid() ||
            !handDragModel.IsValid() || !dragCardId.IsValid() ||
            !clearDragState.IsValid() || !controllerDragModel.IsValid() ||
            !find.IsValid() || !reposition.IsValid() || !dragOut.IsValid() ||
            !CustomComponents::Core::FrontendTickDispatcher::IsAvailable() ||
            !localHand.IsValid() || !awake.IsValid() || !animationIsEmpty.IsValid() || !frontAnimationList.IsValid() || !updateAnimations.IsValid()) {
            LOG_ERR("CustomComponents/DiscardFromHand/PRESENT: required frontend API missing");
            return false;
        }
        CustomComponents::Core::Hooks::Install(getMapped,GetMappedHook,originalGetMapped);
        if (!originalGetMapped) {
            LOG_ERR("CustomComponents/DiscardFromHand/CLEANUP: GetViewObject hook failed");
            return false;
        }
        // Same installation boundary as before; the generic owner keeps
        // replacement selection -> MapBefore -> stock -> MapAfter in order.
        const bool mappingOk = Core::CardViewMappingCoordinator::Initialize(
            mapCardView,
            &ChooseHandDiscardReplacement,
            &CustomComponents::Core::CardViewLifecycleCoordinator::MapBefore,
            &CustomComponents::Core::CardViewLifecycleCoordinator::MapAfter);
        if (!mappingOk) {
            LOG_ERR("CustomComponents/DiscardFromHand/MAP: MapCardView hook failed");
            return false;
        }
        CustomComponents::Core::Hooks::Install(find,FindHook,originalFind);
        CustomComponents::Core::Hooks::Install(reposition,RepositionHook,originalReposition);
        CustomComponents::Core::Hooks::Install(dragOut,DragOutHook,originalDragOut);
        // Install the same stock tick hook at exactly the former installation point:
        // hand reconciliation -> deferred CardView creation -> stock Update.
        const bool tickOk = CustomComponents::Core::FrontendTickDispatcher::Initialize(
            &ReconcilePendingOnFrontendTick,
            &CustomComponents::Core::CardViewLifecycleCoordinator::ProcessDeferredOnFrontendTick);
        CustomComponents::Core::Hooks::Install(awake,LocalHandAwakeHook,originalLocalHandAwake);
        // The zone presentation system already hooks this exact stock method.
        // Only subscribe to its post-stock-update observer; duplicate hooks abort startup.
        const bool queueObserverOk = CustomComponents::Core::AnimationQueuePostUpdate::Register(
            &AfterQueueUpdate);
        const bool recordProcessorOk = Presentation::HistoryAnimations::Initialize() &&
            Presentation::HistoryAnimations::Register(History::Kind::DiscardOutsidePlay,StepDiscard,CancelDiscard,true);
        const bool ok = originalFind && originalReposition && originalDragOut &&
                        tickOk && originalLocalHandAwake && queueObserverOk && recordProcessorOk;
        const bool lifecycleOk = CustomComponents::Core::CardViewLifecycleCoordinator::Initialize();
        const bool timelineOk = CustomComponents::Core::CardTimelineStartCoordinator::Initialize();
        CC_DIAG("CustomComponents/DiscardFromHand/MAP: stock replacement preflight enabled; tracer=%d", (int)lifecycleOk);
        CC_DIAG("CustomComponents/CardTimeline: stock Start passthrough=%d", (int)timelineOk);
        CC_DIAG("CustomComponents/DiscardFromHand/PRESENT: hooks find=%d reposition=%d dragOut=%d tick=%d awake=%d; automatic frontend reconcile",(int)(originalFind!=nullptr),(int)(originalReposition!=nullptr),(int)(originalDragOut!=nullptr),(int)tickOk,(int)(originalLocalHandAwake!=nullptr));
        return ok && timelineOk;
    }
} // namespace CustomComponents::Systems::DiscardFromHandPresentationSystem
