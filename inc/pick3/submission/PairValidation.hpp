#include "diagnostics/DiagnosticLogging.hpp"
#pragma once
// Private implementation module. Include PickFromThreeSubmission.hpp, not this file.
namespace CustomComponents::Pick3::Submission {
inline void TrickLogIdentity(const char* stage, unsigned seq, int id) {
    if (++trickIdentityEvents > 650 || id < 0) return;
    Obj* model = Target(boundEntityModelRoot);
    if (!model || !trickEntitiesField.IsValid() || !trickEntityIdField.IsValid()) {
        CC_SELECTION_DIAG("CustomComponents/Pick3/TRICK_IDENTITY: phase=%s seq=%u entity=%d modelReady=0", stage, seq, id);
        return;
    }
    auto* entities = trickEntitiesField[model].Get();
    if (!entities || entities->size < 0 || entities->size > 20000 || (entities->size && !entities->items)) {
        CC_SELECTION_DIAG("CustomComponents/Pick3/TRICK_IDENTITY: phase=%s seq=%u entity=%d listReady=0", stage, seq, id);
        return;
    }
    Obj* entity = nullptr;
    for (int i=0; i<entities->size; ++i) {
        Obj* candidate = entities->items->m_Items[i];
        if (candidate && trickEntityIdField[candidate].Get() == id) { entity=candidate; break; }
    }
    int guid = -1;
    if (entity && TargetBridge::getComponent.IsValid() && TargetBridge::cardClass.IsValid() && TargetBridge::databaseCardGuid.IsValid()) {
        Obj* card=nullptr;
        const auto ex=BNM::TryInvoke([&] { card=TargetBridge::getComponent[entity](TargetBridge::cardClass.GetMonoType()); });
        if (!ex.IsValid() && card) guid=TargetBridge::databaseCardGuid[card].Get();
    }
    if (stage && stage[0] == 'D' && stage[1] == 'R' && stage[2] == 'A' && stage[3] == 'G') trickSecondaryFocusGuid = guid;
    CC_SELECTION_DIAG("CustomComponents/Pick3/TRICK_IDENTITY: phase=%s seq=%u entity=%d found=%d cardGuid=%d cardComponent=%d modelReady=1", stage, seq, id, entity?1:0, guid, guid>0?1:0);
}
inline void TrickLogTargetsIdentity(const char* stage, unsigned seq, IntList* list) {
    if (!list || list->size < 0 || list->size > 4096 || (list->size && !list->items)) return;
    const int count = std::min(list->size, 12);
    for (int i=0; i<count; ++i) TrickLogIdentity(stage, seq, list->items->m_Items[i]);
}

// Stock valid-play enumeration, observation only. PoolableList<T> derives from List<T>.
// Never return, cache, or release pooled objects outside their owner.
using TargetGenerationFn = Obj*(*)(Obj*, int, int, Obj*, const void*);
inline TargetGenerationFn originalDeciderValidTargets = nullptr;
inline TargetGenerationFn originalDeciderValidPlays = nullptr;
inline unsigned trickGenerationEvents = 0;
inline int TrickGenerationGuid(int id);
inline bool IsGenericPick3ReviveBurst(int cardId);
// Read-only narrow validation trace. The concrete Validate implementations are
// individually hooked; the Validator interface itself has no native body.
using TrickOrderFn = int(*)(Obj*, Obj*, int, int, const void*);
using TrickValidateFn = bool(*)(Obj*, Obj*, Obj*, const void*);
inline TrickOrderFn originalTrickOrder = nullptr;
inline TrickValidateFn originalTrickCompositeValidate = nullptr;
inline TrickValidateFn originalTrickOneTimeValidate = nullptr;
inline TrickValidateFn originalTrickNullValidate = nullptr;
inline TrickValidateFn originalTrickInHandValidate = nullptr;
// Stock validator child observers: original results, untouched errors and Play.
inline TrickValidateFn originalTrickFighterValidate = nullptr;
inline TrickValidateFn originalTrickOpenLaneValidate = nullptr;
inline unsigned trickChildValidatorSamples = 0;

// Observers only: never fabricate targets or bypass the game's validator.
using TrickHasAnyTargetsFn = bool(*)(Obj*, int, Obj*, const void*);
inline TrickHasAnyTargetsFn originalTrickHasAnyTargets = nullptr;
inline unsigned trickHasAnyTargetSamples = 0;
inline bool TrickHasAnyTargetsObserved(Obj* self, int cardId, Obj* errors, const void* method) {
    const bool result = originalTrickHasAnyTargets ? originalTrickHasAnyTargets(self, cardId, errors, method) : false;
    if (trickHasAnyTargetSamples < 240) {
        ++trickHasAnyTargetSamples;
        // No managed error enumeration or pooled-object dereference.
        CC_SELECTION_DIAG("CustomComponents/Pick3/PICK3_REVIVE_GATE: phase=HAS_ANY_TARGETS card=%d validator=%p errors=%p accepted=%d sample=%u secondaryPairVerified=0",
                 cardId, self, errors, result ? 1 : 0, trickHasAnyTargetSamples);
    }
    return result;
}

inline thread_local unsigned trickValidationDepth = 0;
inline thread_local int trickValidationGuid = -1;
inline thread_local int trickValidationCard = -1;
inline thread_local unsigned trickOrderCalls = 0;
inline thread_local unsigned trickValidatorCalls = 0;
inline unsigned trickTraceGenerationCount = 0;
// Bounded, per-thread rejection matrix. Read only validator return values;
// do not invoke interface collections or retain managed objects.
struct Pick3Stage1ValidationMatrix {
    unsigned oneTimeCalls=0, oneTimeRejects=0;
    unsigned compositeCalls=0, compositeRejects=0;
    unsigned fighterCalls=0, fighterRejects=0;
    unsigned openLaneCalls=0, openLaneRejects=0;
    unsigned inHandCalls=0, inHandRejects=0;
    unsigned nullCalls=0, nullRejects=0;
    unsigned targetingCalls=0, targetingRejects=0;
};
inline thread_local Pick3Stage1ValidationMatrix pick3Stage1Matrix{};
inline void Pick3RecordStage1Validator(const char* type, bool accepted) {
    if (!trickValidationDepth || !IsGenericPick3ReviveBurst(trickValidationCard) || !type) return;
    unsigned* calls=nullptr;
    unsigned* rejects=nullptr;
    if (!std::strcmp(type,"OneTimeEffect")) { calls=&pick3Stage1Matrix.oneTimeCalls; rejects=&pick3Stage1Matrix.oneTimeRejects; }
    else if (!std::strcmp(type,"Composite")) { calls=&pick3Stage1Matrix.compositeCalls; rejects=&pick3Stage1Matrix.compositeRejects; }
    else if (!std::strcmp(type,"Fighter")) { calls=&pick3Stage1Matrix.fighterCalls; rejects=&pick3Stage1Matrix.fighterRejects; }
    else if (!std::strcmp(type,"OpenLane")) { calls=&pick3Stage1Matrix.openLaneCalls; rejects=&pick3Stage1Matrix.openLaneRejects; }
    else if (!std::strcmp(type,"InHand")) { calls=&pick3Stage1Matrix.inHandCalls; rejects=&pick3Stage1Matrix.inHandRejects; }
    else if (!std::strcmp(type,"Null")) { calls=&pick3Stage1Matrix.nullCalls; rejects=&pick3Stage1Matrix.nullRejects; }
    if (calls) { ++*calls; if (!accepted) ++*rejects; }
}

// Resolve the actual first owned effect from the SAME model as the stock
// OneTimeEffectValidator, not from a UI/selection model. No cached managed
// references or pooled target collections leave this synchronous scope.
struct Pick3InitialEffectSnapshot {
    int effectId = -1;
    Obj* primary = nullptr;
    Obj* secondary = nullptr;
    Obj* primaryQuery = nullptr;
    Obj* secondaryQuery = nullptr;
    bool complete = false;
};
inline Pick3InitialEffectSnapshot ResolvePick3InitialEffectOnModel(Obj* model, int cardId) {
    Pick3InitialEffectSnapshot result;
    const auto selected=Engine::PlayValidation::current;
    const bool scoped=selected.model==model && selected.card==cardId && selected.resolved;
    if(scoped && selected.targeted && !selected.ability)return result;
    if (!model || cardId < 0 || !trickEntitiesField.IsValid() ||
        !trickEntityIdField.IsValid() || !reviveEffectOwnerSourceField.IsValid()) return result;
    auto* entities = trickEntitiesField[model].Get();
    if (!entities || entities->size < 0 || entities->size > 20000 ||
        (entities->size && !entities->items)) return result;
    for (int i = 0; i < entities->size; ++i) {
        Obj* entity = entities->items->m_Items[i];
        if (!entity) continue;
        if(scoped && entity!=selected.ability)continue;
        const int id = trickEntityIdField[entity].Get();
        if (id < 0 || (result.effectId >= 0 && id >= result.effectId)) continue;
        auto componentField = BNM::Class(entity).GetField("Components")
            .cast<BNM::Structures::Mono::List<Obj*>*>();
        auto* components = componentField.IsValid() ? componentField[entity].Get() : nullptr;
        if (!components || components->size < 0 || components->size > 128 ||
            (components->size && !components->items)) continue;
        bool onPlay = false;
        Obj* owner = nullptr;
        Obj* primary = nullptr;
        Obj* secondary = nullptr;
        for (int j = 0; j < components->size; ++j) {
            Obj* c = components->items->m_Items[j];
            if (!c) continue;
            auto* klass = BNM::Class(c).GetClass();
            const char* name = klass ? klass->name : nullptr;
            if (!name) continue;
            if (!std::strcmp(name, "PlayTrigger")) onPlay = true;
            else if (!std::strcmp(name, "EffectEntity")) owner = c;
            else if (!std::strcmp(name, "PrimaryTargetFilter")) primary = c;
            else if (!std::strcmp(name, "SecondaryTargetFilter")) secondary = c;
        }
        if (!onPlay || !owner || reviveEffectOwnerSourceField[owner].Get() != cardId)
            continue;
        // Store the earliest owned PlayTrigger, including if it is malformed;
        // never silently skip to a later effect.
        result = {};
        result.effectId = id;
        result.primary = primary;
        result.secondary = secondary;
        result.primaryQuery = primary && TargetBridge::targetFilterQuery.IsValid()
            ? TargetBridge::targetFilterQuery[primary].Get() : nullptr;
        result.secondaryQuery = secondary && TargetBridge::targetFilterQuery.IsValid()
            ? TargetBridge::targetFilterQuery[secondary].Get() : nullptr;
        result.complete = primary && secondary && result.primaryQuery && result.secondaryQuery;
    }
    return result;
}

// This path deliberately supports ONLY the exact HasComponent(Lane) secondary
// query used by our custom revive cards. Any other query is unproven and
// remains stock-unplayable; never interpret an arbitrary secondary query as a
// blanket permission to revive into any lane.
inline bool Pick3SecondaryIsLaneOnly(Obj* query) {
    if (!query) return false;
    const auto* klass = BNM::Class(query).GetClass();
    if (!klass || !klass->name || std::strcmp(klass->name, "HasComponentQuery"))
        return false;
    static auto componentField = BNM::Class("PvZCards.Engine.Queries", "HasComponentQuery")
        .GetField("ComponentType").cast<Obj*>();
    static auto laneType = BNM::Class("PvZCards.Engine.Components", "Lane").GetMonoType();
    return componentField.IsValid() && laneType &&
        componentField[query].Get() == reinterpret_cast<Obj*>(laneType);
}

// Evaluate a secondary lane query through the game's actual EntityFinder
// rather than approximating nested And/Or/Not/owner query semantics. The
// finder creates its own EntityQueryContext for this exact model. This bridge
// only supports filters whose non-query modifiers do not impose additional
// stock-only restrictions; those remain fail-closed.
inline Pick3SecondaryDecision EvaluatePick3SecondaryQueryWithStockFinder(
    Obj* model, int effectId, Obj* secondaryFilter, Obj* secondaryQuery,
    int primaryCardId, int laneId, int playerId) {
    if (!model || !secondaryFilter || !secondaryQuery || effectId < 0 ||
        primaryCardId <= 0 || laneId <= 0 || playerId < 0)
        return Pick3SecondaryDecision::Unsupported;

    // Extra filter modifiers require the stock TargetEntityFinder + validator,
    // not just the EntityQuery.Matches traversal. Never grant permission for
    // an arbitrary additional-target filter from this bridge.
    const auto* filter = reinterpret_cast<const std::uint8_t*>(secondaryFilter);
    if (*reinterpret_cast<const int*>(filter + 0x2C) != 0 ||
        *reinterpret_cast<Obj* const*>(filter + 0x30) != nullptr ||
        *reinterpret_cast<const bool*>(filter + 0x38))
        return Pick3SecondaryDecision::Unsupported;

    static auto finderClass = BNM::Class("PvZCards.Engine", "EntityFinder");
    static auto findEntities = finderClass.GetMethod("FindEntities", 1)
        .cast<BNM::Structures::Mono::List<Obj*>*>();
    static auto queryContext = finderClass.GetField("QueryContext").cast<Obj*>();
    if (!finderClass.IsValid() || !findEntities.IsValid() ||
        !queryContext.IsValid())
        return Pick3SecondaryDecision::Unsupported;

    Obj* finder = nullptr;
    BNM::Structures::Mono::List<Obj*>* matches = nullptr;
    bool completed = false;
    const auto invoked = BNM::TryInvoke([&] {
        finder = finderClass.CreateNewObjectParameters(model);
        if (!finder) return;
        Obj* context = queryContext[finder].Get();
        if (!context) return;
        auto* contextBytes = reinterpret_cast<std::uint8_t*>(context);
        Obj* effectContext = *reinterpret_cast<Obj**>(contextBytes + 0x18);
        if (!effectContext) return;
        // Phase-1 stock query context: the discarded card is now the primary
        // target; the casting card is the source, and the effect is exact.
        *reinterpret_cast<int*>(contextBytes + 0x20) = primaryCardId;
        auto* effectBytes = reinterpret_cast<std::uint8_t*>(effectContext);
        *reinterpret_cast<int*>(effectBytes + 0x10) = primaryCardId;
        *reinterpret_cast<int*>(effectBytes + 0x14) = pick3PrimaryCaptureCard;
        *reinterpret_cast<int*>(effectBytes + 0x18) = effectId;
        matches = findEntities[finder](secondaryQuery);
        completed = matches != nullptr;
    });
    if (invoked.IsValid() || !completed) return Pick3SecondaryDecision::Unsupported;
    if (matches->size < 0 || matches->size > 20000 ||
        (matches->size && !matches->items))
        return Pick3SecondaryDecision::Unsupported;
    for (int i = 0; i < matches->size; ++i) {
        Obj* entity = matches->items->m_Items[i];
        if (entity && trickEntityIdField.IsValid() &&
            trickEntityIdField[entity].Get() == laneId)
            return Pick3SecondaryDecision::Allow;
    }
    return Pick3SecondaryDecision::Reject;
}

inline bool Pick3VerifyCapturedRevivePair(Obj* play, int* foundCard, int* foundLane,
                                          const char** reason) {
    if (foundCard) *foundCard = -1;
    if (foundLane) *foundLane = -1;
    if (reason) *reason = "NO_CAPTURE";
    if (play && pick3CurrentValidationModel && pick3PrimaryCaptureCard>=0 &&
        (deferredPrimaryCards.card!=pick3PrimaryCaptureCard || deferredPrimaryCards.ids.empty())) {
        PF3_REPAIR_TRACE("CustomComponents/Pick3/REPAIR1712: PRIMARY_MISSING card=%d effect=%d model=%p capturedCard=%d candidates=%zu decision=stockFallback_noSyntheticPair",
            pick3PrimaryCaptureCard,pick3PrimaryCaptureEffect,(void*)pick3CurrentValidationModel,
            deferredPrimaryCards.card,deferredPrimaryCards.ids.size());
    }
    if (!play || !pick3PhysicalPairCheck || !pick3CurrentValidationModel ||
        pick3PrimaryCaptureCard < 0 ||
        deferredPrimaryCards.card != pick3PrimaryCaptureCard ||
        deferredPrimaryCards.ids.empty() || !trickPlayPlayerIdField.IsValid()) return false;
    const auto first = ResolvePick3InitialEffectOnModel(
        pick3CurrentValidationModel, pick3PrimaryCaptureCard);
    if (!first.complete || first.effectId != pick3PrimaryCaptureEffect) {
        if (reason) *reason = "FIRST_EFFECT_UNAVAILABLE";
        return false;
    }
    // The built-in fast path is intentionally strict. Richer filters require
    // an effect-aware evaluator that can prove the exact (primary,lane) match.
    // We do not assume that a component-only lane test represents arbitrary
    // nested / owner / attribute / selector queries.
    const auto* filterBytes = reinterpret_cast<const std::uint8_t*>(first.secondary);
    const bool plainFilter = first.secondary &&
        *reinterpret_cast<const int*>(filterBytes + 0x2C) == 0 &&
        *reinterpret_cast<Obj* const*>(filterBytes + 0x30) == nullptr &&
        !*reinterpret_cast<const bool*>(filterBytes + 0x38);
    const bool plainLaneQuery = plainFilter && Pick3SecondaryIsLaneOnly(first.secondaryQuery);
    if (!plainLaneQuery && !pick3SecondaryFilterEvaluator) {
        if (reason) *reason = "SECONDARY_EVALUATOR_REQUIRED";
        return false;
    }
    auto* entities = trickEntitiesField[pick3CurrentValidationModel].Get();
    if (!entities || entities->size < 0 || entities->size > 20000 ||
        (entities->size && !entities->items)) {
        if (reason) *reason = "MODEL_ENTITIES_UNAVAILABLE";
        return false;
    }
    const int player = trickPlayPlayerIdField[play].Get();
    for (int primary : deferredPrimaryCards.ids) {
        for (int i = 0; i < entities->size; ++i) {
            Obj* lane = entities->items->m_Items[i];
            if (!lane) continue;
            auto componentsField = BNM::Class(lane).GetField("Components")
                .cast<BNM::Structures::Mono::List<Obj*>*>();
            auto* components = componentsField.IsValid() ? componentsField[lane].Get() : nullptr;
            if (!components || components->size < 0 || components->size > 128 ||
                (components->size && !components->items)) continue;
            bool laneComponent = false;
            for (int j = 0; j < components->size; ++j) {
                Obj* component = components->items->m_Items[j];
                if (!component) continue;
                auto* type = BNM::Class(component).GetClass();
                if (type && type->name && !std::strcmp(type->name, "Lane")) {
                    laneComponent = true; break;
                }
            }
            if (!laneComponent) continue;
            const int laneId = trickEntityIdField[lane].Get();
            if (laneId < 0) continue;
            // The registered evaluator may support additional query shapes.
            // Unsupported is not Allow. The callback is synchronous and must
            // use the same validation model and effect identity.
            // Test physical capacity (including Team-Up) first, but never
            // mistake it for acceptance by a complex stock lane query.
            if (pick3TransferPairCheck ? !pick3TransferPairCheck(pick3CurrentValidationModel,first.effectId,primary,laneId,player) : !pick3PhysicalPairCheck(pick3CurrentValidationModel,primary,laneId,player)) continue;
            const auto decision = plainLaneQuery
                ? Pick3SecondaryDecision::Allow
                : pick3SecondaryFilterEvaluator(
                    pick3CurrentValidationModel, first.effectId, first.secondary,
                    first.secondaryQuery, primary, laneId, player);
            if (decision == Pick3SecondaryDecision::Unsupported) {
                static thread_local int lastUnsupportedEffect=-1;
                if (lastUnsupportedEffect!=first.effectId) {
                    lastUnsupportedEffect=first.effectId;
                    CC_SELECTION_DIAG("CustomComponents/Pick3/PICK3_COMPLEX_LANE_QUERY: effect=%d result=UNSUPPORTED afterPhysical=1 stockQueryRequired=1",first.effectId);
                }
            }
            if (decision == Pick3SecondaryDecision::Allow) {
                if (foundCard) *foundCard = primary;
                if (foundLane) *foundLane = laneId;
                if (reason) *reason = "LEGAL_PAIR";
                return true;
            }
        }
    }
    if (reason) *reason = "NO_FILTERED_PHYSICAL_PAIR";
    return false;
}

inline int TrickOrderTrace(Obj* self, Obj* card, int player, int target, const void* method) {
    const int result = originalTrickOrder ? originalTrickOrder(self, card, player, target, method) : 0;
    if (trickValidationDepth && TrickFocus(trickValidationGuid) && trickTraceGenerationCount <= 160) {
        ++trickOrderCalls;
        CC_SELECTION_DIAG("CustomComponents/Pick3/TRICK_VALIDATION: phase=ORDERS guid=%d card=%d player=%d target=%d targetGuid=%d orders=%d",
                 trickValidationGuid, trickValidationCard, player, target, TrickGenerationGuid(target), result);
    }
    return result;
}
inline thread_local Obj* receiveReviveRecognitionModel = nullptr;
inline Obj* ReviveRecognitionModel();
inline bool TrickValidateObserved(const char* type, TrickValidateFn original, Obj* self, Obj* play, Obj* errors, const void* method) {
    // 1.7.28: incoming Type-3 Process() does not necessarily nest a
    // DetermineIfPlayIsValid() call. Bind the *validator's own* simulation
    // EntityModel before IsGenericPick3ReviveBurst(), rather than trying to
    // reuse LocalSelectTargetService's Type-6-only Finder. The binding lives
    // solely for this synchronous validation invocation and always restores.
    struct ScopedType3ValidatorRecognition final {
        Obj* previous;
        Obj* model = nullptr;
        int card = -1;
        explicit ScopedType3ValidatorRecognition(const char* kind, Obj* validator, Obj* candidate)
            : previous(receiveReviveRecognitionModel) {
            if (!enabled || !kind || std::strcmp(kind, "OneTimeEffect") ||
                !validator || !candidate || trickServiceActivePlay != candidate ||
                !trickPlayTypeField.IsValid() || !trickPlayCardIdField.IsValid() ||
                trickPlayTypeField[candidate].Get() != 3 ||
                !pick3OneTimeValidatorModelField.IsValid() ||
                !boundEntityModelClass.IsValid()) return;
            card = trickPlayCardIdField[candidate].Get();
            Obj* found = pick3OneTimeValidatorModelField[validator].Get();
            if (found && BNM::IsA(found, boundEntityModelClass)) {
                model = found;
                receiveReviveRecognitionModel = found;
            }
            static thread_local unsigned samples = 0;
            if (++samples <= 96)
                CC_SELECTION_DIAG("CustomComponents/Pick3/RECEIVER1728: phase=VALIDATOR_MODEL_BIND card=%d service=%p play=%p validator=%p model=%p retained=%p accepted=%d",
                    card, trickServiceActivePlay, candidate, validator, found,
                    Target(boundEntityModelRoot), model ? 1 : 0);
        }
        ~ScopedType3ValidatorRecognition() { receiveReviveRecognitionModel = previous; }
    } receiveType3Model(type, self, play);
    const Obj* previousCandidate = trickCandidateActivePlay;
    const bool scopedCandidate = kDeferredPick3TrialEnabled && trickValidationDepth &&
        IsGenericPick3ReviveBurst(trickValidationCard) &&
        play && trickPlayCardIdField.IsValid() &&
        trickPlayCardIdField[play].Get() == trickValidationCard;
    if (scopedCandidate) trickCandidateActivePlay = play;
    Obj* previousModel = pick3CurrentValidationModel;
    const int previousModelCard = pick3CurrentValidationCard;
    // PlayCardService validates a fresh Play (often type 3), not the generated
    // candidate Play. Re-establish the SAME model-bound primary capture inside
    // its own synchronous OneTimeEffectValidator call; never reuse the hand/UI
    // snapshot across transactions or clients.
    const bool scopedService = kDeferredPick3TrialEnabled && self && play &&
        trickServiceActivePlay == play &&
        trickPlayCardIdField.IsValid() && trickPlayTypeField.IsValid() &&
        trickPlayTypeField[play].Get() == 3 &&
        IsGenericPick3ReviveBurst(trickPlayCardIdField[play].Get());
    const bool oneTimeScope = (scopedCandidate || scopedService) && self && type &&
        !std::strcmp(type, "OneTimeEffect") &&
        pick3OneTimeValidatorModelField.IsValid();
    static thread_local unsigned serviceScopeSamples = 0;
    if (oneTimeScope) {
        const int scopedCard = scopedService ? trickPlayCardIdField[play].Get() : trickValidationCard;
        pick3CurrentValidationModel = pick3OneTimeValidatorModelField[self].Get();
        pick3CurrentValidationCard = scopedCard;
        Engine::PlayValidation::Prime(self);
        const auto firstEffect = ResolvePick3InitialEffectOnModel(
            pick3CurrentValidationModel, scopedCard);
        // Establish exact effect ownership before the stock target utility
        // executes. The scope is restored after stock validation returns.
        pick3PrimaryCaptureModel = pick3CurrentValidationModel;
        pick3PrimaryCaptureCard = scopedCard;
        pick3PrimaryCaptureEffect = firstEffect.complete ? firstEffect.effectId : -1;
        deferredPrimaryCards = {};
        pick3SynchronousVerifiedCastTarget = -1;
        if (scopedService && ++serviceScopeSamples <= 96)
            CC_SELECTION_DIAG("CustomComponents/Pick3/RECEIVER1727: phase=CAPTURE_ENTER card=%d play=%p service=%p determine=%p model=%p effect=%d complete=%d type=3 samePlay=1", scopedCard,
                     play,trickServiceActivePlay,trickDetermineActivePlay,
                     pick3CurrentValidationModel, firstEffect.effectId, firstEffect.complete ? 1 : 0);
        static thread_local unsigned effectSamples = 0;
        if (++effectSamples <= 96) {
            CC_SELECTION_DIAG("CustomComponents/Pick3/PICK3_INITIAL_EFFECT: card=%d guid=%d model=%p firstEffect=%d primary=%p primaryQuery=%p secondary=%p secondaryQuery=%p complete=%d pairVerified=0 decision=stock-unchanged",
                trickValidationCard, trickValidationGuid, pick3CurrentValidationModel,
                firstEffect.effectId, firstEffect.primary, firstEffect.primaryQuery,
                firstEffect.secondary, firstEffect.secondaryQuery,
                firstEffect.complete ? 1 : 0);
        }
        static thread_local unsigned modelSamples = 0;
        if (++modelSamples <= 96)
            CC_SELECTION_DIAG("CustomComponents/Pick3/PICK3_INITIAL_MODEL: phase=VALIDATOR_ENTER card=%d guid=%d validator=%p model=%p modelVerified=%d queryContextReady=0 pairVerified=0 override=0",
                     trickValidationCard, trickValidationGuid, self,
                     pick3CurrentValidationModel, pick3CurrentValidationModel ? 1 : 0);
    }
    if(!oneTimeScope && type && !std::strcmp(type,"OneTimeEffect"))Engine::PlayValidation::Prime(self);
    const bool result = original ? original(self, play, errors, method) : false;
    if (oneTimeScope && scopedCandidate && result &&
        pick3CurrentValidationModel && pick3PrimaryCaptureEffect>=0 &&
        deferredPrimaryCards.card==pick3PrimaryCaptureCard &&
        deferredPrimaryCards.ids.size()==1) {
        const int sole=deferredPrimaryCards.ids.front();
        auto& proof=stockSoleInitialPrimary;
        if (proof.card==-1) {
            proof={pick3PrimaryCaptureCard,pick3PrimaryCaptureEffect,sole,
                   pick3CurrentValidationModel,false};
        } else if (proof.card!=pick3PrimaryCaptureCard ||
                   proof.effect!=pick3PrimaryCaptureEffect ||
                   proof.target!=sole || proof.model!=pick3CurrentValidationModel) {
            proof.ambiguous=true;
        }
        PF3_REPAIR_TRACE("CustomComponents/Pick3/REPAIR1716: SOLE_STOCK_PROOF card=%d effect=%d target=%d model=%p ambiguous=%d proofSource=validator",
            pick3PrimaryCaptureCard,pick3PrimaryCaptureEffect,sole,
            (void*)pick3CurrentValidationModel,int(proof.ambiguous));
    }
    if (oneTimeScope && scopedService && result &&
        pick3CurrentValidationModel && pick3PrimaryCaptureEffect >= 0 &&
        deferredPrimaryCards.card == pick3PrimaryCaptureCard &&
        !deferredPrimaryCards.ids.empty() && deferredPrimaryCards.ids.size() <= 128) {
        pick3AcceptedCastPrimaries.model = pick3CurrentValidationModel;
        pick3AcceptedCastPrimaries.card = pick3PrimaryCaptureCard;
        pick3AcceptedCastPrimaries.effect = pick3PrimaryCaptureEffect;
        pick3AcceptedCastPrimaries.ids = deferredPrimaryCards.ids;
        pick3AcceptedCastPrimaries.captured = std::chrono::steady_clock::now();
        if (pick3SynchronousVerifiedCastTarget >= 0) {
            pick3VerifiedInitialCast = {pick3CurrentValidationModel,
                pick3PrimaryCaptureCard,pick3PrimaryCaptureEffect,
                pick3SynchronousVerifiedCastTarget,std::chrono::steady_clock::now()};
            PF3_REPAIR_TRACE("CustomComponents/Pick3/REPAIR1710: CAST_COMMITTED card=%d effect=%d cast=%d model=%p stage=stockValidatorAccepted",
                pick3PrimaryCaptureCard,pick3PrimaryCaptureEffect,
                pick3SynchronousVerifiedCastTarget,(void*)pick3CurrentValidationModel);
        }
        CC_SELECTION_DIAG("CustomComponents/Pick3/PICK3_REVIVE_OFFER_BRIDGE: phase=STORE card=%d effect=%d ids=%zu first=%d model=%p validated=1",
            pick3AcceptedCastPrimaries.card, pick3AcceptedCastPrimaries.effect,
            pick3AcceptedCastPrimaries.ids.size(), pick3AcceptedCastPrimaries.ids[0],
            pick3AcceptedCastPrimaries.model);
    }
    if (oneTimeScope) {
        // Never let IDs or validation-model pointers leak into a later Play,
        // model, or opponent's selection transaction.
        if (scopedService && serviceScopeSamples <= 96)
            CC_SELECTION_DIAG("CustomComponents/Pick3/PICK3_SERVICE_PAIR: phase=CAPTURE_EXIT card=%d result=%d", pick3PrimaryCaptureCard, result ? 1 : 0);
        deferredPrimaryCards = {};
        pick3SynchronousVerifiedCastTarget = -1;
        pick3PrimaryCaptureModel = nullptr;
        pick3PrimaryCaptureCard = -1;
        pick3PrimaryCaptureEffect = -1;
        pick3CurrentValidationModel = previousModel;
        pick3CurrentValidationCard = previousModelCard;
    }
    if (scopedCandidate) trickCandidateActivePlay = const_cast<Obj*>(previousCandidate);
    Pick3RecordStage1Validator(type, result);
    // Correlate the initial PickFromThree Play with the particular stock validator
    // rejection. No managed collection inspection or return-value override.
    if (scopedCandidate && !result && type &&
        (!std::strcmp(type, "OneTimeEffect") || !std::strcmp(type, "Composite"))) {
        static unsigned pickFromThreeGateRejectSamples = 0;
        if (++pickFromThreeGateRejectSamples <= 240) {
            const int zone = FindDeferredInitialZone(trickValidationCard);
            // Read only the concrete Play.Targets list already verified by the
            // game's Play layout. Never invoke methods on the pooled validator
            // collections (the earlier native crash came from that path).
            int targetCount = -1, firstTarget = -1;
            if (trickPlayTargetsField.IsValid()) {
                auto* ids = trickPlayTargetsField[play].Get();
                if (ids && ids->size >= 0 && ids->size <= 32 &&
                    (ids->size == 0 || ids->items)) {
                    targetCount = ids->size;
                    if (targetCount > 0) firstTarget = ids->items->m_Items[0];
                }
            }
            CC_SELECTION_DIAG("CustomComponents/Pick3/PICK_FROM_THREE_STAGE1_GATE: guid=%d card=%d playerScope=generated zone=%d validator=%s play=%p validatorObject=%p targetCount=%d firstTarget=%d targetMatchesZone=%d rejected=1 mutation=0",
                     trickValidationGuid, trickValidationCard, zone, type, play, self,
                     targetCount, firstTarget, (zone >= 0 && firstTarget == zone) ? 1 : 0);
        }
    }
    if (trickDetermineActivePlay && play==trickDetermineActivePlay) {
        CC_SELECTION_DIAG("CustomComponents/Pick3/TRICK_SERVICE: phase=VALIDATOR play=%p guid=%d type=%s validator=%p errors=%p accepted=%d",play,TrickServicePlayGuid(play),type,self,errors,result?1:0);
    }
    if (trickValidationDepth && TrickFocus(trickValidationGuid) && trickTraceGenerationCount <= 160) {
        ++trickValidatorCalls;
        CC_SELECTION_DIAG("CustomComponents/Pick3/TRICK_VALIDATION: phase=VALIDATE guid=%d card=%d type=%s validator=%p play=%p errors=%p accepted=%d",
                 trickValidationGuid, trickValidationCard, type, self, play, errors, result ? 1 : 0);
    }
    return result;
}
// Read-only OneTimeEffectValidator.IsTargetingValid diagnostic.
// The method accepts interface collections; inspect them only through managed
// get_Count/get_Item or GetEnumerator/MoveNext/get_Current, never list offsets.
using TrickIsTargetingValidFn = bool(*)(Obj*, Obj*, Obj*, Obj*, const void*);
inline TrickIsTargetingValidFn originalTrickIsTargetingValid = nullptr;
inline unsigned trickTargetValidSamples = 0;
inline int TrickCollectionCount(Obj* value) {
    if (!value) return -1;
    auto count = BNM::Class(value).GetMethod("get_Count",0).cast<int>();
    if (!count.IsValid()) return -2;
    int n=-2;
    auto exception=BNM::TryInvoke([&]{ n=count[value](); });
    return exception.IsValid() || n<0 || n>4096 ? -2 : n;
}
inline void TrickCollectionInspect(const char* label, Obj* value) {
    if (!value) {
        CC_SELECTION_DIAG("CustomComponents/Pick3/TRICK_TARGET_VALID: list=%s null=1",label);
        return;
    }
    auto klass=BNM::Class(value);
    auto* raw=klass.GetClass();
    const int n=TrickCollectionCount(value);
    int ids[12]{};
    int shown=0;
    bool enumerated=false;
    // Fast path for actual indexable collections (does not assume a memory layout).
    auto item=klass.GetMethod("get_Item",1).cast<int>();
    if (n>=0 && item.IsValid()) {
        enumerated=true;
        for(int i=0;i<n && i<12;i++) {
            int x=0;
            auto ex=BNM::TryInvoke([&]{ x=item[value](i); });
            if(ex.IsValid()) { enumerated=false;break; }
            ids[shown++]=x;
        }
    } else {
        // Generic IEnumerable<int> path; MoveNext and get_Current are invoked
        // on the actual enumerator instance, with an upper bound.
        auto getEnum=klass.GetMethod("GetEnumerator",0).cast<Obj*>();
        if(getEnum.IsValid()) {
            Obj* en=nullptr;
            auto ex=BNM::TryInvoke([&]{en=getEnum[value]();});
            if(!ex.IsValid() && en) {
                auto ek=BNM::Class(en);
                auto move=ek.GetMethod("MoveNext",0).cast<bool>();
                auto current=ek.GetMethod("get_Current",0).cast<int>();
                if(move.IsValid() && current.IsValid()) {
                    enumerated=true;
                    for(int i=0;i<12;i++) {
                        bool more=false;
                        auto mEx=BNM::TryInvoke([&]{more=move[en]();});
                        if(mEx.IsValid()) { enumerated=false;break; }
                        if(!more) break;
                        int x=0;
                        auto cEx=BNM::TryInvoke([&]{x=current[en]();});
                        if(cEx.IsValid()) {enumerated=false;break;}
                        ids[shown++]=x;
                    }
                }
                // Some enumerators implement IDisposable; do not retain them.
                auto dispose=ek.GetMethod("Dispose",0).cast<void>();
                if(dispose.IsValid()) BNM::TryInvoke([&]{dispose[en]();});
            }
        }
    }
    CC_SELECTION_DIAG("CustomComponents/Pick3/TRICK_TARGET_VALID: list=%s class=%s.%s count=%d readable=%d shown=%d ids=[%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d] (first shown entries only)",
        label,raw&&raw->namespaze?raw->namespaze:"?",raw&&raw->name?raw->name:"?",
        n,enumerated?1:0,shown,ids[0],ids[1],ids[2],ids[3],ids[4],ids[5],ids[6],ids[7],ids[8],ids[9],ids[10],ids[11]);
}
// Patch 1.7.18: passive, synchronous snapshots of the *actual arguments*
// passed to OneTimeEffectValidator.IsTargetingValid. Only concrete List<int>
// and PoolableList<int> layouts are known. Interface/HashSet/enumerator
// values are deliberately reported opaque: reflective enumeration in this
// hot path has previously caused crashes during PvP validation.
inline bool TrickReadConcreteTargetArgument(Obj* value, int* count, int* first,
                                            int* second, const char** concrete) {
    *count = -1; *first = -1; *second = -1; *concrete = "opaque";
    if (!value) { *concrete = "null"; return false; }
    auto* klass = BNM::Class(value).GetClass();
    const char* name = klass ? klass->name : nullptr;
    if (!name || (std::strcmp(name, "List`1") != 0 &&
                  std::strcmp(name, "PoolableList`1") != 0 &&
                  std::strcmp(name, "List<int>") != 0 &&
                  std::strcmp(name, "PoolableList<int>") != 0))
        return false;
    const auto* ids = reinterpret_cast<const IntList*>(value);
    const int n = ids->size;
    if (n < 0 || n > 128 ||
        (n && (!ids->items || ids->items->capacity < static_cast<size_t>(n)))) {
        *concrete = "invalid-bounds";
        return false;
    }
    *count = n;
    if (n > 0) *first = ids->items->m_Items[0];
    if (n > 1) *second = ids->items->m_Items[1];
    *concrete = "concrete-list";
    return true;
}
inline bool TrickIsTargetingValidTrace(Obj* self, Obj* valid, Obj* selected, Obj* errors, const void* method) {
    // Service collection enumeration is diagnostic-only. During PvP the game's
    // validation can own pooled/interface enumerators with a shorter lifetime
    // than our reflected reads. Keep this path passive; stock validation and
    // the actual deferred primary-source capture below are unchanged.
    if(trickDetermineActivePlay && TrickFocus(TrickServicePlayGuid(trickDetermineActivePlay))) {
        CC_SELECTION_DIAG("CustomComponents/Pick3/TRICK_SERVICE: phase=TARGET_COMPARE play=%p guid=%d validator=%p collections=not-enumerated",trickDetermineActivePlay,TrickServicePlayGuid(trickDetermineActivePlay),self);
    }
    const bool focus=trickValidationDepth && TrickFocus(trickValidationGuid) && ++trickTargetValidSamples<=160;
    const int before=-2; // Diagnostic collection reads disabled in validation hot path.
    if(focus) CC_SELECTION_DIAG("CustomComponents/Pick3/TRICK_TARGET_VALID: phase=ENTER guid=%d card=%d validator=%p valid=%p selected=%p errors=%p enumeration=disabled",trickValidationGuid,trickValidationCard,self,valid,selected,errors);
    // One narrowly scoped experiment: the initial zone was *actually* produced
    // by the engine for this same card, and the submitted Play selects that exact ID.
    // Preserve stock validation for every other case, including all pick3 effect
    // selections and secondary lane selection after the initial play.
    static thread_local unsigned pvpTargetBoundarySeq = 0;
    const unsigned boundary = ++pvpTargetBoundarySeq;
    const bool receiverService = trickServiceActivePlay &&
        trickPlayCardIdField.IsValid() && trickPlayTypeField.IsValid() &&
        trickPlayTypeField[trickServiceActivePlay].Get() == 3;
    const bool nestedDetermine = receiverService &&
        trickDetermineActivePlay == trickServiceActivePlay;
    CC_SELECTION_DIAG("CustomComponents/Pick3/RECEIVER1727: phase=SERVICE_SCOPE_ENTER seq=%u service=%p determine=%p nested=%d type3=%d valid=%p selected=%p", boundary,
        trickServiceActivePlay,trickDetermineActivePlay,int(nestedDetermine),int(receiverService),valid,selected);
    int serviceCard = -1;
    bool serviceScope = false;
    if (receiverService) {
        serviceCard = trickPlayCardIdField[trickServiceActivePlay].Get();
        CC_SELECTION_DIAG("CustomComponents/Pick3/PICK_FROM_THREE_PVP_VALID: seq=%u phase=PREDICATE_ENTER card=%d",boundary,serviceCard);
        serviceScope = IsGenericPick3ReviveBurst(serviceCard);
        CC_SELECTION_DIAG("CustomComponents/Pick3/PICK_FROM_THREE_PVP_VALID: seq=%u phase=PREDICATE_EXIT card=%d matched=%d",boundary,serviceCard,serviceScope?1:0);
        if (!serviceScope) {
            static thread_local unsigned failureSamples = 0;
            if (++failureSamples <= 48) {
                Obj* recognitionModel = ReviveRecognitionModel();
                auto* entities = recognitionModel && trickEntitiesField.IsValid()
                    ? trickEntitiesField[recognitionModel].Get() : nullptr;
                int found = 0;
                if (entities && trickEntityIdField.IsValid() &&
                    entities->size >= 0 && entities->size <= 20000 &&
                    (!entities->size || entities->items)) {
                    for (int i=0; i<entities->size; ++i) {
                        Obj* e = entities->items->m_Items[i];
                        if (e && trickEntityIdField[e].Get() == serviceCard) { found = 1; break; }
                    }
                }
                CC_SELECTION_DIAG("CustomComponents/Pick3/RECEIVER1728: phase=RECOGNITION_MISS card=%d model=%p retained=%p modelBound=%d entities=%p entityCount=%d found=%d",
                    serviceCard, recognitionModel, Target(boundEntityModelRoot),
                    receiveReviveRecognitionModel ? 1 : 0, entities,
                    entities ? entities->size : -1, found);
            }
        }
    }
    // No BNM::TryInvoke(get_Item) or get_Count on service-owned pooled data.
    // Previously this occurred in the opponent's nested validation callback.
    // Keep the last validated snapshot only if obtained through a proven-safe
    // path elsewhere; do not populate one from an unsafe interface collection.
    // The enclosing service OneTimeEffectValidator captured stock primary IDs
    // from its concrete PoolableList synchronously. Do not clear them here:
    // this comparison is precisely where the pair check needs them. We still
    // never enumerate the incoming ICollection interface objects.
    CC_SELECTION_DIAG("CustomComponents/Pick3/PICK_FROM_THREE_PVP_VALID: seq=%u phase=SERVICE_SCOPE_EXIT matched=%d",boundary,serviceScope?1:0);
    const bool candidateScope = trickCandidateActivePlay && trickValidationDepth &&
        IsGenericPick3ReviveBurst(trickValidationCard) &&
        trickPlayCardIdField.IsValid() &&
        trickPlayCardIdField[trickCandidateActivePlay].Get() == trickValidationCard;
    Obj* activePlay = serviceScope ? trickServiceActivePlay :
        (candidateScope ? trickCandidateActivePlay : nullptr);
    if (kDeferredPick3TrialEnabled && activePlay &&
        trickPlayCardIdField.IsValid() && trickPlayTargetsField.IsValid()) {
        const int card = trickPlayCardIdField[activePlay].Get();
        auto* targets = trickPlayTargetsField[activePlay].Get();
        const int zone = FindDeferredInitialZone(card);
        // PvP safety: never reflectively invoke get_Count on the stock
        // validator's pooled/interface collections. The old "count > 0"
        // shortcut could crash inside OneTimeEffectValidator.Validate and
        // bypassed stock targeting checks for normalized zone Plays.
        // Preserve the existing play-generation bridge, but let stock decide
        // validity here until a proven-safe legality representation exists.
        const char* scope = serviceScope ? "service" : "candidate";
        CC_SELECTION_DIAG("CustomComponents/Pick3/DEFERRED_TRIAL: phase=STOCK_COMPARE_SAFE scope=%s guid=%d card=%d zone=%d play=%p noManagedCount=1 noValidationBypass=1",
            scope,TrickServicePlayGuid(activePlay),card,zone,activePlay);
    }
    // Validate *before* stock writes an initial-targeting error: the first
    // effect selects discarded cards, but the initial Play selects its zone.
    // Do not apply this to subsequent selection-service validation.
    const bool pairScope = (candidateScope || serviceScope) && activePlay &&
        pick3PrimaryCaptureCard >= 0 &&
        pick3PrimaryCaptureCard == (serviceScope ? serviceCard : trickValidationCard) &&
        pick3CurrentValidationCard == pick3PrimaryCaptureCard &&
        pick3PrimaryCaptureModel &&
        pick3PrimaryCaptureModel == pick3CurrentValidationModel &&
        pick3PrimaryCaptureEffect >= 0 && trickPlayTargetsField.IsValid();
    // Diagnostic-only: log exactly the arguments received by this stock
    // comparison, not the earlier PF3 candidate list. Never alter either
    // collection, errors or the return value. Values of unsupported concrete
    // types remain opaque instead of risking managed iterator lifetime bugs.
    if (serviceScope || candidateScope) {
        static thread_local unsigned initialCompareSamples = 0;
        if (++initialCompareSamples <= 128) {
            int validCount=-1,validFirst=-1,validSecond=-1;
            int selectedCount=-1,selectedFirst=-1,selectedSecond=-1;
            const char *validKind=nullptr,*selectedKind=nullptr;
            const bool validRead=TrickReadConcreteTargetArgument(valid,&validCount,
                &validFirst,&validSecond,&validKind);
            const bool selectedRead=TrickReadConcreteTargetArgument(selected,&selectedCount,
                &selectedFirst,&selectedSecond,&selectedKind);
            const int activeCard = serviceScope ? serviceCard : trickValidationCard;
            const int activeType = activePlay && trickPlayTypeField.IsValid()
                ? trickPlayTypeField[activePlay].Get() : -1;
            CC_SELECTION_DIAG("CustomComponents/Pick3/REPAIR1718: INITIAL_PLAY_VALIDATION scope=%s card=%d guid=%d effect=%d model=%p play=%p type=%d validObj=%p validKind=%s validReadable=%d validCount=%d validFirst=%d validSecond=%d selectedObj=%p selectedKind=%s selectedReadable=%d selectedCount=%d selectedFirst=%d selectedSecond=%d pairScope=%d mutation=0",
                serviceScope?"service":"candidate",activeCard,TrickGenerationGuid(activeCard),
                pick3PrimaryCaptureEffect,(void*)pick3CurrentValidationModel,activePlay,activeType,
                valid,validKind,int(validRead),validCount,validFirst,validSecond,
                selected,selectedKind,int(selectedRead),selectedCount,selectedFirst,
                selectedSecond,int(pairScope));
        }
    }
    if (pairScope) {
        auto* playTargets = trickPlayTargetsField[activePlay].Get();
        const int generatedZone = FindDeferredInitialZone(pick3PrimaryCaptureCard);
        // The remote peer has no UI drag-cache. Read the exact player's hand
        // association on the *validator's* simulation model rather than
        // guessing a globally fixed casting zone (old code used literal 10).
        // Both local and remote casting Plays must match this hand entity.
        const int playOwner=trickPlayPlayerIdField.IsValid() ?
            trickPlayPlayerIdField[activePlay].Get() : -1;
        const int modelHand=Pick3CastingHandOnModel(pick3CurrentValidationModel,playOwner);
        const bool remoteMissingGenerated = serviceScope && !candidateScope &&
            generatedZone < 0 && trickPlayTypeField.IsValid() &&
            trickPlayTypeField[activePlay].Get() == 3;
        const bool remoteZoneFallback = remoteMissingGenerated && modelHand >= 0;
        // A generated one-target burst is our exact casting target on the
        // candidate path. It is NOT necessarily HandAssociation.HandEntityId
        // (observed 159 vs hand 3 in the 11:16 PvP test). The remote hand
        // fallback is kept only when it *itself* matches the incoming Play.
        // Do not accept a received non-hand target without equivalent proof.
        const int incomingZone = playTargets && playTargets->size == 1 &&
            playTargets->items ? playTargets->items->m_Items[0] : -1;
        const bool fromStockBurst = generatedZone >= 0 && incomingZone == generatedZone;
        const bool fromVerifiedRemoteHand = remoteZoneFallback && incomingZone == modelHand;
        // 1.7.15: The receiving peer does not execute the sender's drag and
        // its generated-zone cache is sometimes empty. In the two-client
        // 2026-11-02 17:53:59 trace, stock Play.Type=3 targeted entity 10;
        // the receiver had generated=-1 and verified the discarded-card/lane
        // pair successfully. That casting entity is NOT necessarily HandId.
        // This exact compatibility case is deliberately not a replacement
        // for arbitrary remote target verification: only the initial
        // service-side Play, a known legacy casting entity, and the *same*
        // owning validator model/effect may use it. The physical card/lane
        // preflight below still must pass before any local acceptance.
        constexpr int kObservedLegacyInitialCastEntity = 10;
        const bool fromProvenLegacyReceiver =
            remoteMissingGenerated && incomingZone == kObservedLegacyInitialCastEntity &&
            playOwner >= 0 && pick3CurrentValidationModel &&
            pick3PrimaryCaptureModel == pick3CurrentValidationModel &&
            pick3PrimaryCaptureCard == serviceCard &&
            pick3PrimaryCaptureEffect >= 0;
        const int initialZone = fromStockBurst ? generatedZone :
            (fromVerifiedRemoteHand ? modelHand :
             (fromProvenLegacyReceiver ? incomingZone : -1));
        const bool exactZone = initialZone >= 0 &&
            (fromStockBurst || fromVerifiedRemoteHand || fromProvenLegacyReceiver);
        PF3_REPAIR_TRACE("CustomComponents/Pick3/REPAIR1715: RECEIVER_CAST_SCOPE card=%d effect=%d player=%d generated=%d incoming=%d hand=%d model=%p fromBurst=%d fromHand=%d fromLegacyReceiver=%d exact=%d needsPair=1",
            pick3PrimaryCaptureCard,pick3PrimaryCaptureEffect,playOwner,generatedZone,incomingZone,modelHand,
            (void*)pick3CurrentValidationModel,int(fromStockBurst),int(fromVerifiedRemoteHand),
            int(fromProvenLegacyReceiver),int(exactZone));
        PF3_REPAIR_TRACE("CustomComponents/Pick3/REPAIR1710: CAST_SOURCE card=%d effect=%d owner=%d generated=%d incoming=%d hand=%d burstMatch=%d remoteHandMatch=%d accepted=%d scope=%s",
            pick3PrimaryCaptureCard,pick3PrimaryCaptureEffect,playOwner,generatedZone,incomingZone,modelHand,
            int(fromStockBurst),int(fromVerifiedRemoteHand),int(exactZone),serviceScope?"service":"candidate");
        if(serviceScope && !exactZone && incomingZone>=0)
            PF3_REPAIR_TRACE("CustomComponents/Pick3/REPAIR1710: REMOTE_CAST_UNPROVEN card=%d effect=%d incoming=%d generated=%d hand=%d preservedStock=1",
                pick3PrimaryCaptureCard,pick3PrimaryCaptureEffect,incomingZone,generatedZone,modelHand);
        PF3_REPAIR_TRACE("CustomComponents/Pick3/REPAIR178: CAST_PREFLIGHT card=%d owner=%d model=%p hand=%d generated=%d incoming=%d fallback=%d exact=%d scope=%s",
            pick3PrimaryCaptureCard,playOwner,(void*)pick3CurrentValidationModel,
            modelHand,generatedZone,playTargets&&playTargets->size==1&&playTargets->items?playTargets->items->m_Items[0]:-1,
            int(remoteZoneFallback),int(exactZone),serviceScope?"service":"candidate");
        if (serviceScope) {
            static thread_local unsigned zoneSamples = 0;
            if (++zoneSamples <= 120)
                CC_SELECTION_DIAG("CustomComponents/Pick3/PICK3_SERVICE_ZONE: card=%d generated=%d resolved=%d fallback=%d submitted=%d exact=%d model=%p",
                         pick3PrimaryCaptureCard, generatedZone, initialZone,
                         (remoteZoneFallback || fromProvenLegacyReceiver) ? 1 : 0,
                         playTargets && playTargets->size == 1 && playTargets->items
                            ? playTargets->items->m_Items[0] : -1,
                         exactZone ? 1 : 0, pick3CurrentValidationModel);
        }
        int primary = -1, lane = -1;
        const char* why = "NOT_INITIAL_ZONE";
        const bool pair = exactZone && Pick3VerifyCapturedRevivePair(
            activePlay, &primary, &lane, &why);
        static thread_local unsigned pairLogSamples = 0;
        if (++pairLogSamples <= 120)
            CC_SELECTION_DIAG("CustomComponents/Pick3/PICK3_STAGE1_PAIR: card=%d guid=%d zone=%d initial=%d primary=%d lane=%d verified=%d reason=%s",
                     pick3PrimaryCaptureCard, TrickGenerationGuid(pick3PrimaryCaptureCard), initialZone,
                     exactZone ? 1 : 0, primary, lane, pair ? 1 : 0, why);
        if (serviceScope && pairLogSamples <= 120)
            CC_SELECTION_DIAG("CustomComponents/Pick3/PICK3_SERVICE_PAIR: phase=VALIDATE card=%d zone=%d exact=%d primary=%d lane=%d accepted=%d reason=%s",
                pick3PrimaryCaptureCard, initialZone, exactZone ? 1 : 0, primary, lane, pair ? 1 : 0, why);
        if (pair) {
            // The enclosing original validator still decides whether the
            // casting Play is accepted; this record is only staged here.
            pick3SynchronousVerifiedCastTarget=initialZone;
            PF3_REPAIR_TRACE("CustomComponents/Pick3/REPAIR1710: PAIR_APPROVED card=%d effect=%d cast=%d discardedCard=%d lane=%d stage=preStockValidation",
                pick3PrimaryCaptureCard,pick3PrimaryCaptureEffect,initialZone,primary,lane);
            if (serviceScope)
                PF3_REPAIR_TRACE("CustomComponents/Pick3/REPAIR1715: RECEIVER_PAIR_COMMITTED card=%d effect=%d cast=%d selectedCandidate=%d legalLane=%d legacy=%d physicalPair=1 stockServerAck=unknown",
                    pick3PrimaryCaptureCard,pick3PrimaryCaptureEffect,initialZone,primary,lane,
                    int(fromProvenLegacyReceiver));
            // This is a LOCAL CUSTOM validator success, not an invocation of
            // OneTimeEffectValidator.IsTargetingValid and not a server ACK.
            // Preserve the tested 1.7.13 behavior until a stock-compatible
            // authoritative target contract has been demonstrated.
            if (serviceScope && trickServiceActivePlay==activePlay) {
                auth1714LocalPairAccepted=true;
                auth1714LocalPairCard=pick3PrimaryCaptureCard;
                auth1714LocalPairTarget=initialZone;
            }
            PF3_REPAIR_TRACE("CustomComponents/Pick3/REPAIR1714: TARGET_AUTHORITY_CONTRACT guid=%d card=%d effect=%d player=%d scope=%s playType=%d cast=%d primary=%d lane=%d fromBurst=%d remoteHand=%d localBypass=1 stockValidatorCalled=0 authoritativeAccepted=0",
                TrickGenerationGuid(pick3PrimaryCaptureCard),pick3PrimaryCaptureCard,pick3PrimaryCaptureEffect,playOwner,
                serviceScope?"service":"candidate",trickPlayTypeField.IsValid()?trickPlayTypeField[activePlay].Get():-1,
                initialZone,primary,lane,int(fromStockBurst),int(fromVerifiedRemoteHand));
            return true;
        }
        PF3_REPAIR_TRACE("CustomComponents/Pick3/REPAIR1710: PAIR_REJECT card=%d effect=%d cast=%d reason=%s preflight=%d stockFallback=1",
            pick3PrimaryCaptureCard,pick3PrimaryCaptureEffect,initialZone,why,int(exactZone));
    }
    const bool result=originalTrickIsTargetingValid ? originalTrickIsTargetingValid(self,valid,selected,errors,method):false;
    if (serviceScope || candidateScope)
        PF3_REPAIR_TRACE("CustomComponents/Pick3/REPAIR1714: STOCK_TARGET_DECISION guid=%d card=%d scope=%s accepted=%d localBypass=0",
            TrickGenerationGuid(serviceScope?serviceCard:trickValidationCard),serviceScope?serviceCard:trickValidationCard,
            serviceScope?"service":"candidate",int(result));
    if (trickValidationDepth && IsGenericPick3ReviveBurst(trickValidationCard)) {
        ++pick3Stage1Matrix.targetingCalls;
        if (!result) ++pick3Stage1Matrix.targetingRejects;
    }

    if(focus) {
        const int after=-2;
        CC_SELECTION_DIAG("CustomComponents/Pick3/TRICK_TARGET_VALID: phase=EXIT guid=%d card=%d result=%d errorCountBefore=%d errorCountAfter=%d added=%d errors=opaque",
            trickValidationGuid,trickValidationCard,result?1:0,before,after,(before>=0&&after>=0)?after-before:-1);
    }
    return result;
}
inline bool TrickCompositeValidate(Obj* a, Obj* b, Obj* c, const void* m) { return TrickValidateObserved("Composite", originalTrickCompositeValidate, a,b,c,m); }
inline bool TrickOneTimeValidate(Obj* a, Obj* b, Obj* c, const void* m) {
    auto field=pick3OneTimeValidatorModelField;
    Engine::PlayValidation::Scope scope(a && field.IsValid()?field[a].Get():nullptr,b);
    return Core::Validation::AfterNative(TrickValidateObserved("OneTimeEffect", originalTrickOneTimeValidate, a,b,c,m),[]{return Engine::PlayValidation::Allows();});
}
inline bool TrickNullValidate(Obj* a, Obj* b, Obj* c, const void* m) { return TrickValidateObserved("Null", originalTrickNullValidate, a,b,c,m); }
inline bool TrickInHandValidate(Obj* a, Obj* b, Obj* c, const void* m) { return TrickValidateObserved("InHand", originalTrickInHandValidate, a,b,c,m); }
inline bool TrickFighterValidate(Obj* a, Obj* b, Obj* c, const void* m) {
    return TrickValidateObserved("Fighter", originalTrickFighterValidate, a,b,c,m);
}
inline bool TrickOpenLaneValidate(Obj* a, Obj* b, Obj* c, const void* m) {
    return TrickValidateObserved("OpenLane", originalTrickOpenLaneValidate, a,b,c,m);
}


// Patch 1.7.11: receive-side recognition must inspect the model owned by
// the LocalSelectTargetService actually processing the incoming PvP Play.
// This scope is synchronous and thread-local: never rebind the presentation
// EntityModel or retain the receiver model beyond Process().
}
