#include "diagnostics/DiagnosticLogging.hpp"

struct LaneReturnOperation {
    Obj* model; Obj* system; Obj* effect; LaneTransferSource source;
    int cardId,laneId,effectId;
    bool environment=false,rendered=false,recordSubmitted=false;
    int replacedEnvironment=-1;
    Obj* lane=nullptr;Obj* modifier=nullptr;Obj* discardModifier=nullptr;
    Obj* environmentPlacer=nullptr;Obj* history=nullptr;Obj* instances=nullptr;Obj* record=nullptr;
    std::shared_ptr<Core::ManagedRuntime::Root> recordRoot;
    std::shared_ptr<Core::ManagedRuntime::Root> discardModifierRoot,environmentPlacerRoot;

    template<class Action> bool Try(Action action) {
        const auto error=BNM::TryInvoke(action);
        return !error.IsValid();
    }
    bool IsPlaced() {
        Obj* card=FindEntity(model,cardId);
        Obj* element=FindComponent(card,"LaneElement");
        auto* lanes=element && laneElementIds.IsValid()?laneElementIds[element].Get():nullptr;
        bool found=false;
        if(lanes && lanes->items && lanes->size>0 && lanes->size<=32 && lanes->size<=int(lanes->items->capacity))
            for(int i=0;i<lanes->size;++i) if(lanes->items->m_Items[i]==laneId) found=true;
        return found && card && !SourceContains(model,source,cardId) &&
            !FindComponent(card,"DiscardPileElement") && !FindComponent(card,"DeckElement") && (!environment || laneEnvironmentId[lane].Get()==cardId);
    }
    bool Prepare() {
        const bool legal=source.deck?CheckDeckLanePair(model,cardId,laneId,source.playerId):CheckDiscardRevivePair(model,cardId,laneId,source.playerId).legal;
        if(!legal) return false;
        environment=FindComponent(source.card,"Environment")!=nullptr;
        lane=FindComponent(FindEntity(model,laneId),"Lane");
        modifier=moveLaneSystemModifier[system].Get();history=moveLaneSystemHistory[system].Get();
        if(!modifier || !history || !laneModifierModel.IsValid() || laneModifierModel[modifier].Get()!=model ||
           !discardRemoveCard.IsValid() || !discardAddCard.IsValid() || !laneElementIds.IsValid() ||
           !historyAppendRecord.IsValid() || !historyRecords.IsValid() || !historyInstances.IsValid() ||
           !cardIdDataCreateFromEntity.IsValid() || !effectRecordSetDefaultIds.IsValid() || !effectRecordSetTargetId.IsValid()) return false;
        if(!environment && (!lanePlayCard.IsValid() || !createLaneRecordSetCard.IsValid() ||
           !createLaneRecordSetOrder.IsValid() || !createLaneRecordSetFaceDown.IsValid())) return false;
        if(environment && (!laneEnvironmentId.IsValid() || !environmentPlacerClass.IsValid() ||
           !environmentPlacerCtor.IsValid() || !environmentPlaceCard.IsValid() || !createEnvironmentRecordSetCard.IsValid())) return false;
        instances=historyInstances[history].Get();
        rendered=Core::SceneViewContext::MapperForModel(reinterpret_cast<uintptr_t>(model))!=nullptr;
        if(!Try([&]{
            discardModifier=ScratchDiscardModifier(model);
            discardModifierRoot=std::make_shared<Core::ManagedRuntime::Root>(discardModifier);
            if(!discardModifierRoot->Get()) return;
            if(environment) {
                replacedEnvironment=laneEnvironmentId[lane].Get();
                environmentPlacer=environmentPlacerClass.CreateNewInstance();
                environmentPlacerRoot=std::make_shared<Core::ManagedRuntime::Root>(environmentPlacer);
                if(!environmentPlacerRoot->Get()) return;
                if(environmentPlacer) environmentPlacerCtor[environmentPlacer](model);
            }
            auto type=BNM::Class("PvZCards.Engine.Records",environment?"CreateEnvironmentInLaneRecord":"CreateCardInLaneRecord");
            record=Core::ManagedPool::Acquire(instances,type);
            if(!record) return;
            recordRoot=std::make_shared<Core::ManagedRuntime::Root>(record);
            if(!recordRoot->Get()) return;
            Obj* data=cardIdDataCreateFromEntity[static_cast<Obj*>(nullptr)](source.card);
            Core::ManagedRuntime::Root dataRoot(data);
            if(!dataRoot.Get()) {record=nullptr;return;}
            if(environment) createEnvironmentRecordSetCard[record](data);
            else {createLaneRecordSetCard[record](data);createLaneRecordSetOrder[record](0);createLaneRecordSetFaceDown[record](false);}
            effectRecordSetDefaultIds[record](effect);effectRecordSetTargetId[record](laneId);
        })) return false;
        return discardModifierRoot && discardModifierRoot->Get() && record && recordRoot && recordRoot->Get() &&
            (!environment || (environmentPlacerRoot && environmentPlacerRoot->Get()));
    }
    bool Remove() {
        if(source.deck){if(!Try([&]{RemoveDeckLaneSource(model,source,cardId);}))return false;}
        else Try([&]{discardRemoveCard[discardModifier](cardId,source.poolId);});
        Obj* card=FindEntity(model,cardId);
        return card && !SourceContains(model,source,cardId) && !FindComponent(card,"DiscardPileElement") && !FindComponent(card,"DeckElement") && !FindComponent(card,"LaneElement");
    }
    bool Place() {
        // Only the rendered model records view work. Headless predictions still
        // perform the same gameplay mutation and produce their own history.
        if(rendered) Core::CardViewLifecycleCoordinator::NoteReturn(cardId,laneId,source.poolId);
        Try([&]{if(environment) environmentPlaceCard[environmentPlacer](cardId,laneId);
                else lanePlayCard[modifier](cardId,laneId,source.playerId);});
        return IsPlaced();
    }
    bool Recover() {
        Obj* card=FindEntity(model,cardId);
        if(!card || FindComponent(card,"LaneElement") || FindComponent(card,"HandElement") || (!source.deck && FindComponent(card,"DeckElement"))) return false;
        bool restored=source.deck?RestoreDeckLaneSource(model,source,cardId):(DiscardContains(source.pool,cardId) && FindComponent(card,"DiscardPileElement"));
        if(!source.deck && !restored && !FindComponent(card,"DiscardPileElement") && !SourceContains(model,source,cardId)) {
            Try([&]{discardAddCard[discardModifier](cardId,source.poolId);});
            restored=DiscardContains(source.pool,cardId) && FindComponent(card,"DiscardPileElement");
        }
        // Compensate a displaced environment only if nobody else has filled
        // its slot and its original entity is still cleanly in a discard pile.
        if(environment && replacedEnvironment>=0 && laneEnvironmentId[lane].Get()!=replacedEnvironment) {
            if(laneEnvironmentId[lane].Get()!=-1) return false;
            DiscardContext previous;
            if(!ResolveDiscardContext(model,replacedEnvironment,previous) || FindComponent(previous.card,"LaneElement")) return false;
            if(!Try([&]{discardRemoveCard[discardModifier](replacedEnvironment,previous.discardPileId);
                        environmentPlaceCard[environmentPlacer](replacedEnvironment,laneId);})) return false;
            restored=restored && laneEnvironmentId[lane].Get()==replacedEnvironment;
        }
        return restored;
    }
    bool Record() {
        Try([&]{historyAppendRecord[history](record);});
        auto* records=historyRecords[history].Get();
        if(records && records->items && records->size>=0 && records->size<=int(records->items->capacity))
            for(int i=0;i<records->size;++i) if(records->items->m_Items[i]==record) recordSubmitted=true;
        return recordSubmitted;
    }
    void Publish() {
        if(rendered) {
            if(!source.deck)ZoneTriggeredAbilityPresentationSystem::RememberDiscardToLaneMove(model,cardId,effectId,laneId);

        }
        CustomTriggerSystem::FireInModel("PlayTrigger",effect,model);
        if(!source.deck)CustomTriggerSystem::FireInModel("ReturnedFromDiscardPileTrigger",effect,model);
    }
    ~LaneReturnOperation() {
        if(!recordSubmitted && recordRoot && recordRoot->Get())
            Try([&]{Core::ManagedPool::Return(instances,recordRoot->Get());});
    }
};

