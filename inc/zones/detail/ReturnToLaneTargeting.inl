#include <unordered_map>
#include <optional>
#include <mutex>
// Included inside ZoneTransferSystem after its physical pair checker.
// Stock FindValidPrimaryTargets removes candidates via IsValidMove. A return
// starts in the discard pile, so that inner move test needs transfer legality.
// Keep stock query evaluation, secondary locations, scoping and pool ownership.
namespace Targeting {
using FindFn=Obj*(*)(Obj*,Obj*,Obj*,const void*);
using MoveFn=bool(*)(Obj*,int,int,Obj*,const void*);
inline FindFn originalPrimary=nullptr,originalSecondary=nullptr;
// Player-facing secondary targeting uses this DIFFERENT three-argument overload.
using SecondaryPairFn=Obj*(*)(Obj*,int,Obj*,Obj*,const void*);
inline SecondaryPairFn originalSecondaryPair=nullptr;
inline MoveFn originalMove=nullptr;
inline BNM::Field<Obj*> queryModel;
inline BNM::Field<Obj*> finderEntities,entitiesModel;
inline BNM::Field<int> ownerSource;
inline thread_local bool activeDeckSource=false;
// Scoped only while stock TargetEntityFinder searches one SwapPosition effect.
// Swap secondary targets are FIGHTERS, not lane IDs, and stock IsValidMove
// rejects every candidate when it interprets the fighter as a lane.
inline thread_local Obj* activeSwapModel=nullptr;
inline thread_local Obj* activeSwapEffect=nullptr;
inline thread_local Obj* activeCombatModel=nullptr;
// Keep only IDs and a weak model token across UI frames, never raw Entity
// pointers. Multiple selection controllers may share one finder object.
struct FighterStage {Core::ModelRegistry::Token token{};int effectId=-1;int sourceId=-1;int firstId=-1;bool swap=false;bool combat=false;};
inline std::mutex stageMutex;
inline std::unordered_map<Obj*,FighterStage> stageByFinder;
// The frontend and simulation may use different TargetEntityFinder objects.
// Key the selected fighter by the model token and actual effect entity, not
// by the finder pointer (which is merely an observer).
inline std::map<std::pair<Core::ModelRegistry::Token,int>,int> firstByEvent;
inline int FirstForEffect(Obj* model,int effect){
    if(!model || effect<0)return -1;
    std::lock_guard<std::mutex> guard(stageMutex);
    auto it=firstByEvent.find({Core::ModelRegistry::Identify(model,false),effect});
    return it==firstByEvent.end()?-1:it->second;
}
inline void RememberFirst(Obj* model,int effect,int first){
    if(!model || effect<0 || first<0)return;
    const auto token=Core::ModelRegistry::Identify(model,true);
    if(!token)return;
    std::lock_guard<std::mutex> guard(stageMutex);
    if(firstByEvent.size()>128)firstByEvent.clear();
    firstByEvent[{token,effect}]=first;
}

inline void Stage(Obj* finder,Obj* model,Obj* effect,bool swap,bool combat){
    if(!finder)return;
    std::lock_guard<std::mutex> lock(stageMutex);
    if(stageByFinder.size()>128)stageByFinder.clear();
    if(!model || !effect || (!swap && !combat)){stageByFinder.erase(finder);return;}
    Obj* identity=FindComponent(effect,"EffectEntity");
    stageByFinder[finder]={Core::ModelRegistry::Identify(model,true),Engine::Match::Id(effect),
        identity && ownerSource.IsValid()?ownerSource[identity].Get():-1,-1,swap,combat};
}
inline std::optional<FighterStage> GetStage(Obj* finder,Obj* model){
    std::lock_guard<std::mutex> lock(stageMutex);
    auto it=stageByFinder.find(finder);
    if(it==stageByFinder.end() || !model || it->second.token!=Core::ModelRegistry::Identify(model,false))return {};
    return it->second;
}
struct SwapScope {
    Obj* previous=activeSwapModel;
    Obj* previousEffect=activeSwapEffect;
    Obj* previousCombat=activeCombatModel;
    explicit SwapScope(Obj* effect,Obj* context) {
        const bool swap=FindComponent(effect,"SwapPositionEffectDescriptor")!=nullptr;
        const bool combat=FindComponent(effect,"CombatEffectDescriptor")!=nullptr;
        Obj* model=context && queryModel.IsValid()?queryModel[context].Get():nullptr;
        activeSwapModel=swap?model:nullptr;activeSwapEffect=swap?effect:nullptr;
        activeCombatModel=combat?model:nullptr;
    }
    ~SwapScope(){activeSwapEffect=previousEffect;activeSwapModel=previous;activeCombatModel=previousCombat;}
};
struct SourceScope {bool previous=activeDeckSource;explicit SourceScope(Obj* effect){activeDeckSource=FindComponent(effect,"PlayFromDeckToLaneEffectDescriptor")!=nullptr;}~SourceScope(){activeDeckSource=previous;}};
inline ReturnToLaneTargeting::Context Resolve(Obj* finder,Obj* effect,Obj* context) {
    if(!finder || !effect || !context || !queryModel.IsValid() || !ownerSource.IsValid() ||
       (!FindComponent(effect,"ReturnFromDiscardPileToLaneEffectDescriptor") && !FindComponent(effect,"PlayFromDeckToLaneEffectDescriptor"))) return {};
    Obj* model=queryModel[context].Get();
    auto finderField=finderEntities;
    auto modelField=entitiesModel;
    Obj* entities=finderField[finder].Get();
    if(!model || !entities || modelField[entities].Get()!=model ||
       FindEntity(model,entityId[effect].Get())!=effect)return {};
    Obj* owner=FindComponent(effect,"EffectEntity");
    Obj* source=model && owner?FindEntity(model,ownerSource[owner].Get()):nullptr;
    if(!source)return {};
    auto player=getPlayerIdFromCard;
    return {finder,model,player[static_cast<Obj*>(nullptr)](source),effect};
}
// The game's native card->lane move finder is not a complete physical-deck
// primary finder. Evaluate the authored filter and require a real deck-pool
// card that fits a lane. Prefer the event target's LastLane if supplied by
// the secondary filter, so no invalid primary is selected in the first place.
static Obj* ValidDeckPrimaries(Obj* finder,Obj* effect,Obj* context,Obj* stock){
    if(!finder || !effect || !context || !queryModel.IsValid())return stock;
    Obj* model=queryModel[context].Get();
    Obj* filter=FindComponent(effect,"PrimaryTargetFilter");
    auto qfield=BNM::Class("PvZCards.Engine.Components","EntityFilter").GetField("Query").cast<Obj*>();
    Obj* query=filter && qfield.IsValid()?qfield[filter].Get():nullptr;
    if(!model || !query)return stock;
    Obj* target=nullptr;
    static auto get=BNM::Class("PvZCards.Engine","EntityQueryContext").GetMethod("GetTargetEntity",0).cast<Obj*>();
    if(get.IsValid())target=get[context]();
    Obj* prev=target?FindComponent(target,"LastLane"):nullptr;
    static auto previousLane=BNM::Class("PvZCards.Engine.Components","LastLane").GetField("LaneId").cast<int>();
    Obj* secondary=FindComponent(effect,"SecondaryTargetFilter");
    Obj* secondaryQuery=secondary && qfield.IsValid()?qfield[secondary].Get():nullptr;
    const bool usesLastLane=secondaryQuery && secondaryQuery->klass && secondaryQuery->klass->name &&
        std::string_view(secondaryQuery->klass->name)=="LastLaneOfTargetQuery";
    const int requiredLane=usesLastLane && prev && previousLane.IsValid()?previousLane[prev].Get():-1;
    auto api = Engine::EntityAccess::Api();
    auto* entities=api.entities[model].Get();
    if(!Engine::EntityAccess::Valid(entities))return stock;
    std::vector<Obj*> lanes;
    for(int i=0;i<entities->size;++i){
        Obj* lane=entities->items->m_Items[i];
        if(FindComponent(lane,"Lane") && (requiredLane<0 || Engine::Match::Id(lane)==requiredLane))
            lanes.push_back(lane);
    }
    auto fits=[&](Obj* card){
        Engine::PlacementQueryContext::Scope scope({model,effect});
        for(Obj* lane:lanes)
            if(Engine::BoardPlacement::Fits(model,card,lane,nullptr))return true;
        return false;
    };
    auto valid=[&](int cardId){
        DeckLaneContext deck;
        return ResolveDeckLaneContext(model,cardId,deck) &&
            Engine::BoardPlacement::Matches(query,deck.card,context) && fits(deck.card);
    };
    auto validList=[](Obj* input){
        auto* list=reinterpret_cast<BNM::Structures::Mono::List<int>*>(input);
        return list && list->items && list->size>=0 && list->size<=int(list->items->capacity);
    };
    auto* existing=reinterpret_cast<BNM::Structures::Mono::List<int>*>(stock);
    int original=validList(stock)?existing->size:-1;
    if(validList(stock)){
        int count=0;for(int i=0;i<existing->size;++i)
            if(valid(existing->items->m_Items[i]))existing->items->m_Items[count++]=existing->items->m_Items[i];
        existing->size=count;
        if(count>0)return stock;
    }
    // FindTargets() only returns stock target-scope candidates. It cannot
    // discover a physical DeckElement that stock already excluded.
    std::vector<int> ids;
    int physical=0,matching=0,materializedDeck=0;
    for(int i=0;i<entities->size && i<20000;++i){
        Obj* card=entities->items->m_Items[i];
        if(!card || !FindComponent(card,"Card") || FindComponent(card,"LaneElement") ||
           FindComponent(card,"HandElement") || FindComponent(card,"DiscardPileElement"))continue;
        ++physical;
        const int id=Engine::Match::Id(card);
        DeckLaneContext membership;
        if(!ResolveDeckLaneContext(model,id,membership))continue;
        ++materializedDeck;
        bool matches=false;
        auto ex=BNM::TryInvoke([&]{matches=Engine::BoardPlacement::Matches(query,card,context);});
        if(ex.IsValid() || !matches)continue;
        ++matching;
        if(fits(membership.card))ids.push_back(id);
    }
    // A fail-safe cannot be reached if primary selection returns no target at
    // all. When the physical pool has no legal card, let the authored fallback
    // query select an existing card only as a routing placeholder. The move
    // system recognizes that it is not in the deck and invokes the stock
    // CreateCardSystem at effect execution time; prediction/target search does
    // not create cards.
    Obj* fallbackQuery=nullptr;
    auto* descriptor=FindComponent(effect,"PlayFromDeckToLaneEffectDescriptor");
    const auto options=CustomComponents::Zones::DeckLaneCreation::Read(descriptor);
    if(ids.empty() && options.pluckFailSafe){
        auto poolField=BNM::Class("PvZCards.Core.Systems.Utilities","TargetEntityFinder")
            .GetField("<Instances>k__BackingField").cast<Obj*>();
        Obj* instances=poolField.IsValid()?poolField[finder].Get():nullptr;
        if(instances && options.subsetQuery){
            auto copy=Core::ManagedQuery::CopyMethod();
            if(copy.IsValid()){
                auto copied=BNM::TryInvoke([&]{fallbackQuery=copy(options.subsetQuery,instances);});
                if(copied.IsValid())fallbackQuery=nullptr;
            }
        }else if(instances){
            fallbackQuery=CustomComponents::Zones::DeckLaneCreation::BuildFallbackQuery(effect,instances);
        }
        Core::ManagedRuntime::Root fallbackRoot(fallbackQuery);
        if(fallbackQuery && fallbackRoot.Get()){
            for(int i=0;i<entities->size && i<20000;++i){
                Obj* card=entities->items->m_Items[i];
                if(!card || !FindComponent(card,"Card") || FindComponent(card,"LaneElement"))continue;
                DeckLaneContext membership;
                if(ResolveDeckLaneContext(model,Engine::Match::Id(card),membership))continue;
                bool matches=false;
                auto checked=BNM::TryInvoke([&]{matches=Engine::BoardPlacement::Matches(fallbackQuery,card,context);});
                if(!checked.IsValid() && matches && fits(card))ids.push_back(Engine::Match::Id(card));
            }
            static std::atomic<unsigned> fallbackSamples{0};
            if(fallbackSamples.fetch_add(1)<48)
                LOG_INFO("CustomComponents/PlayFromDeckToLane/FALLBACK_TARGETS: candidates=%zu lane=%d explicitQuery=%d",
                    ids.size(),requiredLane,int(options.subsetQuery!=nullptr));
        }
        if(fallbackQuery){
            auto giveBack=Core::ManagedQuery::ReturnMethod();
            if(giveBack.IsValid())giveBack(fallbackQuery,instances);
        }
        // Creation needs no existing copy of the requested card. Use the
        // event target only as a routing ID after proving a database result
        // fits the requested lane; execution still goes through CreateCardSystem.
        if(ids.empty() && target && FindComponent(target,"Card")){
            std::vector<int> laneIds;laneIds.reserve(lanes.size());
            for(Obj* lane:lanes)laneIds.push_back(Engine::Match::Id(lane));
            const int lane=Zones::DeckLaneCreation::FirstLegalFallback(model,effect,laneIds,context);
            if(lane>=0){
                ids.push_back(Engine::Match::Id(target));
                LOG_INFO("CustomComponents/PlayFromDeckToLane/FALLBACK_ROUTE: placeholder=%d lane=%d databaseValidated=1",ids.back(),lane);
            }
        }
    }
    static std::atomic<unsigned> samples{0};if(samples.fetch_add(1)<100)
        LOG_INFO("CustomComponents/PlayFromDeckToLane/POOL_PRIMARY: stock=%d cardEntities=%d poolCards=%d queryMatches=%d legal=%zu lane=%d target=%d",
            original,physical,materializedDeck,matching,ids.size(),requiredLane,target?Engine::Match::Id(target):-1);
    if(ids.empty())return stock;
    static auto poolField=BNM::Class("PvZCards.Core.Systems.Utilities","TargetEntityFinder")
        .GetField("<Instances>k__BackingField").cast<Obj*>();
    Obj* instances=poolField.IsValid()?poolField[finder].Get():nullptr;
    if(!instances)return stock;
    auto pool=BNM::Class("PvZCards.Engine","PoolableList`1")
        .GetGeneric({BNM::Class("System","Int32").GetCompileTimeClass()});
    Obj* result=Core::ManagedPool::Acquire(instances,pool);
    if(!validList(result)){
        if(result)Core::ManagedPool::Return(instances,result);
        return stock;
    }
    auto clear=BNM::Class(result).GetMethod("Clear",0).cast<void>();
    auto add=BNM::Class(result).GetMethod("Add",1).cast<void>();
    if(!clear.IsValid() || !add.IsValid()){
        Core::ManagedPool::Return(instances,result);return stock;
    }
    auto ex=BNM::TryInvoke([&]{clear[result]();for(int id:ids)add[result](id);});
    if(ex.IsValid()){Core::ManagedPool::Return(instances,result);return stock;}
    if(stock)Core::ManagedPool::Return(instances,stock);
    return result;
}
// Manually targeted fighter-to-fighter effects cannot use the stock
// card-to-lane finder: FindValid*Targets treats the secondary ID as a lane.
// Lists are owned by TargetEntityFinder's InstanceManager, not by this hook.
inline bool FighterListBounds(Obj* raw) {
    auto* list=reinterpret_cast<BNM::Structures::Mono::List<int>*>(raw);
    return list && list->size>=0 && list->size<=4096 &&
        (!list->size || (list->items && list->size<=int(list->items->capacity)));
}
inline Obj* FighterResultList(Obj* finder,Obj* stock) {
    if(FighterListBounds(stock))return stock;
    static auto instancesField=BNM::Class("PvZCards.Core.Systems.Utilities","TargetEntityFinder")
        .GetField("<Instances>k__BackingField").cast<Obj*>();
    Obj* instances=finder && instancesField.IsValid()?instancesField[finder].Get():nullptr;
    if(!instances)return nullptr;
    auto pool=BNM::Class("PvZCards.Engine","PoolableList`1")
        .GetGeneric({BNM::Class("System","Int32").GetCompileTimeClass()});
    Obj* made=Core::ManagedPool::Acquire(instances,pool);
    if(made && !FighterListBounds(made)){
        Core::ManagedPool::Return(instances,made);return nullptr;
    }
    return made;
}
inline bool FillFighterList(Obj* result,const std::vector<int>& ids) {
    if(!FighterListBounds(result))return false;
    auto type=BNM::Class(result);
    auto clear=type.GetMethod("Clear",0).cast<void>();
    auto add=type.GetMethod("Add",1).cast<void>();
    if(!clear.IsValid() || !add.IsValid())return false;
    auto ex=BNM::TryInvoke([&]{
        clear[result]();
        for(int id:ids)add[result](id);
    });
    return !ex.IsValid();
}
inline Obj* FighterFilterQuery(Obj* effect,bool secondary) {
    Obj* filter=FindComponent(effect,secondary?"SecondaryTargetFilter":"PrimaryTargetFilter");
    static auto queryField=BNM::Class("PvZCards.Engine.Components","EntityFilter")
        .GetField("Query").cast<Obj*>();
    return filter && queryField.IsValid()?queryField[filter].Get():nullptr;
}
inline bool EligibleFighter(Obj* model,Obj* card,bool){return card && Engine::FighterPair::Live(model,Engine::Match::Id(card));}
inline std::vector<int> EnumerateFighters(Obj* model,Obj* query,Obj* context,bool swap,int first=-1,Obj* swapEffect=nullptr){
    std::vector<int> ids;
    auto api=Engine::EntityAccess::Api();
    auto* list=model && api.entities.IsValid()?api.entities[model].Get():nullptr;
    if(!Engine::EntityAccess::Valid(list) || !query || list->size>20000)return ids;
    Obj* selected=first>=0?FindEntity(model,first):nullptr;
    for(int i=0;i<list->size;++i){
        Obj* card=list->items->m_Items[i];int id=Engine::Match::Id(card);
        if(id<0 || id==first || !FindComponent(card,"LaneElement") ||
           !EligibleFighter(model,card,swap))continue;
        if(selected && swap && !CustomComponents::Effects::SwapPosition::CanExchange(model,first,id,swapEffect))continue;
        if(!Engine::BoardPlacement::Matches(query,card,context))continue;
        ids.push_back(id);
    }
    return ids;
}
static Obj* Primary(Obj* finder,Obj* effect,Obj* context,const void* method) {
    SourceScope source(effect);SwapScope swap(effect,context);
    ReturnToLaneTargeting::Scope scope(Resolve(finder,effect,context));
    Stage(finder,activeSwapModel?activeSwapModel:activeCombatModel,effect,
          activeSwapModel!=nullptr,activeCombatModel!=nullptr);
    Obj* stock=originalPrimary(finder,effect,context,method);
    if(activeDeckSource)return ValidDeckPrimaries(finder,effect,context,stock);
    const bool isSwap=activeSwapModel!=nullptr,isCombat=activeCombatModel!=nullptr;
    if(!isSwap && !isCombat)return stock;
    Obj* model=isSwap?activeSwapModel:activeCombatModel;
    Obj* query=FighterFilterQuery(effect,false);
    if(!model || !query)return stock;
    auto ids=EnumerateFighters(model,query,context,isSwap);
    if(isSwap && query->klass && std::string_view(query->klass->name)=="SelfQuery"){
        // SelfQuery means the authored source. Never substitute
        // an arbitrary Plant from the board for the current actor.
        auto staged=GetStage(finder,model);
        int self=staged?staged->sourceId:-1;
        Obj* actor=self>=0?FindEntity(model,self):nullptr;
        ids.clear();
        if(actor && EligibleFighter(model,actor,true) && FindComponent(actor,"LaneElement") &&
           Engine::BoardPlacement::Matches(query,actor,context))ids.push_back(self);
    }
    // The primary and secondary filters need not describe the same set.
    // Check each primary against its own possible partner, not primary count.
    if(isCombat || isSwap){
        Obj* secondQuery=FighterFilterQuery(effect,true);
        Obj* association=FindComponent(effect,"EffectEntity");
        const int actor=association && ownerSource.IsValid()?ownerSource[association].Get():-1;
        std::erase_if(ids,[&](int first){
            if(!secondQuery)return isSwap || !Engine::FighterPair::Legal(model,actor,first);
            Variables::QueryScope pairContext({model,actor,first,Engine::Match::Id(effect),nullptr,nullptr});
            return !pairContext.context || EnumerateFighters(model,secondQuery,pairContext.context,isSwap,first,effect).empty();
        });
    }
    if(ids.empty()){
        if(FighterListBounds(stock))FillFighterList(stock,{});
        return stock;
    }
    Obj* result=FighterResultList(finder,stock);
    if(!result || !FillFighterList(result,ids))return stock;
    static std::atomic<unsigned> samples{0};if(samples.fetch_add(1)<50)
        LOG_INFO("CustomComponents/Targeting/FIGHTER_PRIMARY: effect=%d candidates=%zu swap=%d combat=%d",
            entityId.IsValid()?entityId[effect].Get():-1,ids.size(),int(isSwap),int(isCombat));
    return result;
}
static Obj* Secondary(Obj* finder,Obj* effect,Obj* context,const void* method) {
    SourceScope source(effect);SwapScope swap(effect,context);
    ReturnToLaneTargeting::Scope scope(Resolve(finder,effect,context));
    Obj* stock=originalSecondary(finder,effect,context,method);
    const bool isSwap=activeSwapModel!=nullptr,isCombat=activeCombatModel!=nullptr;
    if(!isSwap && !isCombat)return stock;
    Obj* model=isSwap?activeSwapModel:activeCombatModel;
    const int first=Variables::ContextId(context,"get_TargetId");
    Obj* chosen=model?FindEntity(model,first):nullptr;
    if(!chosen || !FindComponent(chosen,"LaneElement") ||
       !EligibleFighter(model,chosen,isSwap)){
        if(FighterListBounds(stock))FillFighterList(stock,{});
        return stock;
    }
    Obj* query=FighterFilterQuery(effect,true);
    if(!query){if(FighterListBounds(stock))FillFighterList(stock,{});return stock;}
    const auto ids=EnumerateFighters(model,query,context,isSwap,first,effect);
    Obj* result=FighterResultList(finder,stock);
    if(!result || !FillFighterList(result,ids))return stock;
    static std::atomic<unsigned> samples{0};if(samples.fetch_add(1)<50)
        LOG_INFO("CustomComponents/Targeting/FIGHTER_SECONDARY: effect=%d first=%d candidates=%zu combat=%d",
            entityId.IsValid()?entityId[effect].Get():-1,first,ids.size(),int(isCombat));
    return result;
}
// The stock UI normally invokes FindValidSecondaryTargets(int,
// IEnumerable<Entity>,Validator); the previous two-argument hook could not
// observe this path. Enumerate the fighter query using the STAGED effect's
// source and the selected first fighter, not a lane ID.
static Obj* SecondaryPair(Obj* finder,int first,Obj* secondaryTargets,Obj* validator,const void* method){
    Obj* foundFinder=finder && finderEntities.IsValid()?finderEntities[finder].Get():nullptr;
    Obj* model=foundFinder && entitiesModel.IsValid()?entitiesModel[foundFinder].Get():nullptr;
    auto stage=GetStage(finder,model);
    if(!stage)return originalSecondaryPair(finder,first,secondaryTargets,validator,method);
    // Synchronous simulation calls have an explicit effect scope. UI calls
    // must prove the same live ChooseTargetBlockingEffect, not reuse old state.
    if(!activeSwapModel && !activeCombatModel){
        static auto effects=BNM::Class("PvZCards.Core.Systems.Utilities","TargetEntityFinder").GetField("<Effects>k__BackingField").cast<Obj*>();
        Obj* queue=effects.IsValid()?effects[finder].Get():nullptr;
        Obj* blocking=nullptr;
        if(queue){auto peek=BNM::Class(queue).GetMethod("Peek",0).cast<Obj*>();if(peek.IsValid())BNM::TryInvoke([&]{blocking=peek[queue]();});}
        static auto type=BNM::Class("PvZCards.Engine.Effects","ChooseTargetBlockingEffect");
        static auto definition=BNM::Class("PvZCards.Engine","Effect").GetField("EffectEntityId").cast<int>();
        if(!blocking || !type.IsValid() || !BNM::IsA(blocking,type) || !definition.IsValid() || definition[blocking].Get()!=stage->effectId)
            return originalSecondaryPair(finder,first,secondaryTargets,validator,method);
    }
    Obj* effect=FindEntity(model,stage->effectId);
    if(!effect || (stage->swap && !FindComponent(effect,"SwapPositionEffectDescriptor")) ||
       (stage->combat && !FindComponent(effect,"CombatEffectDescriptor"))){
        Stage(finder,model,effect,false,false);return originalSecondaryPair(finder,first,secondaryTargets,validator,method);
    }
    Obj* selected=FindEntity(model,first);
    if(!effect || !selected || !FindComponent(selected,"LaneElement") ||
       !EligibleFighter(model,selected,stage->swap))
        return originalSecondaryPair(finder,first,secondaryTargets,validator,method);
    Obj* query=FighterFilterQuery(effect,true);
    if(!query)return originalSecondaryPair(finder,first,secondaryTargets,validator,method);
    Variables::QueryScope qscope({model,stage->sourceId,first,stage->effectId,nullptr,nullptr});
    if(!qscope.context)return originalSecondaryPair(finder,first,secondaryTargets,validator,method);
    const auto ids=EnumerateFighters(model,query,qscope.context,stage->swap,first,effect);
    {std::lock_guard<std::mutex> guard(stageMutex);
     auto it=stageByFinder.find(finder);
     if(it!=stageByFinder.end() && it->second.token==stage->token && it->second.effectId==stage->effectId)
         it->second.firstId=first;}
    RememberFirst(model,stage->effectId,first);
    Obj* list=FighterResultList(finder,nullptr);
    if(!list || !FillFighterList(list,ids))
        return originalSecondaryPair(finder,first,secondaryTargets,validator,method);
    static std::atomic<unsigned> samples{0};
    if(samples.fetch_add(1)<85)
        LOG_INFO("CustomComponents/Targeting/FIGHTER_SECONDARY_PAIR: effect=%d first=%d candidates=%zu swap=%d combat=%d",
            stage->effectId,first,ids.size(),int(stage->swap),int(stage->combat));
    return list;
}
// Stock UI is driven by FindValidTargetsFromEffect(ChooseTargetBlockingEffect),
// not by the earlier FindValidSecondaryTargets setup callback. The returned
// list must contain fighter entity IDs for custom fighter-pair effects.
// Dispatch through the Pick3-owned hook rather than detouring it twice.
static Obj* FighterUiTargets(void* rawFinder,void* rawBlocking,const void*){
    Obj* finder=static_cast<Obj*>(rawFinder);
    Obj* blocking=static_cast<Obj*>(rawBlocking);
    if(!finder || !blocking || !finderEntities.IsValid() || !entitiesModel.IsValid())return nullptr;
    static auto blockerClass=BNM::Class("PvZCards.Engine.Effects","ChooseTargetBlockingEffect");
    static auto phaseField=blockerClass.GetField("Phase").cast<int>();
    static auto blockerEffect=BNM::Class("PvZCards.Engine","Effect").GetField("EffectEntityId").cast<int>();
    static auto blockerSource=BNM::Class("PvZCards.Engine","Effect").GetField("SourceId").cast<int>();
    static auto blockerTarget=BNM::Class("PvZCards.Engine","Effect").GetField("TargetId").cast<int>();
    if(!blockerClass.IsValid() || !BNM::IsA(blocking,blockerClass) || !phaseField.IsValid() ||
       !blockerEffect.IsValid() || !blockerSource.IsValid() || !blockerTarget.IsValid())return nullptr;
    const int phase=phaseField[blocking].Get();
    if(phase!=0&&phase!=1)return nullptr;
    Obj* entityFinder=finderEntities[finder].Get();
    Obj* model=entityFinder?entitiesModel[entityFinder].Get():nullptr;
    const int effectId=blockerEffect[blocking].Get();
    Obj* effect=model?FindEntity(model,effectId):nullptr;
    if(!effect)return nullptr;
    const bool swap=FindComponent(effect,"SwapPositionEffectDescriptor")!=nullptr;
    const bool combat=FindComponent(effect,"CombatEffectDescriptor")!=nullptr;
    if(!swap && !combat){Stage(finder,model,effect,false,false);return nullptr;}
    if(phase==0){
        const int source=blockerSource[blocking].Get();
        Variables::QueryScope context({model,source,blockerTarget[blocking].Get(),effectId,nullptr,nullptr});
        auto* query=FighterFilterQuery(effect,false);
        if(!context.context||!query)return nullptr;
        auto ids=EnumerateFighters(model,query,context.context,swap);
        auto* second=FighterFilterQuery(effect,true);
        std::erase_if(ids,[&](int first){
            if(!second)return swap||!Engine::FighterPair::Legal(model,source,first);
            Variables::QueryScope pair({model,source,first,effectId,nullptr,nullptr});
            return !pair.context||EnumerateFighters(model,second,pair.context,swap,first,effect).empty();
        });
        Stage(finder,model,effect,swap,combat);
        auto* result=FighterResultList(finder,nullptr);
        if(!result||!FillFighterList(result,ids))return nullptr;
        return result;
    }
    int first=blockerTarget[blocking].Get();
    Obj* selected=FindEntity(model,first);
    if(!selected || !FindComponent(selected,"LaneElement") ||
       !EligibleFighter(model,selected,swap)){
        // The primary target can live in the original query context while
        // ChooseTargetBlockingEffect.TargetId still has its initial value.
        auto previous=GetStage(finder,model);
        if(previous && previous->effectId==effectId && previous->firstId>=0){
            first=previous->firstId;selected=FindEntity(model,first);
        }
        // Stock UI service may use another finder instance from simulation.
        if(!selected || !FindComponent(selected,"LaneElement")){
            const int recorded=FirstForEffect(model,effectId);
            if(recorded>=0){first=recorded;selected=FindEntity(model,first);}
        }
        if((!selected || !FindComponent(selected,"LaneElement") ||
            !EligibleFighter(model,selected,swap)) && swap){
            first=blockerSource[blocking].Get();selected=FindEntity(model,first);
        }
    }
    if(!selected || !FindComponent(selected,"LaneElement") ||
       !EligibleFighter(model,selected,swap)){
        static std::atomic<unsigned> misses{0};
        if(misses.fetch_add(1)<75)
            LOG_INFO("CustomComponents/Targeting/FIGHTER_UI_MISS: phase=%d effect=%d source=%d target=%d first=%d staged=%d",
                phaseField[blocking].Get(),effectId,blockerSource[blocking].Get(),
                blockerTarget[blocking].Get(),first,GetStage(finder,model).has_value()?1:0);
        return nullptr;
    }
    Obj* query=FighterFilterQuery(effect,true);
    if(!query)return nullptr;
    const int source=blockerSource[blocking].Get();
    Variables::QueryScope qscope({model,source,first,effectId,nullptr,nullptr});
    if(!qscope.context)return nullptr;
    auto ids=EnumerateFighters(model,query,qscope.context,swap,first,effect);
    Obj* result=FighterResultList(finder,nullptr);
    if(!result || !FillFighterList(result,ids))return nullptr;
    static std::atomic<unsigned> logs{0};
    if(logs.fetch_add(1)<100)
        LOG_INFO("CustomComponents/Targeting/FIGHTER_UI_SECONDARY: effect=%d first=%d candidates=%zu swap=%d combat=%d",
             effectId,first,ids.size(),int(swap),int(combat));
    return result;
}
// The stock ShouldAddChooseTargetEffects(int,filter,candidates) assumes the
// second target is a lane. In particular, LaneContainsAllyFighter interprets
// the fighter entity ID as a lane and can throw a NullReferenceException.
// Only bypass this logic for our authored secondary fighter filters.
using CurrentTargetsFn=Obj*(*)(Obj*,const void*);
inline CurrentTargetsFn originalCurrentTargets=nullptr;
static Obj* CurrentTargets(Obj* finder,const void* method){
    static auto queueField=BNM::Class("PvZCards.Core.Systems.Utilities","TargetEntityFinder")
        .GetField("<Effects>k__BackingField").cast<Obj*>();
    Obj* queue=finder && queueField.IsValid()?queueField[finder].Get():nullptr;
    Obj* blocking=nullptr;
    if(queue){
        auto peek=BNM::Class(queue).GetMethod("Peek",0).cast<Obj*>();
        if(peek.IsValid()){
            auto ex=BNM::TryInvoke([&]{blocking=peek[queue]();});
            if(ex.IsValid())blocking=nullptr;
        }
    }
    static std::atomic<unsigned> samples{0};
    if(samples.fetch_add(1)<100)LOG_INFO("CustomComponents/Targeting/FIGHTER_CURRENT: finder=%p queue=%p top=%p type=%s",
         finder,queue,blocking,blocking && blocking->klass && blocking->klass->name?blocking->klass->name:"<null>");
    if(blocking){if(Obj* result=FighterUiTargets(finder,blocking,method))return result;}
    return originalCurrentTargets(finder,method);
}
static bool FighterShouldChooseSecondary(void* self,int first,void* rawFilter,void* rawCandidates,bool* handled){
    if(handled)*handled=false;
    Obj* filter=static_cast<Obj*>(rawFilter);
    Obj* chooser=static_cast<Obj*>(self);
    if(!filter || !chooser || !handled)return false;
    static auto modelField=BNM::Class("PvZCards.Core.Systems.Utilities","ChooseTargetEffectCreator")
        .GetField("<Model>k__BackingField").cast<Obj*>();
    Obj* model=modelField.IsValid()?modelField[chooser].Get():nullptr;
    Obj* selected=model?FindEntity(model,first):nullptr;
    if(!selected)return false;
    auto api=Engine::EntityAccess::Api();
    auto* entities=model && api.entities.IsValid()?api.entities[model].Get():nullptr;
    if(!Engine::EntityAccess::Valid(entities))return false;
    // Reuse this chooser's existing adapter: no second native hook. The stock
    // single-lane shortcut assumes the primary is already in play, whereas a
    // revive primary is in discard. Let stock assign the sole legal candidate.
    auto* lanes=reinterpret_cast<BNM::Structures::Mono::List<int>*>(rawCandidates);
    if(Engine::EntityAccess::Valid(lanes) && lanes->size==1){
        const int lane=lanes->items->m_Items[0];
        if(FindComponent(FindEntity(model,lane),"Lane")){
            for(int i=0;i<entities->size;++i){Obj* effect=entities->items->m_Items[i];
                if(!FindComponent(effect,"ReturnFromDiscardPileToLaneEffectDescriptor") || FindComponent(effect,"SecondaryTargetFilter")!=filter)continue;
                Obj* identity=FindComponent(effect,"EffectEntity");
                Obj* source=identity && ownerSource.IsValid()?FindEntity(model,Core::Metadata::Bind(ownerSource,identity).Get()):nullptr;
                const int player=source && getPlayerIdFromCard.IsValid()?Core::Metadata::Bind(getPlayerIdFromCard,static_cast<Obj*>(nullptr))(source):-1;
                if(player>=0 && CheckDiscardRevivePair(model,first,lane,player).legal){
                    *handled=true;
                    LOG_INFO("CustomComponents/Revive/AUTO_LANE: card=%d lane=%d legalCandidates=1",first,lane);
                    return false;
                }
            }
        }
    }
    if(!FindComponent(selected,"LaneElement"))return false;
    for(int i=0;i<entities->size && i<20000;++i){
        Obj* effect=entities->items->m_Items[i];
        if(!effect)continue;
        const bool swap=FindComponent(effect,"SwapPositionEffectDescriptor")!=nullptr;
        const bool combat=FindComponent(effect,"CombatEffectDescriptor")!=nullptr;
        if(!swap && !combat)continue;
        if(FindComponent(effect,"SecondaryTargetFilter")!=filter)continue;
        if(!EligibleFighter(model,selected,swap))continue;
        auto* list=reinterpret_cast<BNM::Structures::Mono::List<int>*>(rawCandidates);
        // This is a managed List<int> with live fighter IDs. The selector's
        // exact query and distinctness filtering is applied upstream.
        const bool valid=FighterListBounds(static_cast<Obj*>(rawCandidates));
        *handled=true;
        // The stock secondary candidate list may contain lanes, not fighters.
        // Re-evaluate the authored fighter filter against the selected fighter;
        // this also prevents a non-null-but-wrong stock list from concealing
        // a valid fighter pair.
        Obj* query=FighterFilterQuery(effect,true);
        Obj* association=FindComponent(effect,"EffectEntity");
        const int source=association && ownerSource.IsValid()?ownerSource[association].Get():-1;
        Variables::QueryScope scope({model,source,first,Engine::Match::Id(effect),nullptr,nullptr});
        const auto fighters=(scope.context && query)?EnumerateFighters(model,query,scope.context,swap,first,effect):std::vector<int>{};
        // Stock consumes this same list to assign the automatic target.
        // Replace stale lane candidates before returning the no-prompt result.
        const bool candidatesReady=valid && FillFighterList(static_cast<Obj*>(rawCandidates),fighters);
        const bool singletonReady=fighters.size()==1 && candidatesReady && list->size==1 &&
            list->items->m_Items[0]==fighters.front();
        const bool manualRequired=!fighters.empty() && !singletonReady;
        static std::atomic<unsigned> logs{0};
        if(logs.fetch_add(1)<85)
            LOG_INFO("CustomComponents/Targeting/FIGHTER_CHOOSE_SECONDARY: effect=%d first=%d stockList=%d stockValid=%d fighterCandidates=%zu manualRequired=%d",
                Engine::Match::Id(effect),first,valid?list->size:-1,int(valid),fighters.size(),int(manualRequired));
        return manualRequired;
    }
    return false;
}
static bool Move(Obj* finder,int card,int lane,Obj* validator,const void* method) {
    // Stock calls IsValidMove for card -> lane while enumerating PRIMARY
    // targets. Only treat the second ID as a fighter when it really is one.
    Obj* pairModel=activeSwapModel?activeSwapModel:activeCombatModel;
    if(pairModel){
        Obj* destination=FindEntity(pairModel,lane);
        if(destination && FindComponent(destination,"Lane"))
            return originalMove(finder,card,lane,validator,method);
        if(activeSwapModel)
            return CustomComponents::Effects::SwapPosition::CanExchange(pairModel,card,lane,activeSwapEffect);
        return Engine::FighterPair::Legal(pairModel,card,lane);
    }
    if(!ReturnToLaneTargeting::Owns(finder)){
        const bool legal=originalMove(finder,card,lane,validator,method);
        if(!legal){
            Obj* entityFinder=finder && finderEntities.IsValid()?Core::Metadata::Bind(finderEntities,finder).Get():nullptr;
            Obj* model=entityFinder?Core::Metadata::Bind(entitiesModel,entityFinder).Get():nullptr;
            static std::atomic<unsigned> traces{0};
            if(reinterpret_cast<std::uintptr_t>(model)==Core::SceneViewContext::ModelIdentity() && traces.fetch_add(1)<48)
                LOG_INFO("CustomComponents/Targeting/STOCK_MOVE_REJECT: card=%d lane=%d owner=%d cardExists=%d laneExists=%d",card,lane,Engine::BoardPlacement::Owner(FindEntity(model,card)),int(FindEntity(model,card)!=nullptr),int(FindEntity(model,lane)!=nullptr));
        }
        return legal;
    }
    const auto scope=ReturnToLaneTargeting::active;
    Obj* model=static_cast<Obj*>(scope.model);
    if(activeDeckSource)return CheckDeckLanePair(model,card,lane,scope.player,static_cast<Obj*>(scope.effect));
    const auto pair=CheckDiscardRevivePair(model,card,lane,scope.player);
    static thread_local unsigned samples=0;
    if(++samples<=96)LOG_INFO("CustomComponents/ReturnToLane/TARGET_PAIR: card=%d lane=%d player=%d legal=%d reason=%s source=discard-transfer stockQueries=preserved",
        card,lane,scope.player,int(pair.legal),pair.reason);
    return pair.legal;
}
inline bool Initialize() {
    auto type=BNM::Class("PvZCards.Core.Systems.Utilities","TargetEntityFinder");
    auto entity=BNM::Class("PvZCards.Engine","Entity");
    auto context=BNM::Class("PvZCards.Engine","EntityQueryContext");
    queryModel=context.GetField("<Model>k__BackingField").cast<Obj*>();
    finderEntities=type.GetField("<EntityFinder>k__BackingField").cast<Obj*>();
    entitiesModel=BNM::Class("PvZCards.Engine","EntityFinder").GetField("Model").cast<Obj*>();
    ownerSource=BNM::Class("PvZCards.Engine.Components","EffectEntity").GetField("SourceId").cast<int>();
    auto primary=type.GetMethod("FindValidPrimaryTargets",2);
    auto secondary=type.GetMethod("FindValidSecondaryTargets",{entity.GetCompileTimeClass(),context.GetCompileTimeClass()});
    auto intType=BNM::Class("System","Int32");
    // The runtime has two overloads; the gameplay UI usually calls this one.
    auto secondaryPair=type.GetMethod("FindValidSecondaryTargets",3);
    auto move=type.GetMethod("IsValidMove",3);
    auto current=type.GetMethod("FindValidTargetsFromCurrentEffect",0);
    if(!queryModel.IsValid() || !finderEntities.IsValid() || !entitiesModel.IsValid() ||
       !ownerSource.IsValid() || !entityId.IsValid() || !primary.IsValid() ||
       !secondary.IsValid() || !secondaryPair.IsValid() || !move.IsValid())return false;
    const bool ready=Core::Hooks::Install(move,Move,originalMove) &&
        Core::Hooks::Install(primary,Primary,originalPrimary) &&
        Core::Hooks::Install(secondary,Secondary,originalSecondary) &&
        Core::Hooks::Install(secondaryPair,SecondaryPair,originalSecondaryPair);
    // Shared frontend calls this zero-argument overload for the top of the
    // stock queue. The Pick3 bridge still owns FindValidTargetsFromEffect.
    const bool currentReady=current.IsValid() && Core::Hooks::Install(current,CurrentTargets,originalCurrentTargets);
    LOG_INFO("CustomComponents/ReturnToLane/TARGETING: ready=%d currentUi=%d primaryQuery=stock secondaryQuery=stock legality=discard-transfer",int(ready),int(currentReady));
    return ready;
}
}
