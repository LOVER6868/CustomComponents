#include "diagnostics/DiagnosticLogging.hpp"

// A return is a zone transfer of the original entity. The stock movement
// runtime is only a carrier; it must not produce a MoveTrigger.
static void MoveLaneProcessEffectHook(void* self, Obj* effect, const void* method) {
    if(!self || !effect) { originalMoveLaneProcessEffect(self,effect,method); return; }
    Obj* system=static_cast<Obj*>(self);
    Obj* model=moveLaneSystemModel[system].Get();
    const int effectId=effectEffectEntityId[effect].Get();
    if(!FindComponent(FindEntity(model,effectId),"ReturnFromDiscardPileToLaneEffectDescriptor")) {
        originalMoveLaneProcessEffect(self,effect,method); return;
    }

    const int cardId=effectTargetId[effect].Get();
    const int laneId=effectSecondaryTargetId[effect].Get();
    DiscardContext source;
    if(!ResolveDiscardContext(model,cardId,source)) return;
    const auto pair=CheckDiscardRevivePair(model,cardId,laneId,source.playerId);
    if(!pair.legal) {
        LOG_WARN("CustomComponents/ReturnToLane: rejected card=%d lane=%d reason=%s",cardId,laneId,pair.reason);
        return;
    }
    const bool environment=FindComponent(source.card,"Environment")!=nullptr;
    Obj* lane=FindComponent(FindEntity(model,laneId),"Lane");
    Obj* modifier=moveLaneSystemModifier[system].Get();
    Obj* history=moveLaneSystemHistory[system].Get();
    const auto recordClass=BNM::Class("PvZCards.Engine.Records",
        environment ? "CreateEnvironmentInLaneRecord" : "CreateCardInLaneRecord");
    // Resolve all placement/history dependencies before removing the card.
    if(!modifier || !history || !laneModifierModel.IsValid() || laneModifierModel[modifier].Get()!=model ||
       !discardRemoveCard.IsValid() || !discardModifierClass.IsValid() || !discardModifierEntities.IsValid() ||
       !historyAddRecordByType.IsValid() || !cardIdDataCreateFromEntity.IsValid() ||
       !effectRecordSetDefaultIds.IsValid() || !effectRecordSetTargetId.IsValid() ||
       !laneElementIds.IsValid() || !recordClass.IsValid()) return;
    if(!environment && (!lanePlayCard.IsValid() || !createLaneRecordSetCard.IsValid() ||
       !createLaneRecordSetOrder.IsValid() || !createLaneRecordSetFaceDown.IsValid())) return;

    // Preserve the established view-lifecycle guard: an old mapping cannot be
    // reused as a new lane card. Never remove mappings owned by the frontend.
    const auto modelIdentity=reinterpret_cast<uintptr_t>(model);
    Obj* mapper=Core::SceneViewContext::RootedMapper();
    if(!mapper || Core::SceneViewContext::ModelIdentity()!=modelIdentity ||
       !Core::CardViewLifecycleCoordinator::getView.IsValid()) return;
    if(Core::CardViewLifecycleCoordinator::getView[mapper](cardId)) {
        LOG_WARN("CustomComponents/ReturnToLane: card=%d still has a view; transfer skipped",cardId);
        return;
    }

    // Only synchronous identities survive here. Nested returns of another card
    // or another model are allowed; the same in-flight transfer is not.
    struct ActiveReturn { Obj* model; int card; };
    static thread_local std::vector<ActiveReturn> active;
    for(const auto& item:active) if(item.model==model && item.card==cardId) return;
    active.push_back({model,cardId});
    struct ReturnScope {
        std::vector<ActiveReturn>& active;
        ~ReturnScope(){ active.pop_back(); }
    } scope{active};

    Obj* discardModifier=ScratchDiscardModifier(model);
    Obj* environmentPlacer=nullptr;
    if(environment) {
        if(!environmentPlacerClass.IsValid()) return;
        environmentPlacer=environmentPlacerClass.CreateNewInstance();
        if(environmentPlacer) environmentPlacerCtor[environmentPlacer](model);
        if(!environmentPlacer) return;
    }
    if(!discardModifier) return;
    discardRemoveCard[discardModifier](cardId,source.discardPileId);
    Obj* card=FindEntity(model,cardId);
    if(!card || DiscardContains(source.discardPile,cardId) ||
       FindComponent(card,"DiscardPileElement") || FindComponent(card,"LaneElement")) {
        LOG_ERR("CustomComponents/ReturnToLane: removal failed card=%d; placement skipped",cardId);
        return;
    }

    Core::CardViewLifecycleCoordinator::NoteReturn(cardId,laneId,source.discardPileId);
    if(environment) environmentPlaceCard[environmentPlacer](cardId,laneId);
    else lanePlayCard[modifier](cardId,laneId,source.playerId);

    card=FindEntity(model,cardId);
    Obj* element=FindComponent(card,"LaneElement");
    auto* lanes=element ? laneElementIds[element].Get() : nullptr;
    bool placed=false;
    if(lanes && lanes->items && lanes->size>0 && lanes->size<=32 && lanes->size<=int(lanes->items->capacity))
        for(int i=0;i<lanes->size;++i) if(lanes->items->m_Items[i]==laneId) placed=true;
    if(environment) placed=placed && laneEnvironmentId[lane].Get()==cardId;
    if(!placed || !card || DiscardContains(source.discardPile,cardId) || FindComponent(card,"DiscardPileElement")) {
        LOG_ERR("CustomComponents/ReturnToLane: placement verification failed card=%d lane=%d",cardId,laneId);
        return;
    }

    ZoneTriggeredAbilityPresentationSystem::RememberDiscardToLaneMove(model,cardId,effectId,laneId);
    ZoneTriggeredAbilityPresentationSystem::RememberHandDeckReturn(model,cardId,3);
    Obj* data=cardIdDataCreateFromEntity[static_cast<Obj*>(nullptr)](card);
    if(!data) return;
    Obj* record=historyAddRecordByType[history](reinterpret_cast<Obj*>(recordClass.GetMonoType()));
    if(!record) return;
    if(environment) createEnvironmentRecordSetCard[record](data);
    else {
        createLaneRecordSetCard[record](data);
        createLaneRecordSetOrder[record](0);
        createLaneRecordSetFaceDown[record](false);
    }
    effectRecordSetDefaultIds[record](effect);
    effectRecordSetTargetId[record](laneId);

    // Preserve return/play listeners and their original card target. Both
    // broadcasts must use this simulation model, including in PvP.
    const bool played=CustomTriggerSystem::FireInModel("PlayTrigger",effect,model);
    const bool returned=CustomTriggerSystem::FireInModel("ReturnedFromDiscardPileTrigger",effect,model);
    LOG_INFO("CustomComponents/ReturnToLane: completed card=%d lane=%d environment=%d playEvent=%d returnEvent=%d",
        cardId,laneId,int(environment),int(played),int(returned));
}
