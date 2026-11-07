// Source-zone adapter for physical deck deployment. No mutation during queries.
struct DeckLaneContext {Obj* card=nullptr;Obj* pool=nullptr;int cardId=-1,playerId=-1,deckId=-1,index=-1;};
inline bool ResolveDeckLaneContext(Obj* model,int cardId,DeckLaneContext& out) {
    out={};Obj* card=FindEntity(model,cardId);if(!card || !FindComponent(card,"DeckElement") || FindComponent(card,"LaneElement") || FindComponent(card,"HandElement") || FindComponent(card,"DiscardPileElement"))return false;
    const int player=getPlayerIdFromCard[static_cast<Obj*>(nullptr)](card);Obj* owner=FindEntity(model,player);Obj* association=FindComponent(owner,"DeckAssociation");if(!association)return false;
    const int deck=deckAssociationId[association].Get();Obj* pool=FindComponent(FindEntity(model,deck),"OrderedCardPool");if(!pool)return false;
    static auto entriesMetadata=BNM::Class("PvZCards.Engine.Components","OrderedCardPool").GetField("Entries").cast<MonoList<Obj*>*>();
    static auto idMetadata=BNM::Class("PvZCards.Engine.Components.Data","CardPoolEntry").GetField("EntityId").cast<int>();auto entriesField=entriesMetadata;auto id=idMetadata;
    auto* entries=entriesField[pool].Get();if(!Engine::EntityAccess::Valid(entries))return false;int found=-1;
    for(int i=0;i<entries->size;++i)if(entries->items->m_Items[i] && id[entries->items->m_Items[i]].Get()==cardId){if(found>=0)return false;found=i;}
    if(found<0)return false;out={card,pool,cardId,player,deck,found};return true;
}
inline bool CheckDeckLanePair(Obj* model,int card,int lane,int player) {
    DeckLaneContext source;if(!ResolveDeckLaneContext(model,card,source) || source.playerId!=player)return false;
    // No creation-grant scope: the physical card must already own its traits.
    Engine::PlacementQueryContext::Scope scope({model,nullptr});
    return Engine::BoardPlacement::Fits(model,source.card,FindEntity(model,lane),nullptr);
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
