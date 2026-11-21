// Source-zone adapter for physical deck deployment. No mutation during queries.
struct DeckLaneContext {Obj* card=nullptr;Obj* pool=nullptr;int cardId=-1,playerId=-1,deckId=-1,index=-1;};
inline bool ResolveDeckLaneContext(Obj* model,int cardId,DeckLaneContext& out) {
    out={};Obj* card=FindEntity(model,cardId);if(!card || !FindComponent(card,"Card") || FindComponent(card,"LaneElement") || FindComponent(card,"HandElement") || FindComponent(card,"DiscardPileElement"))return false;
    // DeckElement is not authoritative: materialized cards can be represented
    // solely by CardPoolEntry.EntityId in the owning OrderedCardPool. The
    // entry check below is authoritative and excludes other decks. 
    const int player=getPlayerIdFromCard[static_cast<Obj*>(nullptr)](card);Obj* owner=FindEntity(model,player);Obj* association=FindComponent(owner,"DeckAssociation");if(!association)return false;
    const int deck=deckAssociationId[association].Get();Obj* pool=FindComponent(FindEntity(model,deck),"OrderedCardPool");if(!pool)return false;
    static auto entriesMetadata=BNM::Class("PvZCards.Engine.Components","OrderedCardPool").GetField("Entries").cast<MonoList<Obj*>*>();
    static auto idMetadata=BNM::Class("PvZCards.Engine.Components.Data","CardPoolEntry").GetField("EntityId").cast<int>();auto entriesField=entriesMetadata;auto id=idMetadata;
    auto* entries=entriesField[pool].Get();if(!Engine::EntityAccess::Valid(entries))return false;int found=-1;
    for(int i=0;i<entries->size;++i)if(entries->items->m_Items[i] && id[entries->items->m_Items[i]].Get()==cardId){if(found>=0)return false;found=i;}
    if(found<0)return false;out={card,pool,cardId,player,deck,found};return true;
}
inline bool CheckDeckLanePair(Obj* model,int card,int lane,int player,Obj* effectEntity=nullptr) {
    DeckLaneContext source;
    if(!ResolveDeckLaneContext(model,card,source)){
        Obj* candidate=FindEntity(model,card);
        if(!candidate || !FindComponent(candidate,"Card"))return false;
        Obj* owner=FindComponent(effectEntity,"EffectEntity");
        auto sourceId=BNM::Class("PvZCards.Engine.Components","EffectEntity").GetField("SourceId").cast<int>();
        Obj* caster=owner && sourceId.IsValid()?FindEntity(model,sourceId[owner].Get()):nullptr;
        return caster && getPlayerIdFromCard[static_cast<Obj*>(nullptr)](caster)==player &&
            Zones::DeckLaneCreation::HasLegalFallback(model,effectEntity,lane);
    }
    if(source.playerId!=player)return false;
    // No creation-grant scope: the physical card must already own its traits.
    Engine::PlacementQueryContext::Scope scope({model,effectEntity});
    return Engine::BoardPlacement::Fits(model,source.card,FindEntity(model,lane),nullptr);
}
// A stock random primary selection can pick a real deck card that cannot fit
// the requested lane. Before falling back to creating a NEW card, exhaust the
// physical deck's query-matching and legally placeable cards. Pure preflight:
// no deck mutation, no generated cards, no selection of out-of-query cards.
inline int FindPlaceableDeckSource(Obj* model,Obj* effectEntity,Obj* runtime,int laneId,int originalId){
    if(!model || !effectEntity || !runtime || laneId<0)return -1;
    auto playerField=BNM::Class("PvZCards.Engine","Effect").GetField("PlayerId").cast<int>();
    auto sourceField=BNM::Class("PvZCards.Engine","Effect").GetField("SourceId").cast<int>();
    const int sourceId=sourceField.IsValid()?sourceField[runtime].Get():-1;
    Obj* source=FindEntity(model,sourceId);
    int player=playerField.IsValid()?playerField[runtime].Get():-1;
    if(!FindComponent(FindEntity(model,player),"Player"))
        player=source?getPlayerIdFromCard[static_cast<Obj*>(nullptr)](source):-1;
    if(player<0 || !FindComponent(FindEntity(model,player),"Player"))return -1;
    Obj* primary=FindComponent(effectEntity,"PrimaryTargetFilter");
    static auto queryField=BNM::Class("PvZCards.Engine.Components","EntityFilter").GetField("Query").cast<Obj*>();
    Obj* query=primary && queryField.IsValid()?queryField[primary].Get():nullptr;
    if(!query)return -1; // No provable authored filter: don't broaden the draw.
    auto context=Engine::BoardPlacement::Context(model,effectEntity,originalId);
    if(!context || !context->Get())return -1;
    auto api=Engine::EntityAccess::Api();auto* entities=api.entities[model].Get();
    if(!Engine::EntityAccess::Valid(entities) || entities->size>10000)return -1;
    std::vector<int> candidates;
    for(int i=0;i<entities->size;++i){
        Obj* card=entities->items->m_Items[i];
        if(!card || !FindComponent(card,"Card") || FindComponent(card,"LaneElement") ||
           FindComponent(card,"HandElement") || FindComponent(card,"DiscardPileElement"))continue;
        const int id=Engine::Match::Id(card);
        if(!CheckDeckLanePair(model,id,laneId,player,effectEntity))continue;
        bool matches=false;
        const auto error=BNM::TryInvoke([&]{matches=Engine::BoardPlacement::Matches(query,card,context->Get());});
        if(!error.IsValid() && matches)candidates.push_back(id);
    }
    if(candidates.empty()){
        static std::atomic<unsigned> misses{0};
        if(misses.fetch_add(1)<48)
            LOG_INFO("CustomComponents/PlayFromDeckToLane/POOL_SOURCE_EMPTY: lane=%d player=%d original=%d cardEntities=%d",laneId,player,originalId,entities->size);
        return -1;
    }
    // Use match RNG where available to preserve random-deck semantics.
    int choice=source?Engine::Match::Roll(source,int(candidates.size())):-1;
    if(choice<0 || choice>=int(candidates.size()))choice=0;
    static std::atomic<unsigned> samples{0};if(samples.fetch_add(1)<32)
        LOG_INFO("CustomComponents/PlayFromDeckToLane/VIABLE_DECK: original=%d chosen=%d count=%zu lane=%d",
            originalId,candidates[choice],candidates.size(),laneId);
    return candidates[choice];
}
struct LaneTransferSource {Obj* card=nullptr;Obj* pool=nullptr;int playerId=-1,poolId=-1,index=-1;bool deck=false;};
inline bool ResolveLaneSource(Obj* model,int card,bool deck,LaneTransferSource& out) {
    if(deck){DeckLaneContext c;if(!ResolveDeckLaneContext(model,card,c))return false;out={c.card,c.pool,c.playerId,c.deckId,c.index,true};return true;}
    DiscardContext c;if(!ResolveDiscardContext(model,card,c))return false;out={c.card,c.discardPile,c.playerId,c.discardPileId,-1,false};return true;
}
inline bool SourceContains(Obj* model,const LaneTransferSource& source,int card) {
    if(!source.deck)return DiscardContains(source.pool,card);
    auto entriesField=BNM::Class("PvZCards.Engine.Components","OrderedCardPool").GetField("Entries").cast<MonoList<Obj*>*>();
    auto id=BNM::Class("PvZCards.Engine.Components.Data","CardPoolEntry").GetField("EntityId").cast<int>();
    auto* entries=source.pool?entriesField[source.pool].Get():nullptr;if(!Engine::EntityAccess::Valid(entries))return true; // unknown is not proven removal
    for(int i=0;i<entries->size;++i)if(entries->items->m_Items[i] && id[entries->items->m_Items[i]].Get()==card)return true;
    return false;
}
inline std::shared_ptr<Core::ManagedRuntime::Root> DeckLaneModifier(Obj* model,Obj* card) {
    auto lease=Engine::Match::Service(Engine::Match::Instances(card),BNM::Class("PvZCards.Core.Systems.Utilities","DeckModifier"));
    if(!lease || !lease->Get())return {};
    auto field=BNM::Class(lease->Get()).GetField("<Model>k__BackingField").cast<Obj*>();
    return field.IsValid() && field[lease->Get()].Get()==model?lease:nullptr;
}
inline bool RemoveDeckLaneSource(Obj* model,const LaneTransferSource& source,int card) {
    auto lease=DeckLaneModifier(model,source.card);if(!lease)return false;
    auto remove=BNM::Class(lease->Get()).GetMethod("RemoveCard",2).cast<void>();if(!remove.IsValid())return false;
    remove[lease->Get()](card,source.poolId);return true;
}
inline bool RestoreDeckLaneSource(Obj* model,const LaneTransferSource& source,int card) {
    if(FindComponent(source.card,"LaneElement") || FindComponent(source.card,"HandElement") || FindComponent(source.card,"DiscardPileElement"))return false;
    auto lease=DeckLaneModifier(model,source.card);if(!lease)return false;
    if(getPlayerIdFromCard[static_cast<Obj*>(nullptr)](source.card)!=source.playerId)return false;
    if(SourceContains(model,source,card)) {
        Engine::Match::Component(source.card,BNM::Class("PvZCards.Engine.Components","DeckElement").GetMonoType(),true);
    } else {
        if(FindComponent(source.card,"DeckElement")) {
            auto remove=BNM::Class("PvZCards.Engine","Entity").GetMethod("RemoveComponent",1).cast<Obj*>();
            remove[source.card](BNM::Class("PvZCards.Engine.Components","DeckElement").GetMonoType());
        }
        auto add=BNM::Class(lease->Get()).GetMethod("AddCardAt",3).cast<void>();if(!add.IsValid())return false;
        add[lease->Get()](source.card,source.pool,0);
    }
    DeckLaneContext restored;if(!ResolveDeckLaneContext(model,card,restored) || restored.deckId!=source.poolId)return false;
    auto field=BNM::Class("PvZCards.Engine.Components","OrderedCardPool").GetField("Entries").cast<MonoList<Obj*>*>();auto* entries=field[source.pool].Get();if(!Engine::EntityAccess::Valid(entries))return false;
    const int destination=std::clamp(source.index,0,entries->size-1);
    if(restored.index!=destination){Obj* entry=entries->items->m_Items[restored.index];Core::ManagedRuntime::Root root(entry);auto type=BNM::Class(reinterpret_cast<Obj*>(entries));auto remove=type.GetMethod("RemoveAt",1).cast<void>();auto insert=type.GetMethod("Insert",2).cast<void>();if(!remove.IsValid() || !insert.IsValid() || !root.Get())return false;remove[entries](restored.index);insert[entries](destination,entry);}
    return ResolveDeckLaneContext(model,card,restored) && restored.index==destination;
}