static void MoveLaneProcessEffectHook(void* self,Obj* effect,const void* method) {
    if(!self || !effect) {originalMoveLaneProcessEffect(self,effect,method);return;}
    Obj* system=static_cast<Obj*>(self);
    Obj* model=moveLaneSystemModel[system].Get();
    const int effectId=effectEffectEntityId[effect].Get();
    const bool fromDeck=FindComponent(FindEntity(model,effectId),"PlayFromDeckToLaneEffectDescriptor")!=nullptr;
    if(!fromDeck && !FindComponent(FindEntity(model,effectId),"ReturnFromDiscardPileToLaneEffectDescriptor")) {
        originalMoveLaneProcessEffect(self,effect,method);return;
    }
    const int cardId=effectTargetId[effect].Get(),laneId=effectSecondaryTargetId[effect].Get();
    LaneTransferSource source;
    if(!ResolveLaneSource(model,cardId,fromDeck,source)) return;
    struct Identity {Obj* model;int card;};
    static thread_local std::vector<Identity> active;
    for(const auto& value:active) if(value.model==model && value.card==cardId) return;
    active.push_back({model,cardId});
    struct Guard {std::vector<Identity>& entries;~Guard(){entries.pop_back();}} guard{active};
    LaneReturnOperation operation{model,system,effect,source,cardId,laneId,effectId};
    const auto result=Zones::ExecuteTransfer(operation);
    if(result.phase!=Zones::TransferPhase::Complete)
        LOG_ERR("CustomComponents/ReturnToLane: failed card=%d lane=%d phase=%d committed=%d recovered=%d",cardId,laneId,int(result.phase),int(result.committed),int(result.recovered));
}

