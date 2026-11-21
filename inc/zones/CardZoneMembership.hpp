#pragma once
#include "engine/BoardPlacement.hpp"
namespace CustomComponents::Zones::Membership {
inline bool InDeck(Engine::Match::Obj* model,int id){
    using Obj=Engine::Match::Obj;
    Obj* card=Engine::EntityAccess::Find(model,id);if(!card)return false;
    if(Engine::EntityAccess::Component(card,"HandElement") || Engine::EntityAccess::Component(card,"LaneElement") || Engine::EntityAccess::Component(card,"DiscardPileElement"))return false;
    Obj* owner=Engine::EntityAccess::Find(model,Engine::BoardPlacement::Owner(card));
    Obj* association=Engine::EntityAccess::Component(owner,"DeckAssociation");
    static auto deck=BNM::Class("PvZCards.Engine.Components","DeckAssociation").GetField("DeckEntityId").cast<int>();
    static auto entries=BNM::Class("PvZCards.Engine.Components","OrderedCardPool").GetField("Entries").cast<BNM::Structures::Mono::List<Obj*>*>();
    static auto entityId=BNM::Class("PvZCards.Engine.Components.Data","CardPoolEntry").GetField("EntityId").cast<int>();
    if(!association || !deck.IsValid() || !entries.IsValid() || !entityId.IsValid())return false;
    Obj* pool=Engine::EntityAccess::Component(Engine::EntityAccess::Find(model,Core::Metadata::Bind(deck,association).Get()),"OrderedCardPool");
    auto* list=pool?Core::Metadata::Bind(entries,pool).Get():nullptr;if(!Engine::EntityAccess::Valid(list))return false;
    for(int i=0;i<list->size;++i){Obj* row=list->items->m_Items[i];if(row && Core::Metadata::Bind(entityId,row).Get()==id)return true;}
    return false;
}
}
