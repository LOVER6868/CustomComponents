#include "diagnostics/DiagnosticLogging.hpp"
#pragma once
// Private implementation module. Include PickFromThreeSubmission.hpp, not this file.
namespace CustomComponents::Pick3::Submission {
inline thread_local Obj* generationReviveRecognitionModel = nullptr;
inline BNM::Field<Obj*> gameStateUtilsEntityModelField;
inline BNM::Class trickCardComponentClass;
inline Obj* ReviveRecognitionModel() {
    if (receiveReviveRecognitionModel) return receiveReviveRecognitionModel;
    if (generationReviveRecognitionModel) return generationReviveRecognitionModel;
    if (reviveDragLookupActive && deferredValidatedPlay.card >= 0) {
        auto* proven = Target(reviveLegalPlayModelRoot);
        const auto age = std::chrono::steady_clock::now() - deferredValidatedPlay.provenAt;
        if (proven && age >= std::chrono::steady_clock::duration::zero() &&
            age <= std::chrono::seconds(8)) return proven;
    }
    return Target(boundEntityModelRoot);
}
struct ScopedGenerationReviveRecognition final {
    Obj* previous;
    Obj* model;
    explicit ScopedGenerationReviveRecognition(Obj* decider) :
        previous(generationReviveRecognitionModel), model(nullptr) {
        if (decider && gameStateUtilsEntityModelField.IsValid()) {
            auto* candidate=gameStateUtilsEntityModelField[decider].Get();
            if (candidate && boundEntityModelClass.IsValid() &&
                BNM::IsA(candidate,boundEntityModelClass)) model=candidate;
        }
        if (model) generationReviveRecognitionModel=model;
        PF3_REPAIR_TRACE("CustomComponents/Pick3/REPAIR1713: GENERATION_MODEL decider=%p deciderModel=%p retained=%p valid=%d",
            (void*)decider,(void*)model,(void*)Target(boundEntityModelRoot),model?1:0);
    }
    ~ScopedGenerationReviveRecognition() { generationReviveRecognitionModel=previous; }
};
struct ScopedReceiveReviveRecognition final {
    Obj* previous = nullptr;
    Obj* serviceModel = nullptr;
    explicit ScopedReceiveReviveRecognition(Obj* service, Obj* play) {
        previous = receiveReviveRecognitionModel;
        if (!enabled || !service || !play || !playType.IsValid() ||
            playType[play].Get() != 6 || !localClass.IsValid() ||
            !BNM::IsA(service, localClass) || !localServiceFinder.IsValid() ||
            !TargetBridge::finderEntityFinder.IsValid() ||
            !TargetBridge::entityFinderModel.IsValid()) return;
        Obj* finder = localServiceFinder[service].Get();
        Obj* entityFinder = finder ? TargetBridge::finderEntityFinder[finder].Get() : nullptr;
        Obj* model = entityFinder ? TargetBridge::entityFinderModel[entityFinder].Get() : nullptr;
        if (!model || !boundEntityModelClass.IsValid() ||
            !BNM::IsA(model, boundEntityModelClass)) {
            PF3_REPAIR_TRACE("CustomComponents/Pick3/REPAIR1711: RX_MODEL_SCOPE_REJECT service=%p serviceModel=%p retainedModel=%p reason=invalid-service-model",
                (void*)service, (void*)model, (void*)Target(boundEntityModelRoot));
            return;
        }
        serviceModel = model;
        receiveReviveRecognitionModel = model;
        PF3_REPAIR_TRACE("CustomComponents/Pick3/REPAIR1711: RX_MODEL_SCOPE_ENTER service=%p serviceModel=%p retainedModel=%p differ=%d",
            (void*)service, (void*)model, (void*)Target(boundEntityModelRoot),
            int(model != Target(boundEntityModelRoot)));
    }
    ~ScopedReceiveReviveRecognition() {
        receiveReviveRecognitionModel = previous;
        if (serviceModel)
            PF3_REPAIR_TRACE("CustomComponents/Pick3/REPAIR1711: RX_MODEL_SCOPE_EXIT serviceModel=%p restored=%p",
                (void*)serviceModel, (void*)previous);
    }
    ScopedReceiveReviveRecognition(const ScopedReceiveReviveRecognition&) = delete;
    ScopedReceiveReviveRecognition& operator=(const ScopedReceiveReviveRecognition&) = delete;
};

// Native-only GUID lookup for diagnostic and reconciliation call sites.
// Do not call managed Entity.GetComponent from card-play/validation hooks:
// re-entrant IL2CPP invocation here was present in the matching PvP crash stack.
// The entity's Components list already contains the Card component.
inline int TrickGenerationGuid(int id) {
    if (id < 0 || !trickEntitiesField.IsValid() || !trickEntityIdField.IsValid() ||
        !TargetBridge::databaseCardGuid.IsValid() || !trickEntityComponentsField.IsValid() ||
        !trickCardComponentClass.IsValid()) return -1;
    Obj* model = ReviveRecognitionModel();
    if (!model) return -1;
    auto* entities = trickEntitiesField[model].Get();
    if (!entities || entities->size < 0 || entities->size > 20000 ||
        (entities->size && !entities->items)) return -1;
    for (int i = 0; i < entities->size; ++i) {
        Obj* entity = entities->items->m_Items[i];
        if (!entity || trickEntityIdField[entity].Get() != id) continue;
        auto* components = trickEntityComponentsField[entity].Get();
        if (!components || components->size < 0 || components->size > 128 ||
            (components->size && !components->items)) return -1;
        for (int j = 0; j < components->size; ++j) {
            Obj* component = components->items->m_Items[j];
            if (!component) continue;
            if (BNM::IsA(component, trickCardComponentClass))
                return TargetBridge::databaseCardGuid[component].Get();
        }
        return -1;
    }
    return -1;
}
// Strict generic entry-point predicate. This runs against the rooted live
// entity model, not a GUID allowlist. Only the first PlayTrigger effect of
// a Burst card is accepted, and it MUST carry the PickFromThree primary
// filter and ReturnFromDiscardPileToLane descriptor. Later effects are not
// repaired just because they have the same descriptor.
inline bool ComputeGenericPick3ReviveBurst(int cardId) {
    struct Scope { int id; bool log; Scope(int c):id(c),log(PvpCrashTraceEnabled()){ if(log) {++pvpCrashTraceSerial; ++pvpCrashTraceDepth; PvpCrashMark("GENERIC_BURST","ENTER",id); } } ~Scope(){if(log){PvpCrashMark("GENERIC_BURST","EXIT",id);--pvpCrashTraceDepth;}} } traceScope(cardId);
    if(cardId<0 || !trickEntitiesField.IsValid() || !trickEntityIdField.IsValid() ||
       !reviveEffectOwnerSourceField.IsValid() || !TargetBridge::selectionType.IsValid())return false;
    Obj* model=ReviveRecognitionModel();
    auto* entities=model?trickEntitiesField[model].Get():nullptr;
    if(!entities || entities->size<0 || entities->size>20000 || (entities->size && !entities->items))return false;
    Obj* source=nullptr;
    for(int i=0;i<entities->size;++i){
        Obj* e=entities->items->m_Items[i];if(e && trickEntityIdField[e].Get()==cardId){source=e;break;}
    }
    if(!source)return false;
    auto srcField=trickEntityComponentsField;
    auto* src=srcField.IsValid()?srcField[source].Get():nullptr;
    bool burst=false;
    if(src && src->size>=0 && src->size<=128 && (!src->size || src->items)) {
        for(int j=0;j<src->size;++j){
            Obj* c=src->items->m_Items[j];if(!c)continue;
            auto* k=BNM::Class(c).GetClass();if(k && k->name && !std::strcmp(k->name,"Burst")){burst=true;break;}
        }
    }
    if(!burst)return false;
    int firstId=INT32_MAX;bool firstQualifies=false;
    for(int i=0;i<entities->size;++i){
        Obj* e=entities->items->m_Items[i];if(!e)continue;
        int id=trickEntityIdField[e].Get();if(id<0 || id>=firstId)continue;
        auto f=trickEntityComponentsField;
        auto* comps=f.IsValid()?f[e].Get():nullptr;
        if(!comps || comps->size<0 || comps->size>128 || (comps->size && !comps->items))continue;
        bool play=false,revive=false,secondary=false;Obj* owner=nullptr;Obj* primary=nullptr;
        for(int j=0;j<comps->size;++j){
            Obj* c=comps->items->m_Items[j];if(!c)continue;
            auto* k=BNM::Class(c).GetClass();const char* n=k?k->name:nullptr;if(!n)continue;
            if(!std::strcmp(n,"PlayTrigger"))play=true;
            else if(!std::strcmp(n,"EffectEntity"))owner=c;
            else if(!std::strcmp(n,"PrimaryTargetFilter"))primary=c;
            else if(!std::strcmp(n,"SecondaryTargetFilter"))secondary=true;
            else if((!std::strcmp(n,"ReturnFromDiscardPileToLaneEffectDescriptor") || !std::strcmp(n,"PlayFromDeckToLaneEffectDescriptor")))revive=true;
        }
        if(!play || !owner || reviveEffectOwnerSourceField[owner].Get()!=cardId)continue;
        firstId=id;
        firstQualifies=revive && primary && secondary &&
            EnumModes::IsPickFromThree(TargetBridge::selectionType[primary].Get());
    }
    return firstQualifies;
}
// Cheap source-identity validation plus a bounded cache prevents repeated
// full-model class/reflection scans during normal card play and PvP submissions.
// Entries expire after 32 lookups, and are invalidated on model/list changes.
// No managed pointers are retained beyond the live model/source identity check.
inline bool IsGenericPick3ReviveBurst(int cardId) {
    if (cardId < 0 || !trickEntitiesField.IsValid() || !trickEntityIdField.IsValid()) return false;
    Obj* model=ReviveRecognitionModel();
    auto* entities=model?trickEntitiesField[model].Get():nullptr;
    if (!entities || entities->size<0 || entities->size>20000 ||
        (entities->size && !entities->items)) return false;
    Obj* source=nullptr;
    for(int i=0;i<entities->size;++i) {
        Obj* e=entities->items->m_Items[i];
        if(e && trickEntityIdField[e].Get()==cardId) {source=e;break;}
    }
    if(!source)return false;
    struct Entry {
        Obj* model=nullptr;
        const void* list=nullptr;
        Obj* source=nullptr;
        int count=-1, id=-1;
        unsigned ticket=0;
        std::uint64_t generation=0;
        bool result=false;
    };
    static thread_local Entry cache[64]{};
    static thread_local unsigned ticket=0;
    static thread_local bool evaluating=false;
    if(evaluating)return false; // fail closed rather than recursively inspect a mutating model
    ++ticket;
    Entry& slot=cache[static_cast<unsigned>(cardId)%64];
    if(slot.model==model && slot.list==entities && slot.source==source &&
       slot.count==entities->size && slot.id==cardId && slot.generation==matchGeneration.Value() &&
       (ticket-slot.ticket)<=32) return slot.result;
    struct Guard { bool& flag; explicit Guard(bool& f):flag(f){flag=true;} ~Guard(){flag=false;} } guard(evaluating);
    const bool result=ComputeGenericPick3ReviveBurst(cardId);
    slot={model,entities,source,entities->size,cardId,ticket,matchGeneration.Value(),result};
    return result;
}
// Before rewriting a gesture, prove its original release target is a real
// board lane in the current live recognition model. This prevents canceled
// drags (which often resolve to the dragged card itself) from casting a trick.
// This is a read-only structural check; it is NOT a replacement for stock
// physical lane legality or the later secondary-target validator.
inline bool IsReviveDropLaneOnCurrentModel(int targetId) {
    if (targetId<0 || !trickEntitiesField.IsValid() ||
        !trickEntityIdField.IsValid()) return false;
    Obj* model=ReviveRecognitionModel();
    auto* entities=model?trickEntitiesField[model].Get():nullptr;
    if (!entities || entities->size<0 || entities->size>20000 ||
        (entities->size && !entities->items)) return false;
    for (int i=0; i<entities->size; ++i) {
        Obj* entity=entities->items->m_Items[i];
        if (!entity || trickEntityIdField[entity].Get()!=targetId) continue;
        auto field=trickEntityComponentsField;
        auto* components=field.IsValid()?field[entity].Get():nullptr;
        if (!components || components->size<0 || components->size>128 ||
            (components->size && !components->items)) return false;
        for(int j=0; j<components->size; ++j) {
            Obj* component=components->items->m_Items[j];
            if (!component) continue;
            auto* type=BNM::Class(component).GetClass();
            if (type && type->name && !std::strcmp(type->name,"Lane")) return true;
        }
        return false;
    }
    return false;
}
// Identify the exact first-PlayTrigger revive effect behind a picker request.
// No GUID list: this is checked against the live effect ownership and the
// same strict structural signature used for lane-cast validation.
inline bool IsGenericPick3ReviveEffect(int effectId) {
    if(effectId<0 || !trickEntitiesField.IsValid() || !trickEntityIdField.IsValid() ||
       !reviveEffectOwnerSourceField.IsValid())return false;
    Obj* model=ReviveRecognitionModel();
    auto* entities=model?trickEntitiesField[model].Get():nullptr;
    if(!entities || entities->size<0 || entities->size>20000 || (entities->size && !entities->items))return false;
    int ownerId=-1;bool play=false;
    for(int i=0;i<entities->size;++i){
        Obj* e=entities->items->m_Items[i];if(!e || trickEntityIdField[e].Get()!=effectId)continue;
        auto f=trickEntityComponentsField;
        auto* components=f.IsValid()?f[e].Get():nullptr;
        if(!components || components->size<0 || components->size>128 || (components->size && !components->items))return false;
        for(int j=0;j<components->size;++j){
            Obj* c=components->items->m_Items[j];if(!c)continue;
            auto* cl=BNM::Class(c).GetClass();const char* name=cl?cl->name:nullptr;if(!name)continue;
            if(!std::strcmp(name,"PlayTrigger"))play=true;
            if(!std::strcmp(name,"EffectEntity"))ownerId=reviveEffectOwnerSourceField[c].Get();
        }
        break;
    }
    if(!play || ownerId<0 || !IsGenericPick3ReviveBurst(ownerId))return false;
    // Fail closed when this is a later PlayTrigger of that source card.
    for(int i=0;i<entities->size;++i){
        Obj* e=entities->items->m_Items[i];if(!e)continue;
        int id=trickEntityIdField[e].Get();if(id<0 || id>=effectId)continue;
        auto f=trickEntityComponentsField;
        auto* components=f.IsValid()?f[e].Get():nullptr;
        if(!components || components->size<0 || components->size>128 || (components->size && !components->items))continue;
        bool otherPlay=false;int otherOwner=-1;
        for(int j=0;j<components->size;++j){
            Obj* c=components->items->m_Items[j];if(!c)continue;
            auto* cl=BNM::Class(c).GetClass();const char* name=cl?cl->name:nullptr;if(!name)continue;
            if(!std::strcmp(name,"PlayTrigger"))otherPlay=true;
            if(!std::strcmp(name,"EffectEntity"))otherOwner=reviveEffectOwnerSourceField[c].Get();
        }
        if(otherPlay && otherOwner==ownerId)return false;
    }
    return true;
}
// The concrete target generator is GameStateUtils, already hooked by
// TrickValidTargetsTrace. Observe its returned list from that same hook rather
// than double-patching a native entry point.
inline void TrickLogGeneratedTargets(int player, int cardId, Obj* result) {
    const int guid = TrickGenerationGuid(cardId);
    if (!TrickFocus(guid) || ++trickGenerationEvents>100) return;
    auto* list = reinterpret_cast<IntList*>(result);
    if (!list || list->size < 0 || list->size > 256 || (list->size && !list->items)) {
        CC_SELECTION_DIAG("CustomComponents/Pick3/TRICK_GENERATION: phase=TARGETS guid=%d card=%d player=%d result=%p opaque=1", guid, cardId, player, result);
        return;
    }
    CC_SELECTION_DIAG("CustomComponents/Pick3/TRICK_GENERATION: phase=TARGETS guid=%d card=%d player=%d count=%d", guid,cardId,player,list->size);
    for (int i=0; i<std::min(list->size,16); ++i) {
        int id=list->items->m_Items[i];
        CC_SELECTION_DIAG("CustomComponents/Pick3/TRICK_GENERATION: phase=TARGET guid=%d card=%d index=%d entity=%d targetGuid=%d",guid,cardId,i,id,TrickGenerationGuid(id));
    }
}
inline Obj* TrickGeneratedPlays(Obj* self, int player, int cardId, Obj* validator, const void* method) {
    ScopedGenerationReviveRecognition generationScope(self);
    const auto previousSole=stockSoleInitialPrimary;
    stockSoleInitialPrimary={};
    const int scopedGuid = TrickGenerationGuid(cardId);
    PF3_REPAIR_TRACE("CustomComponents/Pick3/REPAIR1713: GENERATION_IDENTITY card=%d guid=%d player=%d deciderModel=%p root=%p",
        cardId,scopedGuid,player,(void*)generationScope.model,(void*)Target(boundEntityModelRoot));
    const unsigned priorDepth = trickValidationDepth;
    const int priorGuid = trickValidationGuid, priorCard = trickValidationCard;
    const unsigned priorOrders = trickOrderCalls, priorValidations = trickValidatorCalls;
    const auto priorMatrix = pick3Stage1Matrix;
    if (TrickFocus(scopedGuid)) {
        trickValidationDepth = priorDepth + 1;
        trickValidationGuid = scopedGuid;
        trickValidationCard = cardId;
        trickOrderCalls = 0;
        trickValidatorCalls = 0;
        ++trickTraceGenerationCount;
        pick3Stage1Matrix = {};
    }
    const bool pick3Revive = IsGenericPick3ReviveBurst(cardId);
    if (pick3Revive && trickTraceGenerationCount <= 96)
        CC_SELECTION_DIAG("CustomComponents/Pick3/PICK3_PAIR_CONTEXT: phase=GENERATION_ENTER guid=%d card=%d player=%d zone=%d validator=%p finderContext=not-captured primaryProducer=missing pairVerified=0", scopedGuid,cardId,player,FindDeferredInitialZone(cardId),validator);
    Obj* result = originalDeciderValidPlays ? originalDeciderValidPlays(self, player, cardId, validator, method) : nullptr;
    if (pick3Revive && trickTraceGenerationCount <= 160) {
        const auto& m = pick3Stage1Matrix;
        CC_SELECTION_DIAG("CustomComponents/Pick3/PICK3_STAGE1_REJECTION_MATRIX: guid=%d card=%d player=%d firstTargeting=%u/%u oneTime=%u/%u composite=%u/%u fighter=%u/%u openLane=%u/%u inHand=%u/%u null=%u/%u scope=stockGeneration override=0 pairVerified=0",
                 scopedGuid, cardId, player,
                 m.targetingRejects,m.targetingCalls,m.oneTimeRejects,m.oneTimeCalls,
                 m.compositeRejects,m.compositeCalls,m.fighterRejects,m.fighterCalls,
                 m.openLaneRejects,m.openLaneCalls,m.inHandRejects,m.inHandCalls,
                 m.nullRejects,m.nullCalls);
    }
    if (TrickFocus(scopedGuid) && trickTraceGenerationCount <= 160)
        CC_SELECTION_DIAG("CustomComponents/Pick3/TRICK_VALIDATION: phase=SUMMARY guid=%d card=%d player=%d validator=%p ordersCalls=%u tracedValidateCalls=%u coverage=partial",
                 scopedGuid, cardId, player, validator, trickOrderCalls, trickValidatorCalls);
    if (TrickFocus(scopedGuid)) {
        trickValidationDepth = priorDepth;
        trickValidationGuid = priorGuid;
        trickValidationCard = priorCard;
        trickOrderCalls = priorOrders;
        trickValidatorCalls = priorValidations;
        pick3Stage1Matrix = priorMatrix;
    }
    const int guid=TrickGenerationGuid(cardId);
    if (IsGenericPick3ReviveBurst(cardId)) {
        // Capture stock candidate count *before* modifying the single generated
        // Play's target. A casting-zone placeholder is not proof of legality.
        auto* stock = reinterpret_cast<BNM::Structures::Mono::List<Obj*>*>(result);
        const int count = stock && stock->size>=0 && stock->size<=256 &&
            (stock->size==0 || stock->items) ? stock->size : -1;
        CC_SELECTION_DIAG("CustomComponents/Pick3/PICK3_LEGALITY_AUDIT: phase=BEFORE_NORMALIZE guid=%d card=%d player=%d stockPlays=%d",guid,cardId,player,count);
        // Follow the exact stock result even when the card's only playable
        // state is a full board with a team-up destination. Never synthesize
        // a Play: legality remains owned by the stock decider/validator.
        // This trace correlates a missing Play with the previously computed
        // initial-zone candidate without inspecting transient managed lists.
        static unsigned zeroPlayAuditSamples = 0;
        if (count == 0 && zeroPlayAuditSamples++ < 240) {
            CC_SELECTION_DIAG("CustomComponents/Pick3/PICK_FROM_THREE_ZERO_PLAY: guid=%d card=%d player=%d initialZone=%d validator=%p hypothesis=stock-lane-placement-gate",
                     guid, cardId, player, FindDeferredInitialZone(cardId), validator);
        }
    }
    // Snapshot only the single validated zone play (not the pooled list).
    if(kDeferredPick3TrialEnabled && IsGenericPick3ReviveBurst(cardId)) {
        deferredValidatedPlay={};
        auto* candidates=reinterpret_cast<BNM::Structures::Mono::List<Obj*>*>(result);
        if(candidates && candidates->size==1 && candidates->items && trickPlayCardIdField.IsValid() &&
           trickPlayPlayerIdField.IsValid() && trickPlayTypeField.IsValid() && trickPlayTargetsField.IsValid()) {
            Obj* candidate=candidates->items->m_Items[0];
            if(candidate && trickPlayCardIdField[candidate].Get()==cardId &&
               trickPlayPlayerIdField[candidate].Get()==player &&
               trickPlayTypeField[candidate].Get()==0) {
                auto* ids=trickPlayTargetsField[candidate].Get();
                const int zone=FindDeferredInitialZone(cardId);
                if (ids && ids->size==1 && ids->items && zone>=0) {
                    const int generatedTarget=ids->items->m_Items[0];
                    const auto& sole=stockSoleInitialPrimary;
                    // The initial Play is deliberately generated for the casting
                    // entity. Its target is NOT the first effect's selected card.
                    // Bind the synchronous one-option candidate snapshot to the
                    // actual first effect/model, without modifying the generated
                    // casting Play (which the drag/UI logic depends on).
                    const auto firstProof=ResolvePick3InitialEffectOnModel(generationScope.model,cardId);
                    const bool exactSole=generatedTarget==zone &&
                        sole.card==cardId && sole.target>=0 &&
                        sole.target!=cardId && !sole.ambiguous &&
                        firstProof.complete && firstProof.effectId==sole.effect &&
                        sole.model && sole.model==generationScope.model &&
                        TrickGenerationGuid(sole.target)>0;
                    if (exactSole) {
                        deferredValidatedPlay={cardId,zone,player};
                        deferredValidatedPlay.stockSolePrimary=sole.target;
                        PF3_REPAIR_TRACE("CustomComponents/Pick3/REPAIR1717: SOLE_INITIAL_BOUND card=%d guid=%d cast=%d sole=%d effect=%d player=%d model=%p source=validatorPrimarySnapshot",
                            cardId,guid,zone,sole.target,sole.effect,player,(void*)sole.model);
                    } else if (generatedTarget==zone) {
                        deferredValidatedPlay={cardId,zone,player};
                        CC_SELECTION_DIAG("CustomComponents/Pick3/INITIAL_PLAY_BRIDGE: phase=ALREADY_ZONE guid=%d card=%d player=%d zone=%d",guid,cardId,player,zone);
                    } else {
                        // The original decider returned this Play as legal using the
                        // discarded-card target. Preserve that stock eligibility
                        // evidence, but make its initial Play destination the lane.
                        // Only modify the single generated Play for 712/713. Never
                        // touch the effect's own target finder or selection Play.
                        const int eligibleGuid=TrickGenerationGuid(generatedTarget);
                        const int zoneGuid=TrickGenerationGuid(zone);
                        if (generatedTarget>=0 && generatedTarget!=cardId &&
                            eligibleGuid>0 && zoneGuid<0) {
                            ids->items->m_Items[0]=zone;
                            deferredValidatedPlay={cardId,zone,player};
                            CC_SELECTION_DIAG("CustomComponents/Pick3/INITIAL_PLAY_BRIDGE: phase=NORMALIZED guid=%d card=%d player=%d eligibleEntity=%d eligibleGuid=%d zone=%d",guid,cardId,player,generatedTarget,eligibleGuid,zone);
                        } else {
                            CC_SELECTION_DIAG("CustomComponents/Pick3/INITIAL_PLAY_BRIDGE: phase=SKIP guid=%d card=%d player=%d target=%d targetGuid=%d zone=%d zoneGuid=%d",guid,cardId,player,generatedTarget,eligibleGuid,zone,zoneGuid);
                        }
                    }
                }
            }
        }
    }
    if (deferredValidatedPlay.card==cardId && deferredValidatedPlay.zone>=0 &&
        deferredValidatedPlay.player==player) {
        Obj* generationModel = ReviveRecognitionModel();
        const auto first = ResolvePick3InitialEffectOnModel(generationModel,cardId);
        if (first.complete && first.effectId>=0 && generationScope.model == generationModel &&
            generationModel && guid > 0) {
            CaptureRoot(reviveLegalPlayModelRoot,generationModel);
            if (Target(reviveLegalPlayModelRoot) != generationModel) {
                PF3_REPAIR_TRACE("CustomComponents/Pick3/REPAIR1713: PROOF_REJECT card=%d reason=root-failed",cardId);
                deferredValidatedPlay={};
            } else {
            deferredValidatedPlay.guid=guid;
            deferredValidatedPlay.effect=first.effectId;
            deferredValidatedPlay.provenAt=std::chrono::steady_clock::now();
            PF3_REPAIR_TRACE("CustomComponents/Pick3/REPAIR1712: STOCK_PLAY_PROOF card=%d guid=%d effect=%d player=%d zone=%d model=%p source=stockDecider",
                cardId,guid,first.effectId,player,deferredValidatedPlay.zone,(void*)generationModel);
            PF3_REPAIR_TRACE("CustomComponents/Pick3/REPAIR1713: PROOF_ROOT card=%d guid=%d effect=%d zone=%d model=%p rooted=%d",
                cardId,guid,first.effectId,deferredValidatedPlay.zone,(void*)generationModel,
                Target(reviveLegalPlayModelRoot)==generationModel?1:0);
            }
        } else {
            PF3_REPAIR_TRACE("CustomComponents/Pick3/REPAIR1712: STOCK_PLAY_UNPROVEN card=%d guid=%d zone=%d reason=FIRST_EFFECT_UNAVAILABLE",
                cardId,guid,deferredValidatedPlay.zone);
            deferredValidatedPlay={};
        }
    } else if (kDeferredPick3TrialEnabled && pick3Revive) {
        PF3_REPAIR_TRACE("CustomComponents/Pick3/REPAIR1712: STOCK_PLAY_UNPROVEN card=%d guid=%d zone=%d reason=NO_VALIDATED_SINGLE_PLAY",
            cardId,guid,FindDeferredInitialZone(cardId));
    }
    stockSoleInitialPrimary=previousSole;
    if (!TrickFocus(guid) || ++trickGenerationEvents>100) return result;
    auto* list = reinterpret_cast<BNM::Structures::Mono::List<Obj*>*>(result);
    if (!list || list->size<0 || list->size>256 || (list->size && !list->items)) {
        CC_SELECTION_DIAG("CustomComponents/Pick3/TRICK_GENERATION: phase=PLAYS guid=%d card=%d result=%p opaque=1",guid,cardId,result);
        return result;
    }
    CC_SELECTION_DIAG("CustomComponents/Pick3/TRICK_GENERATION: phase=PLAYS guid=%d card=%d player=%d count=%d",guid,cardId,player,list->size);
    for (int i=0; i<std::min(list->size,12); ++i) {
        Obj* play=list->items->m_Items[i];
        if (!play) continue;
        auto* bytes=reinterpret_cast<const std::uint8_t*>(play);
        int type=*reinterpret_cast<const int*>(bytes+0x10);
        int source=*reinterpret_cast<const int*>(bytes+0x14);
        int owner=*reinterpret_cast<const int*>(bytes+0x18);
        auto* targets=*reinterpret_cast<IntList* const*>(bytes+0x20);
        int order=*reinterpret_cast<const int*>(bytes+0x28);
        int count=(targets && targets->size>=0 && targets->size<=32 && (targets->size==0 || targets->items)) ? targets->size:-1;
        CC_SELECTION_DIAG("CustomComponents/Pick3/TRICK_GENERATION: phase=PLAY guid=%d index=%d type=%d source=%d owner=%d order=%d targetCount=%d",guid,i,type,source,owner,order,count);
        if(count>0) for(int j=0;j<std::min(count,8);++j) {
            int id=targets->items->m_Items[j];
            CC_SELECTION_DIAG("CustomComponents/Pick3/TRICK_GENERATION: phase=PLAY_TARGET guid=%d playIndex=%d index=%d entity=%d targetGuid=%d",guid,i,j,id,TrickGenerationGuid(id));
        }
    }
    return result;
}
}
