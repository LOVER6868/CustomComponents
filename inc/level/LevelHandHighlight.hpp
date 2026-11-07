#pragma once
#include "level/LevelRuntime.hpp"
#include "engine/HandPlayability.hpp"
namespace CustomComponents::Level::HandHighlight {
struct State {bool known=false,stock=false,applied=false;int card=-1;Core::ModelRegistry::Token model=0;};
inline Core::WeakObjectSettings<State> states;
inline void(*originalSetPlayable)(Obj*,bool,const void*)=nullptr;
inline int CardId(Obj* view){static auto metadata=BNM::Class("PvZCards.Game","CardViewComponent").GetField("CardId").cast<int>();auto field=metadata;return view && field.IsValid()?field[view].Get():-1;}
inline bool Allows(Obj* model,int card) {
    if(!model || !isFrontend || !isFrontend() || !isLocalCard || !isLocalCard(model,card) || !localEntry)return true;
    if(auto native=Engine::HandPlayability::Playable(card);native && !*native)return false;
    Obj* entity=Engine::EntityAccess::Find(model,card);if(!DefinitionFor(entity))return true;
    const int entry=localEntry(model,card);if(entry<0)return true;
    auto quote=QuoteFor(model,entity,entry);return quote.valid && quote.affordable;
}
static void SetPlayable(Obj* view,bool stock,const void* method) {
    Obj* model=renderModel?renderModel():nullptr;
    const int card=CardId(view);
    if(!model || !DefinitionFor(Engine::EntityAccess::Find(model,card))){originalSetPlayable(view,stock,method);return;}
    const auto native=Engine::HandPlayability::Playable(card);
    const bool applied=native.value_or(stock) && Allows(model,card);
    states.Set(view,{true,stock,applied,card,Core::ModelRegistry::Identify(model,true)});
    originalSetPlayable(view,applied,method);
}
inline void Present(Obj* model,Obj* view,int card,int entry,const Quote& quote) {
    auto state=states.Get(view);
    const auto token=Core::ModelRegistry::Identify(model);
    if(!state.known || state.card!=card || state.model!=token){
        // Initial highlight can precede scene/model capture. Before our first
        // override, the overlay still contains the stock decision.
        static auto overlayMetadata=BNM::Class("PvZCards.Game","CardViewComponent").GetField("PlayableOverlay").cast<Obj*>();
        static auto activeMetadata=BNM::Class("UnityEngine","GameObject").GetMethod("get_activeSelf",0).cast<bool>();
        auto overlay=overlayMetadata;auto active=activeMetadata;
        Obj* object=view && overlay.IsValid()?overlay[view].Get():nullptr;
        if(!object || !active.IsValid())return;
        const bool stock=active[object]();state={true,stock,stock,card,token};states.Set(view,state);
    }
    const bool applied=CanHighlight(Engine::HandPlayability::Playable(card).value_or(state.stock),entry,quote);
    if(applied==state.applied)return;
    state.applied=applied;states.Set(view,state);originalSetPlayable(view,applied,nullptr);
}
inline bool Initialize(){return Core::Hooks::Install(BNM::Class("PvZCards.Game","CardViewComponent").GetMethod("SetPlayable",1),SetPlayable,originalSetPlayable);}
}
