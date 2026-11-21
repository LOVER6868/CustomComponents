#pragma once
#include <chrono>
#include "history/DomainRecords.hpp"
#include <map>
#include <tuple>
#include <unordered_set>
#include "core/ModelRegistry.hpp"
#include "engine/SimulationChanges.hpp"
#include <BNM/Class.hpp>
#include <BNM/Field.hpp>
#include <BNM/Method.hpp>
#include <BNM/Exceptions.hpp>
#include "core/ManagedPool.hpp"
#include "core/ManagedQuery.hpp"
#include "effects/PlayFromDeckToLaneEffectDescriptor.hpp"
#include "effects/SubsetCreationExtensions.hpp"
#include "engine/MatchAccess.hpp"
#include "engine/EffectEventContext.hpp"
#include "effects/SubsetPlacementPolicy.hpp"
#include "log.h"

namespace CustomComponents::Zones::DeckLaneCreation {
using Obj=BNM::IL2CPP::Il2CppObject;
using Position=Effects::SubsetPlacement::Position;
using Settings=Effects::SubsetPlacement::Settings;
struct Options {
    bool pluckFailSafe=false,faceDown=false;
    Settings creation{};
    Obj* subsetQuery=nullptr; // Optional authored fallback; never used to pluck from real deck.
};
inline Options Read(Obj* descriptor) {
    if(!descriptor)return {};
    auto* value=reinterpret_cast<Components::PlayFromDeckToLaneEffectDescriptor*>(descriptor);
    return {value->PluckFailSafe,value->ForceFaceDown,
        {value->GrantTeamup,value->GrantAquatic,
         Effects::SubsetPlacement::Parse(value->CreationPosition?value->CreationPosition->str():std::string{})},
        value->SubsetQuery};
}
// Recursively prune ONLY the entity-based InDeckQuery. InDeckGuidQuery
// stays untouched, since its GUID membership constraint is meaningful for
 // template/database card creation. Query ownership is retained by the clone.
inline bool RemoveEntityDeckRestrictions(Obj* node,Obj* instances,int depth=0){
    if(!node || !node->klass || !node->klass->name || depth>32)return false;
    const std::string_view name=node->klass->name;
    if(name=="InDeckQuery")return false;
    if(name=="InDeckGuidQuery")return true;
    if(name=="CompositeAllQuery" || name=="CompositeAnyQuery"){
        static auto queries=BNM::Class("PvZCards.Engine.Queries","CompositeQuery").GetField("Queries").cast<BNM::Structures::Mono::List<Obj*>*>();
        auto* children=queries.IsValid()?queries[node].Get():nullptr;
        if(!children || !children->items || children->size<0 || children->size>int(children->items->capacity))return false;
        auto remove=BNM::Class(reinterpret_cast<Obj*>(children)).GetMethod("RemoveAt",1).cast<void>();
        if(!remove.IsValid())return false;
        for(int i=children->size-1;i>=0;--i){
            Obj* child=children->items->m_Items[i];
            if(!RemoveEntityDeckRestrictions(child,instances,depth+1)){
                remove[children](i);
                auto giveBack=Core::ManagedQuery::ReturnMethod();
                if(giveBack.IsValid())giveBack(child,instances);
            }
        }
        // An empty AND is unrestricted; an empty OR has no candidates.
        return name=="CompositeAllQuery" || children->size>0;
    }
    // Not(InDeckQuery) must be removed as a whole, not inverted to match none.
    // Other nested query holders are traversed, never just the outer layer.
    for(const char* fieldName:{"Query","EntityQuery","CompareEntityQuery","CardQuery","LaneQuery","AdditionalTargetQuery"}){
        auto field=BNM::Class(node).GetField(fieldName).cast<Obj*>();
        if(!field.IsValid())continue;
        Obj* child=field[node].Get();
        if(child && !RemoveEntityDeckRestrictions(child,instances,depth+1))return false;
    }
    return true;
}
inline Obj* BuildFallbackQuery(Obj* effectEntity,Obj* instances){
    Obj* filter=effectEntity?Engine::EntityAccess::Component(effectEntity,"PrimaryTargetFilter"):nullptr;
    static auto queryField=BNM::Class("PvZCards.Engine.Components","EntityFilter").GetField("Query").cast<Obj*>();
    Obj* authored=filter && queryField.IsValid()?queryField[filter].Get():nullptr;
    auto copy=Core::ManagedQuery::CopyMethod();
    if(!authored || !instances || !copy.IsValid())return nullptr;
    Obj* result=nullptr;
    auto error=BNM::TryInvoke([&]{result=copy(authored,instances);});
    if(error.IsValid() || !result)return nullptr;
    if(RemoveEntityDeckRestrictions(result,instances))return result;
    const bool onlyDeck=std::string_view(result->klass->name)=="InDeckQuery";
    auto giveBack=Core::ManagedQuery::ReturnMethod();
    if(giveBack.IsValid())giveBack(result,instances);
    if(onlyDeck)return Core::ManagedPool::Acquire(instances,
        BNM::Class("PvZCards.Engine.Queries","AlwaysMatchesQuery"));
    // Never broaden an unsatisfiable OR or Not clause to a wildcard.
    // An authored primary query consisting solely of InDeckQuery is the
    // sole valid root-level "no restriction" special case.
    return nullptr;
}
// Invoke the game's CreateCardSystem.ProcessEffect(CreateCardFromSubsetEffect)
inline int FirstLegalFallback(Obj* model,Obj* effectEntity,const std::vector<int>& laneIds,Obj* context=nullptr){
    const auto options=Read(Engine::EntityAccess::Component(effectEntity,"PlayFromDeckToLaneEffectDescriptor"));
    if(!model || !effectEntity || !options.pluckFailSafe || laneIds.empty())return -1;
    using Key=std::tuple<Core::ModelRegistry::Token,std::uint64_t,int,int,int,int>;
    static thread_local std::map<Key,bool> cache;
    auto contextType=BNM::Class("PvZCards.Engine","EntityQueryContext");
    auto getSource=contextType.GetMethod("get_SourceId",0).cast<int>();
    auto getTarget=contextType.GetMethod("get_TargetId",0).cast<int>();
    Key key{Core::ModelRegistry::Identify(model,true),Engine::SimulationChanges::Revision(model),
        Engine::Match::Id(effectEntity),-1,context && getSource.IsValid()?getSource[context]():-1,
        context && getTarget.IsValid()?getTarget[context]():-1};
    std::vector<std::pair<Key,Obj*>> pending;
    for(int laneId:laneIds){
        if(laneId<0)continue;
        auto laneKey=key;std::get<3>(laneKey)=laneId;
        if(auto found=cache.find(laneKey);found!=cache.end()){
            if(found->second)return laneId;
            continue;
        }
        if(auto* lane=Engine::EntityAccess::Find(model,laneId))pending.emplace_back(laneKey,lane);
    }
    if(pending.empty())return -1;
    Obj* instances=Engine::Match::Instances(effectEntity);
    auto copy=Core::ManagedQuery::CopyMethod();
    Obj* query=options.subsetQuery && copy.IsValid()?copy(options.subsetQuery,instances):BuildFallbackQuery(effectEntity,instances);
    Core::ManagedRuntime::Root queryRoot(query);
    if(!query || !queryRoot.Get())return -1;
    auto ownedContext=context?std::shared_ptr<Core::ManagedRuntime::Root>{}:Engine::BoardPlacement::Context(model,effectEntity);
    if(!context && ownedContext)context=ownedContext->Get();
    auto database=Engine::BoardPlacement::Database(effectEntity);
    auto find=BNM::Class("PvZCards.Core.Utilities","CardDatabase").GetMethod("FindEntities",2).cast<Obj*>();
    Obj* found=nullptr;int legalLane=-1;
    if(context && database && find.IsValid()){
        const auto started=std::chrono::steady_clock::now();
        auto error=BNM::TryInvoke([&]{found=find[database](query,context);});
        const auto queried=std::chrono::steady_clock::now();
        Core::ManagedRuntime::Root foundRoot(found);
        auto* cards=reinterpret_cast<Engine::EntityAccess::List*>(found);
        if(!error.IsValid() && Engine::EntityAccess::Valid(cards)){
            Engine::PlacementQueryContext::Scope scope({model,effectEntity});
            if(cache.size()>128)cache.clear();
            for(const auto& [laneKey,lane]:pending){
                bool legal=false;
                for(int i=0;i<cards->size&&!legal;++i)
                    legal=Engine::BoardPlacement::Fits(model,cards->items->m_Items[i],lane,context);
                cache.emplace(laneKey,legal);
                if(legal){legalLane=std::get<3>(laneKey);break;}
            }
        }
        const auto finished=std::chrono::steady_clock::now();
        const auto ms=[](auto a,auto b){return std::chrono::duration_cast<std::chrono::microseconds>(b-a).count();};
        if(ms(started,finished)>4000)LOG_INFO("CustomComponents/PlayFromDeck/PERF: lanes=%zu databaseUs=%lld placementUs=%lld candidates=%d legalLane=%d",
            pending.size(),static_cast<long long>(ms(started,queried)),static_cast<long long>(ms(queried,finished)),
            Engine::EntityAccess::Valid(cards)?cards->size:0,legalLane);
    }
    auto giveBack=Core::ManagedQuery::ReturnMethod();
    if(giveBack.IsValid())giveBack(query,instances);
    return legalLane;
}
inline bool HasLegalFallback(Obj* model,Obj* effectEntity,int laneId,Obj* context=nullptr){
    return FirstLegalFallback(model,effectEntity,{laneId},context)>=0;
}

// Invoke the game's CreateCardSystem.ProcessEffect(CreateCardFromSubsetEffect)
// rather than manually conjuring or modifying the physical source deck.
// This uses the SAME SubsetCreationExtensions effects-store as stock descriptor
// effects, so GrantTeamup/GrantAquatic/CreationPosition flow to the factory,
// placer and grant records without installing a competing hook.
inline bool RunStockFallback(Obj* sourceEffect,Obj* effectEntity,int laneId,const Options& options) {
    if(!sourceEffect || !effectEntity || laneId<0 || !options.pluckFailSafe)return false;
    auto* instances=Engine::Match::Instances(effectEntity);
    auto systemType=BNM::Class("PvZCards.Core.Systems","CreateCardSystem");
    auto runtimeType=BNM::Class("PvZCards.Engine.Effects","CreateCardFromSubsetEffect");
    auto baseType=BNM::Class("PvZCards.Engine","Effect");
    auto process=systemType.GetMethod("CreateCard",2).cast<void>();
    auto copyBase=baseType.GetMethod("DeepCopyFrom",2).cast<void>();
    auto target=baseType.GetField("TargetId").cast<int>();
    auto secondary=baseType.GetField("SecondaryTargetId").cast<int>();
    auto force=runtimeType.GetField("ForceFaceDown").cast<bool>();
    auto query=runtimeType.GetField("SubsetQuery").cast<Obj*>();
    if(!instances || !process.IsValid() || !copyBase.IsValid() || !target.IsValid() ||
       !secondary.IsValid() || !force.IsValid() || !query.IsValid())return false;
    auto service=Engine::Match::Service(instances,systemType);
    if(!service || !service->Get())return false;
    Obj* runtime=Core::ManagedPool::Acquire(instances,runtimeType);
    Core::ManagedRuntime::Root root(runtime);
    if(!runtime || !root.Get())return false;
    bool executed=false;
    // Main path never reads SubsetQuery. On pluck failure, use the explicitly
    // authored subset, if any. Otherwise clone the authored primary filter and
    // remove only entity-based InDeckQuery constraints recursively. Leave the
    // GUID membership version InDeckGuidQuery completely unchanged.
    Obj* fallback=nullptr;
    if(options.subsetQuery){
        auto copy=Core::ManagedQuery::CopyMethod();
        if(copy.IsValid()){
            auto copyError=BNM::TryInvoke([&]{fallback=copy(options.subsetQuery,instances);});
            if(copyError.IsValid())fallback=nullptr;
        }
    }else{
        fallback=BuildFallbackQuery(effectEntity,instances);
    }
    Core::ManagedRuntime::Root queryRoot(fallback);
    if(!fallback || !queryRoot.Get()){
        Core::ManagedPool::Return(instances,runtime);
        LOG_WARN("CustomComponents/PlayFromDeckToLane: fallback query unavailable source=%s",
            options.subsetQuery?"authored-SubsetQuery":"derived-PrimaryTargetFilter");
        return false;
    }
    // Do not mutate a pooled effect until its inherited context is copied.
    auto error=BNM::TryInvoke([&]{
        copyBase[runtime](sourceEffect,instances);
        target[runtime].Set(laneId);  // stock creation expects the LANE as primary target
        secondary[runtime].Set(-1);
        force[runtime].Set(options.faceDown);
        if(!Core::ManagedRuntime::SetReference(runtime,query,fallback))return;
        fallback=nullptr; // runtime now owns the query and returns it on pool release
        Effects::SubsetCreationExtensions::Save(Effects::SubsetCreationExtensions::effects,
            runtime,options.creation);
        // The ordinary subset finder evaluates its query in a definition-only
        // context. A deck failsafe needs the originating event context (Self,
        // owner and variables), just as its legality preflight does.
        Obj* model=Engine::Match::Provide(instances,BNM::Class("PvZCards.Engine","EntityModel").GetMonoType());
        auto toContext=baseType.GetMethod("ToEntityQueryContext",1).cast<Obj*>();
        auto find=BNM::Class("PvZCards.Core.Utilities","CardDatabase").GetMethod("FindEntities",2).cast<Obj*>();
        if(!model || !toContext.IsValid() || !find.IsValid())return;
        Obj* context=Engine::EffectEventContext::Acquire(sourceEffect,instances,effectEntity);Core::ManagedRuntime::Root contextRoot(context);
        if(!contextRoot.Get())return;
        struct ContextReturn {Obj* instances;Obj* context;~ContextReturn(){Core::ManagedPool::Return(instances,context);}} contextReturn{instances,context};
        Obj* database=Engine::BoardPlacement::Database(effectEntity);
        Obj* found=database?find[database](query[runtime].Get(),context):nullptr;
        Core::ManagedRuntime::Root foundRoot(found);
        auto* cards=reinterpret_cast<Engine::EntityAccess::List*>(found);
        if(!Engine::EntityAccess::Valid(cards))return;
        Engine::PlacementQueryContext::Scope placement({model,effectEntity});
        Obj* lane=Engine::EntityAccess::Find(model,laneId);
        std::vector<int> guids;
        for(int i=0;i<cards->size;++i){Obj* card=cards->items->m_Items[i];
            if(Engine::BoardPlacement::Fits(model,card,lane,context))guids.push_back(Engine::BoardPlacement::Guid(card));}
        const int selected=Engine::Match::Roll(effectEntity,int(guids.size()));
        if(selected<0){LOG_WARN("CustomComponents/PlayFromDeckToLane/FALLBACK_EMPTY: lane=%d candidates=%zu",laneId,guids.size());return;}
        auto api=Engine::EntityAccess::Api();auto* entities=api.entities[model].Get();
        std::unordered_set<int> previous;
        if(!Engine::EntityAccess::Valid(entities))return;
        for(int i=0;i<entities->size;++i)previous.insert(Engine::Match::Id(entities->items->m_Items[i]));
        Effects::SubsetCreationExtensions::CreationData data{guids[selected],options.faceDown,options.creation.teamup,
            options.creation.position==Effects::SubsetPlacement::Position::Front,0};
        LOG_INFO("CustomComponents/PlayFromDeckToLane/FALLBACK_EXECUTE: lane=%d guid=%d candidates=%zu",laneId,data.guid,guids.size());
        Obj* nativeHistory=Engine::Match::Provide(instances,BNM::Class("PvZCards.Engine","History").GetMonoType());
        const int stockBefore=nativeHistory?Engine::CustomRecordHistoryAnchor::Count(nativeHistory):-1;
        process[service->Get()](runtime,data);
        const bool stockRecorded=stockBefore>=0 && Engine::CustomRecordHistoryAnchor::Count(nativeHistory)>stockBefore;
        entities=api.entities[model].Get();
        if(Engine::EntityAccess::Valid(entities))for(int i=0;i<entities->size;++i){
            Obj* card=entities->items->m_Items[i];
            if(!previous.count(Engine::Match::Id(card)) && Engine::BoardPlacement::Guid(card)==data.guid &&
               Engine::EntityAccess::Component(card,"LaneElement")){
                executed=true;
                History::Record fact;fact.kind=History::Kind::ZoneTransfer;fact.visual=History::VisualPolicy::NoVisual;
                fact.entityId=Engine::Match::Id(card);
                static auto sourceId=BNM::Class("PvZCards.Engine","Effect").GetField("SourceId").cast<int>();
                if(sourceId.IsValid())fact.sourceId=Core::Metadata::Bind(sourceId,sourceEffect).Get();
                fact.effectId=Engine::Match::Id(effectEntity);fact.from=History::Origin::Unknown;fact.to=History::Origin::Lane;
                History::TransferPayload payload{fact.entityId,-1,laneId,-1,-1};
                static auto order=BNM::Class("PvZCards.Engine.Components","Lane").GetMethod("GetOrderInLane",1).cast<int>();
                Obj* laneComponent=Engine::EntityAccess::Component(lane,"Lane");
                if(laneComponent && order.IsValid())payload.order=Core::Metadata::Bind(order,laneComponent)(fact.entityId);
                payload.owner=Engine::BoardPlacement::Owner(card);payload.guid=data.guid;payload.created=true;payload.stockRecorded=stockRecorded;
                payload.faceDown=data.faceDown;payload.teamup=Engine::BoardPlacement::Active(card,"Teamup");payload.aquatic=Engine::BoardPlacement::Active(card,"Aquatic");
                fact.ownerId=payload.owner;fact.payload=std::move(payload);History::Writer::Commit(std::move(fact),model,instances);
                break;
            }
        }
    });
    Effects::SubsetCreationExtensions::Save(Effects::SubsetCreationExtensions::effects,runtime,{});
    // Returning the temporary to the stock InstanceManager invokes the stock
    // query-return lifecycle. No synthetic effect is left in the router.
    Core::ManagedPool::Return(instances,runtime);
    if(fallback){auto giveBack=Core::ManagedQuery::ReturnMethod();if(giveBack.IsValid())giveBack(fallback,instances);}
    if(error.IsValid())LOG_ERR("CustomComponents/PlayFromDeckToLane: stock subset fallback threw");
    LOG_INFO("CustomComponents/PlayFromDeckToLane/FALLBACK_RESULT: lane=%d completed=%d exception=%d",laneId,int(executed),int(error.IsValid()));
    return !error.IsValid() && executed;
}
}

