#include "diagnostics/DiagnosticLogging.hpp"
#pragma once
// Private implementation module. Include PickFromThreeSubmission.hpp, not this file.
namespace CustomComponents::Pick3::Submission {
inline void ResetMatch();
inline void(*onMatchReset)()=nullptr;
inline std::uint32_t matchHolderRoot=0;
inline void Drop(std::uint32_t& root) {
    if (root && Core::SceneViewContext::freeGCHandle) Core::SceneViewContext::freeGCHandle(root);
    root = 0;
}
inline void CaptureRoot(std::uint32_t& root, Obj* obj) {
    if (!obj || !Core::SceneViewContext::newGCHandle) return;
    if (Target(root) == obj) return;
    Drop(root);
    root = Core::SceneViewContext::newGCHandle(obj, false);
}
// Restore the two *read-only* service dependencies without re-entering the
// match's DI binder. Patch 1.5.5 quarantined ResolveRepairDependencies(),
// unintentionally leaving revive recognition and the new UI without their
// live EntityModel. The same stock SelectTargetService already owns both.
inline void CaptureDirectServiceDependencies() {
    Obj* service=Target(serviceRoot);
    if(!service || !localClass.IsValid() || !BNM::IsA(service,localClass)) return;
    if(localServiceFinder.IsValid() &&
       TargetBridge::finderEntityFinder.IsValid() && TargetBridge::entityFinderModel.IsValid()) {
        Obj* finder=localServiceFinder[service].Get();
        Obj* entityFinder=finder ? TargetBridge::finderEntityFinder[finder].Get() : nullptr;
        Obj* model=entityFinder ? TargetBridge::entityFinderModel[entityFinder].Get() : nullptr;
        if(model && boundEntityModelClass.IsValid() && BNM::IsA(model,boundEntityModelClass) && Target(boundEntityModelRoot)!=model) {
            CaptureRoot(boundEntityModelRoot,model);
            CC_SELECTION_DIAG("CustomComponents/Pick3/SERVICE_DIRECT_BIND: entityModel=%p source=local-select-finder DI=NOT_INVOKED",model);
        }
    }
    if(localServiceEffects.IsValid() && effectQueueConcrete.IsValid()) {
        Obj* queue=localServiceEffects[service].Get();
        if(queue && BNM::IsA(queue,effectQueueConcrete) && Target(boundEffectsRoot)!=queue) {
            CaptureRoot(boundEffectsRoot,queue);
            CC_SELECTION_DIAG("CustomComponents/Pick3/SERVICE_DIRECT_BIND: effectQueue=%p source=local-select-service DI=NOT_INVOKED",queue);
        }
    }
}
inline void CaptureService(Obj* service, const char* source) {
    if (!enabled || !service || (!BNM::IsA(service, localClass) && !BNM::IsA(service, remoteClass))) return;
    if(Target(serviceRoot)!=service) {
        Drop(boundSignalRoot);Drop(boundEffectsRoot);Drop(boundEntityModelRoot);
        Drop(reviveLegalPlayModelRoot);deferredValidatedPlay={};
    }
    CaptureRoot(serviceRoot, service);
    CaptureDirectServiceDependencies();
    CC_DIAG("CustomComponents/Pick3/SUBMIT: SERVICE_CAPTURE source=%s type=%s remote=%d rooted=%d",
             source, service->klass && service->klass->name ? service->klass->name : "?",
             BNM::IsA(service, remoteClass) ? 1 : 0, serviceRoot ? 1 : 0);
}
// IMPORTANT: Do NOT hook TouchScript.TouchManagerInstance.Update.
// In this build it is a four-byte method at RVA 0x1AA0218, immediately
// followed by another method at 0x1AA021C. Inline hooking it can overwrite
// adjacent code, causing navigation-time SIGSEGV even with no popup open.
// Input safeguards below are limited to UGUI and stock game commands until
// an alternative TouchScript suppression method is verified.
// Avoid relying on SelectTargetCommand's short generated field setters.
// The regular per-frame game input holder already knows BOTH the active
// GameModel and the injected CommandBinder; snapshot them only after the
// game's own Update has returned. The real resolver is called from the UI tick
// rather than during the holder's state transition.
inline bool HasArmedStockHandoff(); // forward; stock handoff state is declared later
static void GameInputHolderUpdated(Obj* self) {
    if (!enabled || !self) return;
    Obj* binder=stateCommandBinder.IsValid()?stateCommandBinder[self].Get():nullptr;
    if(binder) {
        if(matchGeneration.Observe(self,binder)) {
            // Retire presentation while its old managed views are still rooted.
            if(onMatchReset)onMatchReset();
            TargetBridge::Clear();
            ResetMatch();
        }
        CaptureRoot(matchHolderRoot,self);
        CaptureRoot(binderRoot,binder);
    }
    if (stateVisualTurnPhase.IsValid() && visualSubTurn.IsValid()) {
        Obj* phase = stateVisualTurnPhase[self].Get();
        observedVisualPhase = phase != nullptr;
        const int subturn = phase ? visualSubTurn[phase].Get() : -1;
        observedLocalTargetSubTurn = subturn == 1;
        NetworkTrace::ObserveVisualSubturn(phase, subturn, overlayCapturesInput);
    }
    if (stateGameModel.IsValid()) {
        Obj* model = stateGameModel[self].Get();
        if (model && Target(modelRoot) != model &&
                !(overlayCapturesInput || TargetBridge::HasQueuedChoice())) {
            CaptureRoot(modelRoot, model);
            CC_DIAG("CustomComponents/Pick3/SUBMIT: MODEL_CAPTURE source=GameInputStateHolder rooted=%d", modelRoot ? 1 : 0);
        }
    }
    // Binder replacement above may invalidate old rooted dependencies.
    // Reacquire only through safe field reads on the already-rooted service.
    CaptureDirectServiceDependencies();
    PumpDefaultRepair();
    // A refresh ticket is issued only after a completed custom selection and
    // the stock state-repair signal. The command itself checks the input state,
    // injected dependencies, and waits for two stable UI frames.
    PumpHandHighlights(self);
    // Stock EndTurnButtonViewMediator has an Interrupted-state bug after
    // successive manual choices (including PF3). The recovery was previously
    // registered but NEVER driven from a frame observer. This is the existing
    // GameInputStateHolder.Update observer: no new IL2CPP inline detour.
    if (TargetButtonTrace::chooseSerial!=TargetButtonTrace::recoveredSerial &&
        TargetButtonTrace::chooseSerial!=0) {
        const auto& observed=Systems::TargetingPresentationRefreshSystem::observedTargetingIdentity;
        // Do not resume the Pass-to-Combat button if the current stock queue
        // front is another manual choice, even if holder state is briefly 3/4.
        Obj* front=nullptr;
        auto& first=Systems::TargetingPresentationRefreshSystem::getFirstEffectInQueue;
        if (first.IsValid()) front=first[self]();
        const bool frontIsTarget=front && chooseBlockingClass.IsValid() &&
            BNM::IsA(front,chooseBlockingClass);
        const bool noActiveTarget=(!observed.valid &&
            observed.state!=6 && observed.state!=7 && !frontIsTarget &&
            !overlayCapturesInput && !TargetBridge::HasQueuedChoice() &&
            !HasArmedStockHandoff());
        TargetButtonTrace::TryRecoverAfterCompletedLocalPhase(noActiveTarget, true);
    }
}
// Resolve the same scene's injected singleton objects used by stock commands.
// This avoids relying on a SelectTargetCommand, which custom pickers bypass.
inline void ResolveRepairDependencies(bool force) {
    if (!force && ++repairDependencyPoll % 60 != 1) return;
    Obj* binder=Target(binderRoot);
    if (!binder || !getInjectionBinder.IsValid() || !getInjectedInstance.IsValid()) return;
    Obj* injector=nullptr;
    const auto e=BNM::TryInvoke([&] {injector=getInjectionBinder[binder]();});
    if (e.IsValid() || !injector) return;
    const auto lookup=[&](BNM::Class& klass, std::uint32_t& root) {
        if (Target(root) || !klass.IsValid()) return;
        Obj* found=nullptr;
        const auto x=BNM::TryInvoke([&] {
            found=getInjectedInstance[injector](klass.GetMonoType(), static_cast<Obj*>(nullptr));
        });
        if (!x.IsValid() && found && (BNM::IsA(found,klass) || &klass==&effectCollectionInterface))
            CaptureRoot(root,found);
    };
    lookup(gameStateUpdatedSignalClass,boundSignalRoot);
    lookup(effectCollectionInterface,boundEffectsRoot);
    lookup(boundEntityModelClass,boundEntityModelRoot);
}
// Read all three live lists. A response-list transfer isn't retirement.
inline bool EffectAbsentFromQueue(Obj* queue, int effect, int group, bool& safe) {
    safe=false;
    if (!queue || !effectQueueConcrete.IsValid() || !BNM::IsA(queue,effectQueueConcrete) ||
        !boundQueueEffects.IsValid() || !liveEffectEntityId.IsValid() || !liveEffectGroupId.IsValid()) return false;
    bool activeOtherBlocking=false;
    const auto scan=[&](BNM::Structures::Mono::List<Obj*>* entries,bool frontList)->bool {
        if (!entries || entries->size<0 || entries->size>4096 ||
            (entries->size>0 && !entries->items)) {safe=false;return true;}
        for (int i=0; i<entries->size; ++i) {
            Obj* entry=entries->items->m_Items[i];
            if (!entry) continue;
            if (liveEffectEntityId[entry].Get()==effect && liveEffectGroupId[entry].Get()==group)
                return true;
            if (frontList && i==0 && chooseBlockingClass.IsValid() &&
                BNM::IsA(entry,chooseBlockingClass)) activeOtherBlocking=true;
        }
        return false;
    };
    auto* all=boundQueueEffects[queue].Get();
    auto* responses=boundQueueResponses.IsValid()?boundQueueResponses[queue].Get():nullptr;
    auto* groups=boundQueueGroupResponses.IsValid()?boundQueueGroupResponses[queue].Get():nullptr;
    // Missing or malformed lists cannot authorize a synthetic notification.
    if (!all || !responses || !groups) return false;
    if (all->size<0 || responses->size<0 || groups->size<0 ||
        all->size>4096 || responses->size>4096 || groups->size>4096) return false;
    safe=true;
    const bool present=scan(all,true) || scan(responses,true) || scan(groups,true);
    return safe && !present && !activeOtherBlocking;
}
// Highlight observer state must be declared before PumpDefaultRepair uses it.
inline unsigned stockHighlightSinceRefresh = 0;
inline int lastRefreshEffect = -1, lastRefreshGroup = -1;
inline void PumpDefaultRepair() {
    if (!enabled) return;
    // Patch 1.5.5: don't resolve injected services from the frame-time repair
    // pump, especially while mulligan/scene initialization is in progress.
    // The old resolver called getInjectionBinder[binder]() through TryInvoke
    // on a not-yet-ready IL2CPP instance and native-crashed at null+0x38.
    // Synthetic refresh is already disabled, so missing roots need no repair.
    if (!Target(boundEffectsRoot) && pendingRefresh) {
        CC_SELECTION_DIAG("CustomComponents/Pick3/CRASH_FIX_155: repair roots absent; skip early dependency resolution and wait for stock scene lifecycle");
        pendingRefresh=false;
        pendingRefreshAge=pendingStableFrames=0;
    }
    if (pendingRemoteRetirement) {
        // The only positive evidence here is the effect disappearing from the
        // already-bound queue. This is NOT proof of server acceptance; label it
        // as observed local-state retirement, never as a network ACK.
        bool retirementSafe = false;
        Obj* observedQueue = Target(boundEffectsRoot);
        const bool absent = observedQueue &&
            EffectAbsentFromQueue(observedQueue, remoteRetirementEffect,
                                  remoteRetirementGroup, retirementSafe);
#if CUSTOMCOMPONENTS_PF3_REPAIR_TRACE
        if ((remoteRetirementPolls % 120)==0 || absent) {
            const auto& holder=Systems::TargetingPresentationRefreshSystem::observedTargetingIdentity;
            int frontEffect=-1,frontGroup=-1,queueCount=-1,queuedMatches=-1;
            if (observedQueue && effectQueueConcrete.IsValid() && BNM::IsA(observedQueue,effectQueueConcrete) &&
                boundQueueEffects.IsValid()) {
                auto* entries=boundQueueEffects[observedQueue].Get();
                if (entries && entries->size>=0 && entries->size<=4096 &&
                    (entries->size==0 || entries->items)) {
                    queueCount=entries->size;
                    queuedMatches=0;
                    for(int i=0;i<entries->size;++i) {
                        Obj* entry=entries->items->m_Items[i];
                        if(!entry || !liveEffectEntityId.IsValid() || !liveEffectGroupId.IsValid()) continue;
                        const int id=liveEffectEntityId[entry].Get();
                        const int group=liveEffectGroupId[entry].Get();
                        if(i==0) {frontEffect=id;frontGroup=group;}
                        if(id==remoteRetirementEffect && group==remoteRetirementGroup) ++queuedMatches;
                    }
                }
            }
            PF3_REPAIR_TRACE("CustomComponents/Pick3/REPAIR177: RETIRE_OBSERVE effect=%d group=%d polls=%d queue=%p model=%p safe=%d absent=%d front=%d/%d activeCount=%d matchingActive=%d holder=%d/%d holderState=%d phase=%d localSubturn=%d stockSuccesses=%u",
                remoteRetirementEffect,remoteRetirementGroup,remoteRetirementPolls,(void*)observedQueue,
                (void*)Target(modelRoot),int(retirementSafe),int(absent),
                frontEffect,frontGroup,queueCount,queuedMatches,
                holder.effectEntityId,holder.effectGroupId,holder.state,holder.phase,
                int(observedLocalTargetSubTurn),stockTargetSuccesses);
        }
#endif
        if (absent && retirementSafe) {
            CC_SELECTION_DIAG("CustomComponents/Pick3/PVP_RETIRE: STATE_ABSENT effect=%d group=%d polls=%d (local state only; not server ACK)",
                     remoteRetirementEffect, remoteRetirementGroup, remoteRetirementPolls);
            CC_SELECTION_DIAG("CustomComponents/Pick3/PICK3_TRIGGER_PROVENANCE: phase=QUEUE_RETIRED_LOCAL_ONLY effect=%d group=%d remote=1 verifiedServerAck=0 dispatch=deferred",remoteRetirementEffect,remoteRetirementGroup);
            pendingRemoteRetirement = false;
        } else if (++remoteRetirementPolls == 600) {
            LOG_ERR("CustomComponents/Pick3/PVP_RETIRE: STILL_PENDING effect=%d group=%d (no authoritative confirmation)",
                    remoteRetirementEffect, remoteRetirementGroup);
            pendingRemoteRetirement = false;
        }
    }
    if (!pendingRefresh) return;
    // If stock subsequently completed a target command, never send a second
    // presentation update for an uncertain overlapping lifecycle.
    if (stockTargetSuccesses != pendingStockSuccessBaseline) {
        pendingRefresh=false;
        CC_DIAG("CustomComponents/Pick3/DEFAULT_REPAIR: REFRESH_CANCELED_STOCK_COMMAND");
        return;
    }
    if (!observedLocalTargetSubTurn) {
        // Stock, not a synthetic Signal.Dispatch, restored the normal play phase.
        CC_SELECTION_DIAG("CustomComponents/Pick3/PATCH154_STOCK_PHASE_RESTORED: effect=%d group=%d queued=%d capture=%d stockSuccess=%u signalDispatch=0",
                 pendingRefreshEffect, pendingRefreshGroup,
                 TargetBridge::HasQueuedChoice()?1:0, overlayCapturesInput?1:0,
                 stockTargetSuccesses);
        pendingRefresh=false;
        pendingRefreshAge=pendingStableFrames=0;
        return;
    }
    // Engine may need several frames to drain its normal effect processing.
    if (++pendingRefreshAge>600) {
        LOG_ERR("CustomComponents/Pick3/DEFAULT_REPAIR: REFRESH_EXPIRED effect=%d group=%d",pendingRefreshEffect,pendingRefreshGroup);
        pendingRefresh=false;return;
    }
    bool safe=false;
    if (!EffectAbsentFromQueue(Target(boundEffectsRoot),pendingRefreshEffect,pendingRefreshGroup,safe)) {
        pendingStableFrames=0;return;
    }
    if (++pendingStableFrames<2) return;
    Obj* signal=Target(boundSignalRoot);
    if (!safe || !signal || !gameStateUpdatedSignalClass.IsValid() ||
        !BNM::IsA(signal,gameStateUpdatedSignalClass) || !dispatchGameStateUpdated.IsValid()) return;
    // Patch 1.5.3: matched native crash mitigation.
    // libsprig BuildId e334967f... frame +0x2A6EB0 resolves to
    // BNM::TryInvoke(dispatchGameStateUpdated[signal]) in this path.
    // The stock signal can be present yet have invalid injected subscribers
    // or expired scene/model context. Never synthesize this event from a
    // post-selection timer; normal stock GameStateUpdated remains untouched.
    const int effect=pendingRefreshEffect, group=pendingRefreshGroup;
    // Keep the ticket until STOCK naturally exits the target subturn.
    // Do not invoke GameStateUpdated or mutate the game's input/command state.
    if (pendingStableFrames==2)
        CC_SELECTION_DIAG("CustomComponents/Pick3/PATCH154_WAIT_STOCK_PHASE: effect=%d group=%d targetSubturn=%d queueRetired=1 signalDispatch=0",
                 pendingRefreshEffect, pendingRefreshGroup, observedLocalTargetSubTurn?1:0);
    pendingStableFrames=0;
    CC_SELECTION_DIAG("CustomComponents/Pick3/CRASH_FIX_153: SYNTHETIC_REFRESH_SKIPPED effect=%d group=%d reason=matched-native-0x2A6EB0", effect, group);

}
// One-time presentation-only resync. The engine owns all legal-play decisions:
// we call its existing HighlightPlayableCardsCommand with a fully injected,
// match-scoped instance, never dispatch playable signals based on our own guess.
// No Play, model mutation, command Retain/Release, or visual subturn forcing.
// Stock-owned highlight lifecycle observer. No injected command construction.
inline unsigned stockHighlightObservations = 0;
// Returned lists are ordinary managed List<int>, not pooled validator lists.
// Read only their bounded in-memory snapshot. Never invoke collection methods.
using StockPlayableCardsFn = IntList*(*)(Obj*, const void*);
inline StockPlayableCardsFn originalStockPlayableCards = nullptr;
using StockShouldHighlightFn = bool(*)(Obj*, const void*);
inline StockShouldHighlightFn originalStockShouldHighlight = nullptr;
inline unsigned stockPlayableAuditSamples = 0;
inline unsigned stockShouldHighlightSamples = 0;
inline IntList* StockPlayableCardsObserved(Obj* self, const void* method) {
    auto* result = originalStockPlayableCards ? originalStockPlayableCards(self, method) : nullptr;
    if (++stockPlayableAuditSamples <= 240) {
        int n = -1, ids[12]{};
        if (result && result->size >= 0 && result->size <= 512 &&
            (result->size == 0 || result->items)) {
            n = result->size;
            for (int i=0; i<std::min(n,12); ++i) ids[i] = result->items->m_Items[i];
        }
        CC_SELECTION_DIAG("CustomComponents/Pick3/PLAYABLE_AUDIT: phase=STOCK_LIST cache=%p count=%d first12=%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d effect=%d group=%d",
                 self,n,ids[0],ids[1],ids[2],ids[3],ids[4],ids[5],ids[6],ids[7],ids[8],ids[9],ids[10],ids[11],lastRefreshEffect,lastRefreshGroup);
    }
    return result;
}
inline bool StockShouldHighlightObserved(Obj* self, const void* method) {
    const bool result = originalStockShouldHighlight ? originalStockShouldHighlight(self,method) : false;
    if (++stockShouldHighlightSamples <= 240) {
        Obj* cache = (self && highlightCommandPlayCache.IsValid()) ? highlightCommandPlayCache[self].Get() : nullptr;
        CC_SELECTION_DIAG("CustomComponents/Pick3/PLAYABLE_AUDIT: phase=DISPLAY_PERMISSION command=%p cache=%p permitted=%d effect=%d group=%d",
                 self,cache,result?1:0,lastRefreshEffect,lastRefreshGroup);
    }
    return result;
}
// Observe the actual effect chosen by stock. This is a borrowed Entity pointer;
// only its stable Id is read while the original call's result remains alive.
using FirstTargetedEffectFn = Obj*(*)(Obj*, int, const void*);
inline FirstTargetedEffectFn originalFirstTargetedEffect = nullptr;
inline unsigned firstTargetedEffectSamples = 0;
inline Obj* FirstTargetedEffectObserved(Obj* self, int cardId, const void* method) {
    Obj* effect = originalFirstTargetedEffect ? originalFirstTargetedEffect(self,cardId,method) : nullptr;
    if (trickValidationDepth && IsGenericPick3ReviveBurst(cardId) && ++firstTargetedEffectSamples <= 160) {
        int effectId = -1;
        if (effect && trickEntityIdField.IsValid()) effectId = trickEntityIdField[effect].Get();
        CC_SELECTION_DIAG("CustomComponents/Pick3/RETURN_TO_LANE_METADATA: phase=FIRST_TARGETED card=%d guid=%d effectId=%d found=%d",
                 cardId,trickValidationGuid,effectId,effect?1:0);
    }
    return effect;
}

// Only observe native call boundaries; never inspect PoolableList internals.
using StockCachePlaysFn = void(*)(Obj*, Obj*, const void*);
inline StockCachePlaysFn originalStockCachePlays = nullptr;
inline Execute originalStockRefreshPlayable = nullptr;
using StockHighlightUpdateFn = void(*)(Obj*, Obj*, const void*);
inline StockHighlightUpdateFn originalStockHighlightUpdate = nullptr;
// Snapshot stock PlayCache.PlayableCards at its declared IL2CPP field offset
// from the already installed RefreshPlayableCardsList observer.  No additional
// native detour or managed ICollection call is required.
inline BNM::Field<IntList*> stockPlayableCardsField;
inline unsigned stockSafePlayableSnapshots = 0;
inline void AuditPlayableCardsFromRefresh(Obj* cache) {
    if (!cache || !stockPlayableCardsField.IsValid() ||
        lastRefreshEffect < 0 || stockSafePlayableSnapshots++ >= 160) return;
    auto* list = stockPlayableCardsField[cache].Get();
    if (!list) {
        CC_SELECTION_DIAG("CustomComponents/Pick3/PLAYABLE_AUDIT: phase=REFRESH_FIELD cache=%p list=null effect=%d group=%d",cache,lastRefreshEffect,lastRefreshGroup);
        return;
    }
    const int n = list->size;
    if (n < 0 || n > 128 || (n && !list->items)) {
        CC_SELECTION_DIAG("CustomComponents/Pick3/PLAYABLE_AUDIT: phase=REFRESH_FIELD cache=%p invalidCount=%d effect=%d group=%d",cache,n,lastRefreshEffect,lastRefreshGroup);
        return;
    }
    int ids[12]{};
    for(int i=0;i<std::min(n,12);++i) ids[i] = list->items->m_Items[i];
    CC_SELECTION_DIAG("CustomComponents/Pick3/PLAYABLE_AUDIT: phase=REFRESH_FIELD cache=%p count=%d first12=%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d effect=%d group=%d",
             cache,n,ids[0],ids[1],ids[2],ids[3],ids[4],ids[5],ids[6],ids[7],ids[8],ids[9],ids[10],ids[11],lastRefreshEffect,lastRefreshGroup);
}
inline unsigned stockCacheTimelineSamples = 0;
inline thread_local unsigned stockCacheTimelineDepth = 0;
inline void StockCachePlaysObserved(Obj* self, Obj* plays, const void* method) {
    const bool sample = ++stockCacheTimelineSamples <= 300;
    if (sample) CC_SELECTION_DIAG("CustomComponents/Pick3/STOCK_PLAYCACHE: phase=CACHE_ENTER seq=%u cache=%p plays=%p", stockCacheTimelineSamples,self,plays);
    ++stockCacheTimelineDepth;
    if (originalStockCachePlays) originalStockCachePlays(self,plays,method);
    --stockCacheTimelineDepth;
    if (sample) CC_SELECTION_DIAG("CustomComponents/Pick3/STOCK_PLAYCACHE: phase=CACHE_EXIT seq=%u cache=%p",stockCacheTimelineSamples,self);
}
inline void StockRefreshPlayableObserved(Obj* self, const void* method) {
    const bool sample = stockCacheTimelineSamples <= 300;
    if (sample) CC_SELECTION_DIAG("CustomComponents/Pick3/STOCK_PLAYCACHE: phase=REFRESH_ENTER cache=%p nested=%u",self,stockCacheTimelineDepth);
    if (originalStockRefreshPlayable) originalStockRefreshPlayable(self,method);
    AuditPlayableCardsFromRefresh(self);
    if (sample) CC_SELECTION_DIAG("CustomComponents/Pick3/STOCK_PLAYCACHE: phase=REFRESH_EXIT cache=%p",self);
}
inline void StockHighlightUpdateObserved(Obj* self, Obj* allCards, const void* method) {
    if (stockHighlightObservations < 300)
        CC_SELECTION_DIAG("CustomComponents/Pick3/HAND_HIGHLIGHTS: phase=STOCK_UPDATE_ENTER command=%p cardsEnumerable=%p", self,allCards);
    if (originalStockHighlightUpdate) originalStockHighlightUpdate(self,allCards,method);
    // Distinguish a UI update from a stock PlayCache rebuild; a successful
    // UpdateCardsHighlightState does not mean the resulting signals are correct.
    if (stockHighlightObservations < 300 && lastRefreshEffect >= 0)
        CC_SELECTION_DIAG("CustomComponents/Pick3/HAND_HIGHLIGHTS: phase=POST_SELECTION_UPDATE effect=%d group=%d stockExecuteOrdinal=%u cacheNesting=%u",
                 lastRefreshEffect,lastRefreshGroup,stockHighlightSinceRefresh,stockCacheTimelineDepth);
    if (stockHighlightObservations < 300)
        CC_SELECTION_DIAG("CustomComponents/Pick3/HAND_HIGHLIGHTS: phase=STOCK_UPDATE_EXIT command=%p", self);
}
inline Execute originalStockHighlightExecute = nullptr;
inline void StockHighlightExecuteObserved(Obj* self, const void* method) {
    if (stockHighlightObservations < 240) {
        ++stockHighlightObservations;
        if (lastRefreshEffect >= 0 && stockHighlightSinceRefresh < 3) {
            CC_SELECTION_DIAG("CustomComponents/Pick3/HAND_HIGHLIGHTS: phase=POST_SELECTION_STOCK_EXECUTE effect=%d group=%d ordinal=%u command=%p",
                     lastRefreshEffect,lastRefreshGroup,++stockHighlightSinceRefresh,self);
        }
        CC_SELECTION_DIAG("CustomComponents/Pick3/HAND_HIGHLIGHTS: phase=STOCK_EXECUTE_ENTER command=%p sample=%u",
                 self, stockHighlightObservations);
    }
    if (originalStockHighlightExecute) originalStockHighlightExecute(self, method);
    if (stockHighlightObservations <= 240)
        CC_SELECTION_DIAG("CustomComponents/Pick3/HAND_HIGHLIGHTS: phase=STOCK_EXECUTE_EXIT command=%p sample=%u",
                 self, stockHighlightObservations);
}
inline void PumpHandHighlights(Obj* holder) {
    // Deliberate fail-closed safety guard until a stock-owned command lifecycle
    // is established. This suppresses the experimental manual Execute path.
    (void)holder;
    pendingHandHighlights=false;
    return;
#if 0
    if (!enabled || !pendingHandHighlights || !holder) return;
    if (++pendingHandHighlightAge > 180) {
        if (NetworkTrace::Mode()) CC_SELECTION_DIAG("CustomComponents/Pick3/HAND_HIGHLIGHTS: EXPIRED no valid stock highlight command/state");
        pendingHandHighlights=false;
        return;
    }
    if (pendingRefresh || overlayCapturesInput || TargetBridge::HasQueuedChoice() ||
        !stateVisualTurnPhase.IsValid() || !visualSubTurn.IsValid() ||
        !holderInputState.IsValid() || !highlightPlayableCardsClass.IsValid() ||
        !executeHighlightPlayableCards.IsValid()) {
        pendingHandHighlightStable=0;
        return;
    }
    Obj* phase=stateVisualTurnPhase[holder].Get();
    if (!phase || visualSubTurn[phase].Get()==1) {
        pendingHandHighlightStable=0;
        return;
    }
    const int state=holderInputState[holder].Get();
    // These are the game's "SelectCardToPlayInactive/Active" states, not a
    // target selection or ongoing animation. Stock command decides legality.
    if (state!=3 && state!=4) {
        pendingHandHighlightStable=0;
        return;
    }
    if (++pendingHandHighlightStable<2) return;
    Obj* binder=Target(binderRoot);
    if (!binder || !getInjectionBinder.IsValid() || !getInjectedInstance.IsValid()) return;
    Obj* injector=nullptr;
    const auto injectorException=BNM::TryInvoke([&] {injector=getInjectionBinder[binder]();});
    if (injectorException.IsValid() || !injector) return;
    Obj* command=nullptr;
    const auto instanceException=BNM::TryInvoke([&] {
        command=getInjectedInstance[injector](highlightPlayableCardsClass.GetMonoType(),static_cast<Obj*>(nullptr));
    });
    if (instanceException.IsValid() || !command || !BNM::IsA(command,highlightPlayableCardsClass)) return;
    // Only invoke an injected command with ALL stock dependencies available.
    if (!highlightCommandInputState.IsValid() || !highlightCommandPlayCache.IsValid() ||
        !highlightCommandModel.IsValid() || !highlightCommandFacade.IsValid() ||
        !highlightCommandPlayable.IsValid() || !highlightCommandUnplayable.IsValid() ||
        !highlightCommandPlayCache[command].Get() || !highlightCommandModel[command].Get() ||
        !highlightCommandFacade[command].Get() || !highlightCommandPlayable[command].Get() ||
        !highlightCommandUnplayable[command].Get()) return;
    // Respect the injected command's state instead of rewriting pooled data.
    if (highlightCommandInputState[command].Get()!=state) {
        if (NetworkTrace::Mode() && pendingHandHighlightAge==15)
            CC_SELECTION_DIAG("CustomComponents/Pick3/HAND_HIGHLIGHTS: WAIT_STATE actual=%d expected=%d",
                highlightCommandInputState[command].Get(),state);
        return;
    }
    // Consume before calling managed code because dispatching can re-enter.
    pendingHandHighlights=false;
    const auto ex=BNM::TryInvoke([&] {executeHighlightPlayableCards[command]();});
    if (ex.IsValid()) {
        LOG_ERR("CustomComponents/Pick3/HAND_HIGHLIGHTS: stock Execute exception: %s", ex.Message().c_str());
    } else {
        ++handHighlightRefreshCount;
        if (NetworkTrace::Mode())
            CC_SELECTION_DIAG("CustomComponents/Pick3/HAND_HIGHLIGHTS: STOCK_REFRESH state=%d count=%u",state,handHighlightRefreshCount);
    }
#endif
}

// Reconstruct the native PickedFromThree query context from a received stock
// Play.Type=SelectTarget BEFORE the local game service evaluates its effects.
// Both PvP peers may run this method; only real custom effects are handled.
}
