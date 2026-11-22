#include "diagnostics/DiagnosticLogging.hpp"
#pragma once
// Private implementation module. Include PickFromThreeSubmission.hpp, not this file.
namespace CustomComponents::Pick3::Submission {
inline bool syncLocalCompletion = true;
inline BNM::Class gameStateUpdatedSignalClass;
inline BNM::Field<Obj*> playCardStateUpdatedSignal;
inline BNM::Field<Obj*> serviceOnSuccess;
inline BNM::Method<void> dispatchGameStateUpdated;
inline std::uint32_t updatedSignalRoot = 0;
// Capture a genuine SelectTargetCommand signal only while our modal intercepts
// that command. This is a presentation fallback, NOT stock command completion.
inline std::uint32_t interceptedTargetSignalRoot = 0;
inline unsigned int interceptedStockCommands = 0;
// A rooted identity for the suppressed stock command, scoped to one popup.
// Never invoke Execute/Release on it: suppressing Execute means Retain may
// not have happened. This lets later traces distinguish matching service
// ownership from an unrelated target selection.
inline std::uint32_t interceptedCommandRoot = 0;
inline std::uint32_t interceptedCommandServiceRoot = 0;
inline unsigned int commandOwnershipMismatches = 0;
inline unsigned int signalDispatchCount = 0;
inline Push originalPush = nullptr;
inline Execute originalExecute = nullptr;
inline Execute originalSelectTargetAction = nullptr;
inline unsigned int suppressedStockSelectionActions = 0;
// PvP submission is locally predicted before remote state is observed.
// This ticket is *observational*: never synthesize an ACK or mutate gameplay.
inline bool pendingRemoteRetirement = false;
inline int remoteRetirementEffect = -1, remoteRetirementGroup = -1;
inline int remoteRetirementPolls = 0;
inline bool overlayCapturesInput = false;
inline unsigned int postSubmitInputBudget = 0;
inline int postSubmitEffect = -1, postSubmitGroup = -1;
inline unsigned int blockedStockClicks = 0;
inline BNM::Class localClass, remoteClass, playClass, commandClass, stackClass, timerClass;
inline BNM::Class serviceFactoryClass, playCardCommandClass;
inline BNM::Class gameInputHolderClass, gameCommandBinderClass;
inline BNM::Class selectServiceInterfaceClass;
inline BNM::Field<Obj*> stateCommandBinder, stateGameModel;
// Passive ownership check: inspect the already-injected model in the existing
// GameInputStateHolder observer. No EndSubTurn or tiny setter detours.
inline BNM::Field<Obj*> stateVisualTurnPhase;
inline BNM::Field<int> visualSubTurn, opponentPlayerField;
inline bool observedLocalTargetSubTurn = false;
inline bool observedVisualPhase = false;
inline bool pvpOwnerGateReady = false;
// Stock-protocol completion and remote-state repairs are ALWAYS enabled.
// Only NETTRACE output remains ADB-controlled. No extra Play is submitted.
inline BNM::Class effectCollectionInterface, effectQueueConcrete, boundEntityModelClass;
inline BNM::Class chooseBlockingClass;
inline BNM::Field<BNM::Structures::Mono::List<Obj*>*> boundQueueEffects, boundQueueResponses, boundQueueGroupResponses;
inline BNM::Field<Obj*> localServiceFinder;
inline std::uint32_t boundSignalRoot=0, boundEffectsRoot=0, boundEntityModelRoot=0;
inline std::uint32_t reviveLegalPlayModelRoot=0; // rooted and reset on match/binder replacement
inline unsigned int repairDependencyPoll=0, repairSuccesses=0, remotePickRebuilds=0;
inline bool pendingRefresh=false;
// Default-on: the stock playable-card highlight command normally runs when
// GameInputState changes. A custom picker can finish without triggering that
// command until the next hand interaction. Refresh the stock highlights once
// AFTER the targeting phase returns to a normal hand-input state.
inline BNM::Class highlightPlayableCardsClass;
inline BNM::Method<void> executeHighlightPlayableCards;
inline BNM::Field<int> highlightCommandInputState, holderInputState;
inline BNM::Field<Obj*> highlightCommandPlayCache, highlightCommandModel,
    highlightCommandFacade, highlightCommandPlayable, highlightCommandUnplayable;
inline bool pendingHandHighlights=false;
inline int pendingHandHighlightAge=0, pendingHandHighlightStable=0;
inline unsigned int handHighlightRefreshCount=0;
inline int pendingRefreshEffect=-1, pendingRefreshGroup=-1, pendingStableFrames=0, pendingRefreshAge=0;
inline unsigned int pendingStockSuccessBaseline=0;
inline void (*originalLocalSelectProcess)(Obj*, Obj*, const void*) = nullptr;
inline void PumpDefaultRepair();
inline void PumpHandHighlights(Obj* holder);
inline void ResolveRepairDependencies(bool force=false);
inline bool EffectAbsentFromQueue(Obj* queue, int effect, int group, bool& safe);

inline BNM::Method<Obj*> getInjectionBinder, getInjectedInstance;
inline BNM::Field<int> localPlayerField, playType, playPlayer, playCard, playOrder;
inline BNM::Field<IntList*> playTargets;
inline BNM::Field<Obj*> commandModel, commandService, playCardCommandModel;
// Read-only local service queue verification. The game reports
// ServiceFailure::NoBlockingEffect (16) if Process() has no active front.
inline BNM::Field<Obj*> localServiceEffects;
inline BNM::Method<Obj*> peekLocalEffect;
inline BNM::Field<int> liveEffectEntityId, liveEffectGroupId;
// Stock model-side target commitment observer. The ARM64 handler at
// 0x21FE8C8 dispatches to CreateEffectsForTargets (0x21FE910), which
// updates the effect entity and regenerates downstream effects. This is
// distinct from the local SelectTarget service completion callback.
using Pick3ModelSelectFn = void(*)(Obj*, Obj*, const void*);
inline Pick3ModelSelectFn originalPick3ModelSelect = nullptr;
inline bool ValidateAcceptedSelectionCards(Obj* model,const std::vector<Picked::SelectedCard>& cards) {
    if(!model || cards.empty() || cards.size()>3 || !TargetBridge::getEntityById.IsValid()) return false;
    static auto identityField=BNM::Class("PvZCards.Engine","Entity").GetField("<Id>k__BackingField").cast<int>();
    if(!identityField.IsValid()) return false;
    for(const auto& card:cards) {
        if(card.entityId==-1) {if(card.cardGuid<=0) return false;continue;}
        if(card.entityId<=0 || card.cardGuid<=0) return false;
        Obj* entity=TargetBridge::getEntityById[model](card.entityId);
        const int guid=CustomComponents::Queries::InDeckCardGuidOf(entity);
        const int actualId=entity?identityField[entity].Get():-1;
        if(!MatchesPhysicalSelection(card.entityId,card.cardGuid,actualId,guid)) {
            LOG_WARN("CustomComponents/Pick3/PF3_TX: phase=ACCEPTED_CARD_MISMATCH entity=%d expectedGuid=%d actualGuid=%d",card.entityId,card.cardGuid,guid);
            return false;
        }
    }
    return true;
}
inline void Pick3ModelSelectHook(Obj* self, Obj* selected, const void* method) {
    int effectId = -1, groupId = -1, phase = -1, targetCount = -1, first = -1;
    bool revive = false;
    if (selected && liveEffectEntityId.IsValid() && liveEffectGroupId.IsValid()) {
        effectId = liveEffectEntityId[selected].Get();
        groupId = liveEffectGroupId[selected].Get();
        revive = effectId >= 0 && IsGenericPick3ReviveEffect(effectId);
        if (revive) {
            const auto* bytes = reinterpret_cast<const std::uint8_t*>(selected);
            phase = *reinterpret_cast<const std::int32_t*>(bytes + 0x50);
            Obj* targets = *reinterpret_cast<Obj* const*>(bytes + 0x48);
            if (targets) {
                const auto* list = reinterpret_cast<const IntList*>(targets);
                if (list->size >= 0 && list->size <= 32 &&
                    (list->size == 0 || list->items)) {
                    targetCount = list->size;
                    if (targetCount > 0) first = list->items->m_Items[0];
                }
            }
            CC_SELECTION_DIAG("CustomComponents/Pick3/PICK3_REVIVE_MODEL_COMMIT: phase=ENTER effect=%d group=%d selectPhase=%d count=%d first=%d system=%p selection=%p",effectId,groupId,phase,targetCount,first,self,selected);
        }
    }
    // Observe stock SelectTargetEffect state across the already-owned detour.
    // Read-only; this cannot by itself prove an authoritative commit.
    const auto snapshotSelect=[&](const char* stage) {
        int state=-1, count=-1, firstTarget=-1;
        if(selected) {
            const auto* bytes=reinterpret_cast<const std::uint8_t*>(selected);
            state=*reinterpret_cast<const std::int32_t*>(bytes+0x50);
            Obj* listObj=*reinterpret_cast<Obj* const*>(bytes+0x48);
            if(listObj) {
                const auto* list=reinterpret_cast<const IntList*>(listObj);
                if(list->size>=0 && list->size<=32 && (list->size==0 || list->items)) {
                    count=list->size;
                    if(count) firstTarget=list->items->m_Items[0];
                }
            }
        }
        QueryCaller::ObserveSelectState(stage,effectId,groupId,state,count,firstTarget);
    };
    snapshotSelect("BEFORE_STOCK");
    Picked::ObserveStockModelBoundary(effectId,groupId,false,
        selected && ExactRouting::stockCancelled.IsValid(),
        selected && ExactRouting::stockCancelled.IsValid() && ExactRouting::stockCancelled[selected].Get());
    const auto provenanceSeq=Picked::UniqueObservedSequence(effectId,groupId);
    if constexpr(Diagnostics::kSelectionTrace) NetworkTrace::ObserveExistingSelectEnter(effectId,groupId,self,selected);
    CustomComponents::Pick3::Provenance::ScopedObservation provenanceScope(
        effectId,groupId,provenanceSeq,"EXISTING_SELECT_TARGET_PROCESS");
    if (originalPick3ModelSelect) originalPick3ModelSelect(self, selected, method);
    else LOG_ERR("CustomComponents/Pick3/PATCH2_PROVENANCE: phase=ORIGINAL_MISSING boundary=EXISTING_SELECT_TARGET_PROCESS");
    snapshotSelect("AFTER_STOCK");
    Picked::ObserveStockModelBoundary(effectId,groupId,true,
        selected && ExactRouting::stockCancelled.IsValid(),
        selected && ExactRouting::stockCancelled.IsValid() && ExactRouting::stockCancelled[selected].Get());
    NetworkTrace::ObserveExistingSelectExit(effectId,groupId,provenanceSeq);
    if (revive)
        CC_SELECTION_DIAG("CustomComponents/Pick3/PICK3_REVIVE_MODEL_COMMIT: phase=EXIT effect=%d group=%d selectPhase=%d count=%d first=%d",effectId,groupId,phase,targetCount,first);
}

inline BNM::Method<bool> liveEffectIsResolved;
inline BNM::Class liveBlockingEffectClass;
// A multi-group effect may be queued behind a synthetic (-1) front.  A
// front-only activation check cannot distinguish that legitimate state from
// an unrelated early popup.  Only permit the queued group when it is actually
// present in the concrete local EffectQueue and the current front is a
// placeholder, never while another real blocking effect owns the front.
// Non-mutating effect inspection. Never invoke EffectQueue.Peek(): the prior
// build could terminate inside that managed call. Snapshot only concrete lists.
enum class BlockingQueueState { Unknown, Front, PlaceholderQueued, Queued, Absent };
inline BlockingQueueState InspectBlockingQueueObject(const PreviewRequest& request, Obj* queue) {
    if (!queue || !effectQueueConcrete.IsValid() ||
        !boundQueueEffects.IsValid() || !boundQueueResponses.IsValid() ||
        !boundQueueGroupResponses.IsValid() || !liveEffectEntityId.IsValid() ||
        !liveEffectGroupId.IsValid() || !BNM::IsA(queue, effectQueueConcrete))
        return BlockingQueueState::Unknown;
    auto* effects = boundQueueEffects[queue].Get();
    auto* responses = boundQueueResponses[queue].Get();
    auto* groups = boundQueueGroupResponses[queue].Get();
    const auto valid=[](BNM::Structures::Mono::List<Obj*>* list) {
        return list && list->size >= 0 && list->size <= 4096 &&
               (!list->size || list->items);
    };
    if (!valid(effects) || !valid(responses) || !valid(groups)) return BlockingQueueState::Unknown;
    bool found=false, firstReal=false, firstPlaceholder=false, firstMatch=false;
    const auto scan=[&](BNM::Structures::Mono::List<Obj*>* list, int listId) {
        for(int i=0;i<list->size;i++) {
            Obj* e=list->items->m_Items[i];
            if (!e) continue;
            // Queue lists may contain generic effects; both fields belong to Effect.
            const int id=liveEffectEntityId[e].Get();
            const int group=liveEffectGroupId[e].Get();
            const bool match=id==request.effectEntityId && group==request.effectGroupId;
            if(match) found=true;
            if(listId==0 && i==0) {
                firstReal=id>=0;
                firstPlaceholder=id<0;
                firstMatch=match;
            }
        }
    };
    scan(effects,0);scan(responses,1);scan(groups,2);
    // A front choice is not ready while stock response work is still queued.
    // Do not mutate queues: wait for their normal merge/completion lifecycle.
    const bool pendingResponseWork = responses->size != 0 || groups->size != 0;
    BlockingQueueState state = firstMatch && !pendingResponseWork ? BlockingQueueState::Front :
        found ? BlockingQueueState::Queued : BlockingQueueState::Absent;
    if (firstMatch && pendingResponseWork) {
        static int lastDeferredEffect=-1, lastDeferredGroup=-1;
        if (lastDeferredEffect!=request.effectEntityId || lastDeferredGroup!=request.effectGroupId) {
            lastDeferredEffect=request.effectEntityId;
            lastDeferredGroup=request.effectGroupId;
            CC_SELECTION_DIAG("CustomComponents/Pick3/QUEUE_RESPONSE_BARRIER: effect=%d group=%d responses=%d groupResponses=%d action=WAIT_FOR_STOCK_COMPLETION",
                request.effectEntityId,request.effectGroupId,responses->size,groups->size);
        }
    }
    // Emit only on changes, and include identity to catch multi-trigger reuse.
    static int lastEffect=-1,lastGroup=-1,lastState=-1;
    if(lastEffect!=request.effectEntityId || lastGroup!=request.effectGroupId || lastState!=static_cast<int>(state)) {
        lastEffect=request.effectEntityId;lastGroup=request.effectGroupId;lastState=static_cast<int>(state);
        CC_SELECTION_DIAG("CustomComponents/Pick3/QUEUE_INSPECT: effect=%d group=%d state=%d frontReal=%d frontPlaceholder=%d counts=%d/%d/%d noPeek=1",
          request.effectEntityId,request.effectGroupId,lastState,firstReal?1:0,firstPlaceholder?1:0,
          effects->size,responses->size,groups->size);
    }
    return state;
}
// PvP's injected IEffectCollection and PvE's service may expose the same
// concrete EffectQueue. Never infer activation from a pending offer alone.
inline BlockingQueueState InspectBlockingQueue(const PreviewRequest& request, Obj* service) {
    if (!service || !localServiceEffects.IsValid()) return BlockingQueueState::Unknown;
    return InspectBlockingQueueObject(request, localServiceEffects[service].Get());
}
inline Obj* Target(std::uint32_t root);
inline BlockingQueueState InspectBoundBlockingQueue(const PreviewRequest& request) {
    return InspectBlockingQueueObject(request, Target(boundEffectsRoot));
}
inline bool QueuedBehindPlaceholder(const PreviewRequest& request, Obj* service) {
    return InspectBlockingQueue(request,service)==BlockingQueueState::PlaceholderQueued;
}
// A placeholder at the head means stock has NOT advanced to the requested
// blocking choice.  Do not activate a picker (including AUTO_SOLE) merely
// because the choice occurs later in the concrete queue.
inline bool LocalBlockingEffectMatches(const PreviewRequest& request, Obj* service) {
    const auto state=InspectBlockingQueue(request,service);
    if(state==BlockingQueueState::Front) return true;
    static int lastEffect=-1,lastGroup=-1,lastState=-1;
    if(lastEffect!=request.effectEntityId || lastGroup!=request.effectGroupId ||
       lastState!=static_cast<int>(state)) {
        lastEffect=request.effectEntityId;
        lastGroup=request.effectGroupId;
        lastState=static_cast<int>(state);
        CC_SELECTION_DIAG("CustomComponents/Pick3/QUEUE_TURN: action=DEFER effect=%d group=%d state=%d requiresExactFront=1",
            request.effectEntityId,request.effectGroupId,lastState);
    }
    return false;
}

struct TimerParameters { bool isForLocalPlayer; bool isForMulligans; };
inline BNM::Field<bool> timerRunning, timerPaused;
inline BNM::Field<TimerParameters> timerParameters;
inline bool enabled = false;
inline bool allowPvp = false;
inline bool allowPve = false;
inline int deadlineThresholdMs = 600;
inline std::uint32_t deadlineTimerRoot = 0;
inline Core::MatchGeneration matchGeneration;
inline std::uint32_t serviceRoot = 0, modelRoot = 0, stackRoot = 0, binderRoot = 0;
// Read the exact blocking Effect while the game still owns it, before Process.
// This root is ONE transaction's trigger context, not a global last-play id.
inline std::uint32_t selectedTriggerEffectRoot = 0;
inline int selectedTriggerEffectId = -1, selectedTriggerGroupId = -1;
inline CustomComponents::Systems::CustomTriggerSystem::Pick3EffectIdentitySnapshot selectedTriggerIdentity{};
// Immutable single-live choice validated against the stock candidate snapshot
// BEFORE dispatch. Do not revalidate against a mutable offer after Process().
struct VerifiedLiveChoice {
    int effectId = -1;
    int groupId = -1;
    int entityId = -1;
    int cardGuid = -1;
    bool valid = false;
};
inline VerifiedLiveChoice verifiedLiveChoice{};
inline void LogPick3EffectLifetime(const char* phase, Obj* effect) {
    const auto now = CustomComponents::Systems::CustomTriggerSystem::ReadPick3EffectIdentity(effect);
    CC_SELECTION_DIAG("CustomComponents/Pick3/PICK3_EFFECT_LIFETIME: phase=%s ptr=%p valid=%d effectEntity=%d group=%d source=%d target=%d springboard=%d capturedEntity=%d capturedSource=%d",
        phase, (void*)effect, int(now.valid), now.effectEntityId, now.effectGroupId,
        now.sourceId, now.targetId, now.springboardId,
        selectedTriggerIdentity.effectEntityId, selectedTriggerIdentity.sourceId);
}

inline int lastEffect = -1, lastGroup = -1;

inline std::mt19937 rng{std::random_device{}()};

inline Obj* Target(std::uint32_t root) {
    return root && Core::SceneViewContext::getGCHandle ? Core::SceneViewContext::getGCHandle(root) : nullptr;
}
// Optional read-only entity identity probes, using the already rooted match EntityModel.
// No GetEntityById invocation (that overload takes a List<Entity>, not an int).
inline BNM::Field<BNM::Structures::Mono::List<Obj*>*> trickEntitiesField;
inline BNM::Field<BNM::Structures::Mono::List<Obj*>*> trickEntityComponentsField;
inline BNM::Field<int> trickEntityIdField;
// Inspect the current player's hand entity as optional supporting context.
// It is NOT interchangeable with the initial Play's stock burst target.
// This is a native, read-only component lookup; no pooled-list invocation.
inline int Pick3CastingHandOnModel(Obj* model, int player) {
    if (!model || player < 0 || !trickEntitiesField.IsValid() ||
        !trickEntityIdField.IsValid()) return -1;
    auto* entities = trickEntitiesField[model].Get();
    if (!entities || entities->size < 0 || entities->size > 20000 ||
        (entities->size && !entities->items)) return -1;
    static auto handAssocClass=BNM::Class("PvZCards.Engine.Components", "HandAssociation");
    static auto handIdField=handAssocClass.GetField("HandEntityId").cast<int>();
    if (!handAssocClass.IsValid() || !handIdField.IsValid()) return -1;
    for (int i=0; i<entities->size; ++i) {
        Obj* e=entities->items->m_Items[i];
        if (!e || trickEntityIdField[e].Get()!=player) continue;
        auto field=BNM::Class(e).GetField("Components")
            .cast<BNM::Structures::Mono::List<Obj*>*>();
        auto* comps=field.IsValid()?field[e].Get():nullptr;
        if (!comps || comps->size < 0 || comps->size > 128 ||
            (comps->size && !comps->items)) return -1;
        for (int j=0;j<comps->size;++j) {
            Obj* c=comps->items->m_Items[j];
            if (c && BNM::IsA(c,handAssocClass)) return handIdField[c].Get();
        }
        return -1;
    }
    return -1;
}
}
