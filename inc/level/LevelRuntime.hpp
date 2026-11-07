#include "engine/ZoneChanges.hpp"
#pragma once
#include <thread>
#include <tuple>
#include "level/LevelEffectMetadata.hpp"
#include "level/LevelStats.hpp"
#include "engine/BoardPlacement.hpp"
#include "engine/EffectGuards.hpp"
#include "level/LevelQueryState.hpp"
#include "engine/PlayValidation.hpp"
#include "engine/FrontendServices.hpp"
#include "engine/PlayServices.hpp"
namespace CustomComponents::Level {
inline Obj*(*renderModel)()=nullptr;
inline int(*localEntry)(Obj*,int) = nullptr;
inline bool(*isLocalCard)(Obj*,int)=nullptr;
inline bool(*isFrontend)()=nullptr;
inline thread_local Obj* costComponent=nullptr;inline thread_local int overrideCost=0;
inline int(*originalCost)(Obj*,Obj*,const void*)=nullptr;
inline bool(*originalValidate)(Obj*,Obj*,Obj*,const void*)=nullptr;
inline bool(*originalFreeCheck)(Obj*,Obj*,const void*)=nullptr;
inline std::shared_ptr<Core::ManagedRuntime::Root> costCalculator,sunCalculator;
inline thread_local unsigned effectDepth=0;
struct Departure {Core::ModelRegistry::Token model;int card;};
inline thread_local std::vector<Departure> departures;
struct CostState {Obj* component;int total;};inline thread_local std::vector<CostState> costStack;
inline int PlayInt(Obj* play,const char* name){
    static auto type=BNM::Class("PvZCards.Engine.Dtos","Play");static auto card=type.GetField("CardId").cast<int>();static auto player=type.GetField("PlayerId").cast<int>();static auto kind=type.GetField("Type").cast<int>();
    auto field=std::string_view(name)=="CardId"?card:std::string_view(name)=="PlayerId"?player:kind;return play?field[play].Get():-1;
}
inline int EffectInt(Obj* effect,const char* name){static auto type=BNM::Class("PvZCards.Engine","Effect");static auto target=type.GetField("TargetId").cast<int>();static auto player=type.GetField("PlayerId").cast<int>();auto field=std::string_view(name)=="TargetId"?target:player;return effect?field[effect].Get():-1;}
inline std::shared_ptr<const Definition> DefinitionFor(Obj* card){return definitions.Get(Engine::EntityAccess::Component(card,"Level"));}
inline int Available(Obj* model,int player) {
    Obj* owner=Engine::EntityAccess::Find(model,player);Obj* counter=Engine::EntityAccess::Component(owner,"SunCounter");auto get=BNM::Class("PvZCards.Core.Systems.Utilities","SunCountCalculator").GetMethod("GetAvailableSun",1).cast<int>();
    return counter && sunCalculator && sunCalculator->Get() && get.IsValid()?get[sunCalculator->Get()](counter):-1;
}
inline int BaseCost(Obj* card) {
    auto* selected=reinterpret_cast<Application*>(Engine::EntityAccess::Component(card,"LevelApplication"));
    if(Engine::EntityAccess::Component(card,"IsFree") || (selected && selected->InHandSelection && selected->WasFree))return 0;
    Obj* value=Stats::Value(card,"SunCost","SunCostValue");return value?std::max(0,Stats::WithoutLevel(value)):-1;
}
inline Quote QuoteFor(Obj* model,Obj* card,int entry) {
    auto definition=DefinitionFor(card);if(!definition || entry<0 || entry>=int(definition->entries.size()))return {};
    auto quote=Calculate(definition->entries[entry],BaseCost(card),Available(model,Engine::BoardPlacement::Owner(card)));
    const auto delta=std::int64_t(quote.total)-Stats::WithoutLevel(Stats::Value(card,"SunCost","SunCostValue"));if(delta<std::numeric_limits<int>::min() || delta>std::numeric_limits<int>::max())quote.valid=false;
    for(auto item:{std::tuple{"Attack","AttackValue",quote.attack},std::tuple{"Health","MaxHealth",quote.health}})if(Obj* value=Stats::Value(card,std::get<0>(item),std::get<1>(item))){auto total=std::int64_t(Stats::WithoutLevel(value))+std::get<2>(item);if(total<std::numeric_limits<int>::min() || total>std::numeric_limits<int>::max())quote.valid=false;}
    return quote;
}
inline Obj* HandCard(Obj* model,int id){
    Obj* visible=Engine::EntityAccess::Find(model,id);Obj* projection=Engine::FrontendServices::Projection();
    if(!visible || !projection || projection==model || !isFrontend || !isFrontend())return visible;
    Obj* card=Engine::EntityAccess::Find(projection,id);if(!card || !Engine::EntityAccess::Component(card,"HandElement") || !DefinitionFor(card) || Engine::BoardPlacement::Owner(card)!=Engine::BoardPlacement::Owner(visible))return visible;
    Obj* a=Engine::EntityAccess::Component(card,"Card");Obj* b=Engine::EntityAccess::Component(visible,"Card");
    static auto guidMetadata=BNM::Class("PvZCards.Engine.Components","Card").GetField("Guid").cast<int>();auto guid=guidMetadata;
    return a && b && guid[a].Get()==guid[b].Get()?card:visible;
}
inline std::optional<int> PreviewValue(Obj* card){
    if(!card || !DefinitionFor(card) || !Engine::EntityAccess::Component(card,"HandElement"))return std::nullopt;
    auto* state=reinterpret_cast<Application*>(Engine::EntityAccess::Component(card,"LevelApplication"));return state?state->Value:0;
}
inline int SelectedEntry(Obj* card){auto* state=reinterpret_cast<Application*>(Engine::EntityAccess::Component(card,"LevelApplication"));return state && state->InHandSelection?state->EntryIndex:-1;}
inline void BindOutgoing(Obj* play){
    if(!play || PlayInt(play,"Type")!=3 || !renderModel || !isFrontend || !isFrontend() || !isLocalCard)return;
    Obj* model=renderModel();int id=PlayInt(play,"CardId");if(!isLocalCard(model,id))return;
    Obj* card=HandCard(model,id);if(!DefinitionFor(card))return;
    Plays::choices.Set(play,{true,true,SelectedEntry(card)});Plays::frontendChoices.Set(play,true);
}
inline void BindValidation(Obj* model,Obj* play){
    if(!play || PlayInt(play,"Type")!=3 || (Plays::choices.Get(play).present && !Plays::frontendChoices.Get(play)))return;
    Obj* card=Engine::EntityAccess::Find(model,PlayInt(play,"CardId"));if(!DefinitionFor(card))return;
    Plays::choices.Set(play,{true,true,SelectedEntry(card)});Plays::frontendChoices.Set(play,true);
}
static int Cost(Obj* self,Obj* component,const void* method){return component && component==costComponent?overrideCost:originalCost(self,component,method);}
static bool FreeCheck(Obj* self,Obj* card,const void* method){return costComponent && Engine::EntityAccess::Component(card,"SunCost")==costComponent?false:originalFreeCheck(self,card,method);}
static bool Validate(Obj* self,Obj* play,Obj* errors,const void* method) {
    auto field=BNM::Class("PvZCards.Core.Utilities","SunValidator").GetField("<EntityModel>k__BackingField").cast<Obj*>();Obj* model=field[self].Get();
    BindValidation(model,play);
    auto choice=Plays::choices.Get(play);if(PlayInt(play,"Type")!=3 || !choice.present)return originalValidate(self,play,errors,method);
    if(choice.valid && choice.entry<0){Obj* card=Engine::EntityAccess::Find(model,PlayInt(play,"CardId"));return SelectedEntry(card)<0 && originalValidate(self,play,errors,method);}
    Obj* card=Engine::EntityAccess::Find(model,PlayInt(play,"CardId"));auto quote=QuoteFor(model,card,choice.entry);
    auto previous=costComponent;int prior=overrideCost;struct RestoreCost{Obj* previous;int value;~RestoreCost(){costComponent=previous;overrideCost=value;}}restoreCost{previous,prior};costComponent=Engine::EntityAccess::Component(card,"SunCost");overrideCost=quote.valid?quote.total:std::numeric_limits<int>::max();
    const bool stock=originalValidate(self,play,errors,method);costComponent=previous;overrideCost=prior;
    return stock && choice.valid && choice.entry==SelectedEntry(card) && quote.valid && quote.affordable && Engine::BoardPlacement::Owner(card)==PlayInt(play,"PlayerId") && Engine::EntityAccess::Component(card,"HandElement");
}
inline Obj* ModelFor(Obj* router){return Core::ModelRegistry::Get(Engine::RouterContext::RouterModel(router));}
inline bool IsPlay(Obj* effect){return effect && effect->klass && std::string_view(effect->klass->name)=="PlayCardEffect";}
inline bool Guard(const Engine::EffectGuards::Context& context,int) {
    if(!IsPlay(context.effect))return true;auto choice=Plays::effects.Get(context.effect);
    if(!choice.present)return true;
    if(choice.valid && choice.entry<0)return SelectedEntry(Engine::EntityAccess::Find(ModelFor(context.router),EffectInt(context.effect,"TargetId")))<0;
    Obj* model=ModelFor(context.router);Obj* card=Engine::EntityAccess::Find(model,EffectInt(context.effect,"TargetId"));auto quote=QuoteFor(model,card,choice.entry);
    return choice.valid && choice.entry==SelectedEntry(card) && quote.valid && quote.affordable && Engine::EntityAccess::Component(card,"HandElement") && Engine::BoardPlacement::Owner(card)==EffectInt(context.effect,"PlayerId");
}
inline void Before(const Engine::EffectGuards::Context&) {++effectDepth;costStack.push_back({costComponent,overrideCost});}
inline bool Prepare(const Engine::EffectGuards::Context& context) {
    if(!IsPlay(context.effect))return true;
    auto choice=Plays::effects.Get(context.effect);if(!choice.present || choice.entry<0 || !choice.valid)return true;
    Obj* model=ModelFor(context.router);Obj* card=Engine::EntityAccess::Find(model,EffectInt(context.effect,"TargetId"));auto quote=QuoteFor(model,card,choice.entry);
    if(!quote.valid || !quote.affordable || !Stats::Apply(card,quote,choice.entry)){auto cancel=BNM::Class("PvZCards.Engine","Effect").GetMethod("Cancel",0).cast<void>();cancel[context.effect]();return false;}
    if(auto* state=reinterpret_cast<Application*>(Engine::EntityAccess::Component(card,"LevelApplication"))){state->InHandSelection=false;state->WasFree=false;state->WasOnBoard=Engine::CardTypes::Fighter(card) || Engine::EntityAccess::Component(card,"Environment");}
    costComponent=Engine::EntityAccess::Component(card,"SunCost");overrideCost=quote.total;
    // Stock consumes IsFree instead of consulting the cost calculator. A Level
    // choice with extra payment consumes that marker and pays its quoted cost.
    if(quote.extra>0 && Engine::EntityAccess::Component(card,"IsFree")){auto remove=BNM::Class("PvZCards.Engine","Entity").GetMethod("RemoveComponent",1).cast<Obj*>();remove[card](BNM::Class("PvZCards.Engine.Components","IsFree").GetMonoType());}
    static std::atomic<unsigned> samples{0};if(samples.fetch_add(1)<64)LOG_INFO("CustomComponents/Level/COMMIT: card=%d entry=%d cost=%d extra=%d attack=%d health=%d",EffectInt(context.effect,"TargetId"),choice.entry,quote.total,quote.extra,quote.attack,quote.health);
    return true;
}
inline std::mutex handIdsLock;
inline std::vector<int> handIds;
inline void ObserveHand(Obj* card){
    Obj* element=Engine::EntityAccess::Component(card,"HandElement");if(!element)return;
    auto field=BNM::Class("PvZCards.Engine.Components","HandElement").GetField("HandEntityId").cast<int>();int id=field[element].Get();
    std::lock_guard<std::mutex> guard(handIdsLock);if(std::find(handIds.begin(),handIds.end(),id)==handIds.end())handIds.push_back(id);
}
inline bool SetHandMode(Obj* model,Obj* card,int entry){
    auto definition=DefinitionFor(card);if(!model || !card || !definition || entry < -1 || entry>=int(definition->entries.size()) || !Engine::EntityAccess::Component(card,"HandElement"))return false;
    if(entry<0){Stats::Clear(card);return true;}
    const int base=BaseCost(card),available=Available(model,Engine::BoardPlacement::Owner(card));
    if(ResetDynamic(definition->entries[entry],base,available)){Stats::Clear(card);return true;}
    auto quote=QuoteFor(model,card,entry);if(!quote.valid || !Stats::Apply(card,quote,entry))return false;
    auto* state=reinterpret_cast<Application*>(Engine::EntityAccess::Component(card,"LevelApplication"));state->InHandSelection=true;
    if(state->WasFree && quote.extra>0){auto remove=BNM::Class("PvZCards.Engine","Entity").GetMethod("RemoveComponent",1).cast<Obj*>();remove[card](BNM::Class("PvZCards.Engine.Components","IsFree").GetMonoType());}
    ObserveHand(card);return true;
}
inline bool HandCommand(Obj* service,Obj* play){
    auto fail=BNM::Class("PvZCards.Engine","BaseService").GetMethod("DispatchFailureDelegate",1).cast<void>();
    auto success=BNM::Class("PvZCards.Engine","BaseService").GetMethod("DispatchSuccessDelegate",0).cast<void>();
    auto type=BNM::Class("PvZCards.Engine.Dtos","Play");
    auto* targets=type.GetField("Targets").cast<BNM::Structures::Mono::List<int>*>()[play].Get();
    if(!targets || !targets->items || targets->size<0 || targets->size>int(targets->items->capacity)){fail[service](29);return true;}
    auto entry=Engine::HandMode::Decode(PlayInt(play,"Type"),PlayInt(play,"CardId"),PlayInt(play,"PlayerId"),type.GetField("OrderInTargetLocation").cast<int>()[play].Get(),targets->items->m_Items,targets->size);
    if(!entry){fail[service](29);return true;}
    Obj* instances=BNM::Class("PvZCards.Core.Services","LocalPlayCardService").GetField("<Instances>k__BackingField").cast<Obj*>()[service].Get();
    Obj* model=Engine::Match::Provide(instances,BNM::Class("PvZCards.Engine","EntityModel").GetMonoType());
    Obj* card=Engine::EntityAccess::Find(model,PlayInt(play,"CardId"));
    if(!card || Engine::BoardPlacement::Owner(card)!=PlayInt(play,"PlayerId") || !SetHandMode(model,card,*entry)){fail[service](25);return true;}
    static std::atomic<unsigned> samples{0};if(samples.fetch_add(1)<64)LOG_INFO("CustomComponents/Level/HAND_MODE: card=%d requested=%d selected=%d",PlayInt(play,"CardId"),*entry,SelectedEntry(card));
    success[service]();return true;
}
inline void RefreshHandModes(Obj* model){
    if(!model)return;
    std::vector<int> hands;{std::lock_guard<std::mutex> guard(handIdsLock);hands=handIds;}if(hands.empty())return;
    static auto modelType=BNM::Class("PvZCards.Engine","EntityModel");
    static auto existsMetadata=modelType.GetMethod("EntityExists",1).cast<bool>();static auto getMetadata=modelType.GetMethod("GetEntityById",1).cast<Obj*>();
    auto exists=existsMetadata;auto get=getMetadata;
    static auto cardsMetadata=BNM::Class("PvZCards.Engine.Components","Hand").GetField("CardIds").cast<BNM::Structures::Mono::List<int>*>();auto cardsField=cardsMetadata;
    for(int handId:hands){if(!exists[model](handId))continue;Obj* hand=Engine::EntityAccess::Component(get[model](handId),"Hand");if(!hand)continue;
        auto* cards=cardsField[hand].Get();if(!cards || !cards->items || cards->size<0 || cards->size>int(cards->items->capacity))continue;
        for(int i=0;i<cards->size;++i){int id=cards->items->m_Items[i];if(!exists[model](id))continue;Obj* card=get[model](id);
        auto* state=reinterpret_cast<Application*>(Engine::EntityAccess::Component(card,"LevelApplication"));if(!state || !state->InHandSelection)continue;
        const int entry=state->EntryIndex;auto definition=DefinitionFor(card);if(!definition || entry<0 || entry>=int(definition->entries.size())){Stats::Clear(card);continue;}
        auto quote=QuoteFor(model,card,entry);
        if(!quote.valid || ResetDynamic(definition->entries[entry],BaseCost(card),Available(model,Engine::BoardPlacement::Owner(card)))){Stats::Clear(card);continue;}
        if(state->CostBonus!=quote.total-Stats::WithoutLevel(Stats::Value(card,"SunCost","SunCostValue")) || state->AttackBonus!=quote.attack || state->HealthBonus!=quote.health)SetHandMode(model,card,entry);
        }
    }
}
inline void MarkDeparture(Obj* model,int card){if(model && Engine::EntityAccess::Component(Engine::EntityAccess::Find(model,card),"LevelApplication"))departures.push_back({Core::ModelRegistry::Identify(model,true),card});}
inline void After(const Engine::EffectGuards::Context& context) {
    // Refresh only the observed hands at an outer effect boundary. This never
    // scans the model or replays a buff effect; committed board values freeze.
    if(IsPlay(context.effect)) {
        auto choice=Plays::effects.Get(context.effect);
        if(choice.present && choice.entry>=0) {
            Obj* model=ModelFor(context.router);int id=EffectInt(context.effect,"TargetId");Obj* card=Engine::EntityAccess::Find(model,id);
            auto* state=reinterpret_cast<Application*>(Engine::EntityAccess::Component(card,"LevelApplication"));
            if(state){
                if(Engine::EntityAccess::Component(card,"LaneElement"))state->WasOnBoard=true;
                Obj* value=Stats::Value(card,"SunCost","SunCostValue");Stats::ClearValue(value,Engine::Match::Instances(card));Stats::Add(value,Engine::Match::Instances(card),state->CostBonus);
                if(state->WasOnBoard && !Engine::EntityAccess::Component(card,"LaneElement"))MarkDeparture(model,id);
            }
        }
    }
    if(!costStack.empty()){auto previous=costStack.back();costStack.pop_back();costComponent=previous.component;overrideCost=previous.total;}
    if(effectDepth)--effectDepth;if(effectDepth)return;RefreshHandModes(ModelFor(context.router));if(departures.empty())return;
    auto pending=std::move(departures);departures.clear();
    for(auto item:pending){Obj* model=Core::ModelRegistry::Get(item.model);Obj* card=Engine::EntityAccess::Find(model,item.card);auto* state=reinterpret_cast<Application*>(Engine::EntityAccess::Component(card,"LevelApplication"));if(state && state->WasOnBoard && !Engine::EntityAccess::Component(card,"LaneElement"))Stats::Clear(card);}
}
inline void LeaveHand(Obj* model,int id){Obj* card=Engine::EntityAccess::Find(model,id);auto* state=reinterpret_cast<Application*>(Engine::EntityAccess::Component(card,"LevelApplication"));if(state && state->InHandSelection)Stats::Clear(card);}
inline void ReenterHand(Obj* model,int id){Obj* card=Engine::EntityAccess::Find(model,id);if(Engine::EntityAccess::Component(card,"LevelApplication") && !Engine::EntityAccess::Component(card,"LaneElement"))Stats::Clear(card);}
inline bool InitializeRuntime() {
    QueryState::previewValue=PreviewValue;Engine::PlayServices::control=HandCommand;
    auto costType=BNM::Class("PvZCards.Core.Systems.Utilities","SunCostCalculator");auto sunType=BNM::Class("PvZCards.Core.Systems.Utilities","SunCountCalculator");
    costCalculator=std::make_shared<Core::ManagedRuntime::Root>(costType.CreateNewObjectParameters());sunCalculator=std::make_shared<Core::ManagedRuntime::Root>(sunType.CreateNewObjectParameters());
    auto validator=BNM::Class("PvZCards.Core.Utilities","SunValidator");
    return Engine::PlayServices::Initialize() && costCalculator->Get() && sunCalculator->Get() && Core::Hooks::Install(costType.GetMethod("CalculateSunCost",1),Cost,originalCost) && Core::Hooks::Install(validator.GetMethod("Validate",2),Validate,originalValidate) && Core::Hooks::Install(validator.GetMethod("CardIsMarkedIsFree",1),FreeCheck,originalFreeCheck) &&
        Engine::PlayValidation::before.Add(BindValidation) &&
        Engine::ZoneChanges::Initialize() && Engine::ZoneChanges::afterLaneRemove.Add(MarkDeparture) && Engine::ZoneChanges::afterDiscardAdd.Add(MarkDeparture) && Engine::ZoneChanges::beforeHandAdd.Add(ReenterHand) && Engine::ZoneChanges::beforeHandRemove.Add(LeaveHand) &&
        Engine::EffectGuards::rules.Add(Guard) && Engine::EffectGuards::before.Add(Before) && Engine::EffectGuards::RegisterPreparation(Prepare) && Engine::EffectGuards::after.Add(After) && Engine::PlayTransport::beforeSubmit.Add(BindOutgoing) && Engine::PlayTransport::beforeSerialize.Add(BindOutgoing);
}
}
