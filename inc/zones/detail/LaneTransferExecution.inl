#include "diagnostics/DiagnosticLogging.hpp"

struct LaneReturnOperation {
    Obj* model; Obj* system; Obj* effect; LaneTransferSource source;
    int cardId,laneId,effectId;
    Zones::DeckLaneCreation::Options options{};
    bool teamupAdded=false,aquaticAdded=false;
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
        const bool legal=source.deck?CheckDeckLanePair(model,cardId,laneId,source.playerId,FindEntity(model,effectId)):CheckDiscardRevivePair(model,cardId,laneId,source.playerId).legal;
        if(!legal) return false;
        environment=FindComponent(source.card,"Environment")!=nullptr;
        lane=FindComponent(FindEntity(model,laneId),"Lane");
        modifier=moveLaneSystemModifier[system].Get();history=moveLaneSystemHistory[system].Get();
        if(!modifier || !history || !laneModifierModel.IsValid() || laneModifierModel[modifier].Get()!=model ||
           !discardRemoveCard.IsValid() || !discardAddCard.IsValid() || !laneElementIds.IsValid() ||
           !historyAppendRecord.IsValid() || !historyRecords.IsValid() || !historyInstances.IsValid() ||
           !cardIdDataCreateFromEntity.IsValid() || !effectRecordSetDefaultIds.IsValid() || !effectRecordSetTargetId.IsValid()) return false;
        if(!environment && (!lanePlayCard.IsValid() || (options.creation.position!=Effects::SubsetPlacement::Position::Stock && !lanePlayCardWithPlacement.IsValid()) || !createLaneRecordSetCard.IsValid() ||
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
    bool GrantTrait(const char* name,bool& added) {
        if(Engine::BoardPlacement::Active(source.card,name))return true;
        auto type=BNM::Class("PvZCards.Engine.Components",name);
        auto getOrAdd=BNM::Class("PvZCards.Engine","Entity").GetMethod("GetOrAddComponent",1).cast<Obj*>();
        auto add=BNM::Class("PvZCards.Engine.Components","GrantableAbility").GetMethod("AddCounter",3).cast<void>();
        if(!type.IsValid() || !getOrAdd.IsValid() || !add.IsValid())return false;
        Obj* ability=getOrAdd[source.card](type.GetMonoType());
        if(!ability)return false;
        add[ability](0,effectId,0);
        added=true;
        return true;
    }
    void UndoGrants() {
        if(!source.deck)return;
        auto remove=BNM::Class("PvZCards.Engine.Components","GrantableAbility").GetMethod("RemoveCounter",2).cast<void>();
        if(!remove.IsValid())return;
        for(const auto& grant: {std::pair<const char*,bool>{"Teamup",teamupAdded},{"Aquatic",aquaticAdded}})
            if(grant.second)if(Obj* ability=FindComponent(source.card,grant.first))
                Try([&]{remove[ability](0,effectId);});
        teamupAdded=aquaticAdded=false;
    }
    bool Place() {
        // Only the rendered model records view work. Headless predictions still
        // perform the same gameplay mutation and produce their own history.
        if(rendered) Core::CardViewLifecycleCoordinator::NoteReturn(cardId,laneId,source.poolId);
        if(source.deck && !environment) {
            const bool granted=Try([&]{
                if(options.creation.teamup && !GrantTrait("Teamup",teamupAdded))return;
                if(options.creation.aquatic && !GrantTrait("Aquatic",aquaticAdded))return;
            });
            if(!granted || (options.creation.teamup && !Engine::BoardPlacement::Active(source.card,"Teamup")) ||
               (options.creation.aquatic && !Engine::BoardPlacement::Active(source.card,"Aquatic")))return false;
        }
        const bool placed=Try([&]{
            if(environment)environmentPlaceCard[environmentPlacer](cardId,laneId);
            else if(options.creation.position!=Effects::SubsetPlacement::Position::Stock) {
                // Respect authored front/back placement for physical discard revives too.
                // The source remains the exact discarded card; no duplicate is created.
                Effects::SubsetCreationExtensions::Placement placement{cardId,laneId,source.playerId,
                    Effects::SubsetPlacement::Order(options.creation.position,-1)};
                lanePlayCardWithPlacement[modifier](placement);
            } else lanePlayCard[modifier](cardId,laneId,source.playerId);
        });
        if(!placed || !IsPlaced())return false;
        if(source.deck && !environment && options.faceDown) {
            // Explicitly forced facedown physical card; stock subset creation
            // independently handles the same field for conjured cards.
            const auto face=BNM::Class("PvZCards.Engine.Components","FaceDown");
            auto add=BNM::Class("PvZCards.Engine","Entity").GetMethod("GetOrAddComponent",1).cast<Obj*>();
            if(face.IsValid() && add.IsValid())Try([&]{add[source.card](face.GetMonoType());});
        }
        if(!environment) {
            auto getOrder=BNM::Class("PvZCards.Engine.Components","Lane").GetMethod("GetOrderInLane",1).cast<int>();
            if(getOrder.IsValid())Try([&]{createLaneRecordSetOrder[record](getOrder[lane](cardId));});
            Try([&]{createLaneRecordSetFaceDown[record](FindComponent(source.card,"FaceDown")!=nullptr);});
        }
        return true;
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
        if(restored)UndoGrants();
        return restored;
    }
    bool Record() {
        // Refresh the card snapshot after physical grants, otherwise the
        // create-in-lane record would describe its pre-transfer abilities.
        if(source.deck && !environment)Try([&]{
            Obj* data=cardIdDataCreateFromEntity[static_cast<Obj*>(nullptr)](source.card);
            Core::ManagedRuntime::Root root(data);
            if(root.Get())createLaneRecordSetCard[record](data);
        });
        Try([&]{historyAppendRecord[history](record);});
        auto* records=historyRecords[history].Get();
        if(records && records->items && records->size>=0 && records->size<=int(records->items->capacity))
            for(int i=0;i<records->size;++i) if(records->items->m_Items[i]==record) recordSubmitted=true;
        return recordSubmitted;
    }
    void GrantRecord(int type) {
        if(!history)return;
        const auto recordClass=BNM::Class("PvZCards.Engine.Records","AbilityGrantedRecord");
        auto add=BNM::Class("PvZCards.Engine","History").GetMethod("AddRecord",
            {BNM::Class("System","Type").GetCompileTimeClass()}).cast<Obj*>();
        auto defaults=recordClass.GetMethod("SetDefaultIdsFromEffect",1).cast<void>();
        auto setTarget=recordClass.GetMethod("set_TargetId",1).cast<void>();
        auto setAbility=recordClass.GetMethod("set_AbilityType",1).cast<void>();
        auto setValue=recordClass.GetMethod("set_AbilityValue",1).cast<void>();
        if(!add.IsValid() || !defaults.IsValid() || !setTarget.IsValid() || !setAbility.IsValid() || !setValue.IsValid())return;
        Try([&]{
            Obj* granted=add[history](recordClass.GetMonoType());
            if(granted){defaults[granted](effect);setTarget[granted](cardId);setAbility[granted](type);setValue[granted](0);}
        });
    }
    void Publish() {
        if(rendered) {
            if(!source.deck)ZoneTriggeredAbilityPresentationSystem::RememberDiscardToLaneMove(model,cardId,effectId,laneId);

        }
        if(source.deck && !environment) {
            if(teamupAdded)GrantRecord(10);
            if(aquaticAdded)GrantRecord(11);
        }
        // A custom deck/discard-to-lane play bypasses the stock PlayCardEffect
        // preparation hook. Its synthetic PlayTrigger is an ACTUAL new play;
        // advance the Plant play count before creating any queued rewards.
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
    int cardId=effectTargetId[effect].Get();
    const int laneId=effectSecondaryTargetId[effect].Get();
    Obj* effectEntity=FindEntity(model,effectId);
    auto options=fromDeck?Zones::DeckLaneCreation::Read(
        FindComponent(effectEntity,"PlayFromDeckToLaneEffectDescriptor")):Zones::DeckLaneCreation::Options{};
    if(!fromDeck) {
        Obj* descriptor=FindComponent(effectEntity,"ReturnFromDiscardPileToLaneEffectDescriptor");
        if(descriptor) {
            auto* revive=reinterpret_cast<Components::ReturnFromDiscardPileToLaneEffectDescriptor*>(descriptor);
            options.creation.position=Effects::SubsetPlacement::Parse(
                revive->CreationPosition?revive->CreationPosition->str():std::string{});
        }
    }
    if(fromDeck)LOG_INFO("CustomComponents/PlayFromDeckToLane/EXECUTE: effect=%d target=%d lane=%d failsafe=%d",effectId,cardId,laneId,int(options.pluckFailSafe));
    // Only the authored PRIMARY selected physical deck Plant may be plucked.
    // If it cannot be deployed, the optional stock creation fallback uses
    // the stripped primary FILTER, never a secretly re-rolled physical card.
    LaneTransferSource source;
    if(!ResolveLaneSource(model,cardId,fromDeck,source)) {
        if(fromDeck && options.pluckFailSafe && !Zones::DeckLaneCreation::RunStockFallback(effect,effectEntity,laneId,options))
            LOG_WARN("CustomComponents/PlayFromDeckToLane: no physical deck card and stock fallback unavailable");
        return;
    }
    struct Identity {Obj* model;int card;};
    static thread_local std::vector<Identity> active;
    for(const auto& value:active) if(value.model==model && value.card==cardId) return;
    active.push_back({model,cardId});
    struct Guard {std::vector<Identity>& entries;~Guard(){entries.pop_back();}} guard{active};
    LaneReturnOperation operation{model,system,effect,source,cardId,laneId,effectId,options};
    const auto result=Zones::ExecuteTransfer(operation);
    if(operation.IsPlaced()){
        History::Record record;record.kind=History::Kind::ZoneTransfer;
        record.visual=History::VisualPolicy::NoVisual;record.entityId=cardId;record.ownerId=source.playerId;
        record.sourceId=Variables::Field(effect,"SourceId");record.effectId=effectId;
        record.from=fromDeck?History::Origin::Deck:History::Origin::DiscardPile;record.to=History::Origin::Lane;
        record.stage=result.phase==Zones::TransferPhase::Complete?History::Stage::Committed:History::Stage::PartialFailure;
        History::TransferPayload payload{cardId,source.poolId,laneId,source.index,-1};
        payload.owner=source.playerId;payload.guid=Engine::BoardPlacement::Guid(source.card);
        payload.stockRecorded=operation.recordSubmitted;payload.faceDown=Engine::EntityAccess::Component(source.card,"FaceDown")!=nullptr;
        payload.teamup=Engine::BoardPlacement::Active(source.card,"Teamup");payload.aquatic=Engine::BoardPlacement::Active(source.card,"Aquatic");
        static auto order=BNM::Class("PvZCards.Engine.Components","Lane").GetMethod("GetOrderInLane",1).cast<int>();
        if(order.IsValid())payload.order=Core::Metadata::Bind(order,operation.lane)(cardId);
        record.payload=std::move(payload);History::Writer::Commit(std::move(record),model,operation.instances);
    }
    if(result.phase==Zones::TransferPhase::Complete &&
       reinterpret_cast<std::uintptr_t>(model)==Core::SceneViewContext::ModelIdentity())
        Integration::ReplayEventBridge::Publish("zone_card_to_lane", {
            {"entityId",cardId},{"laneId",laneId},{"fromDeck",int(fromDeck)},
            {"sourcePoolId",source.poolId},{"ownerId",source.playerId},
            {"verifiedTransfer",1}});
    if(result.phase!=Zones::TransferPhase::Complete) {
        // Only a pre-mutation failure may be retried through creation. A
        // partially transferred card must NEVER be cloned by the fallback.
        if(fromDeck && result.phase==Zones::TransferPhase::Prepare && options.pluckFailSafe &&
           Zones::DeckLaneCreation::RunStockFallback(effect,effectEntity,laneId,options))return;
        LOG_ERR("CustomComponents/ReturnToLane: failed card=%d lane=%d phase=%d committed=%d recovered=%d",cardId,laneId,int(result.phase),int(result.committed),int(result.recovered));
    }
}

