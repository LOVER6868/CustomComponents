#pragma once
#include "level/LevelRuntime.hpp"
#include "engine/HandPlayability.hpp"
#include "presentation/CardReadiness.hpp"
#include "core/Performance.hpp"
namespace CustomComponents::Level::HandHighlight {
struct State {bool known=false,stock=false,applied=false;int card=-1;Core::ModelRegistry::Token model=0;};
inline Core::WeakObjectSettings<State> states;
inline int CardId(Obj* view){static auto metadata=BNM::Class("PvZCards.Game","CardViewComponent").GetField("CardId").cast<int>();auto field=metadata;return view && field.IsValid()?field[view].Get():-1;}
inline bool Allows(Obj* model,int card,const std::optional<bool>& native) {
    if(!model || !isFrontend || !isFrontend() || !isLocalCard || !isLocalCard(model,card) || !localEntry)return true;
    Obj* entity=Engine::EntityAccess::Find(model,card);if(!DefinitionFor(entity))return true;
    if(!Engine::EntityAccess::Component(entity,"HandElement"))return true;
    if(native && !*native)return false;
    const int entry=localEntry(model,card);if(entry<0)return true;
    Core::Performance::Scope quoteCost(Core::Performance::Area::LevelQuote,"highlight-QuoteFor",card);
    auto quote=QuoteFor(model,entity,entry);return quote.valid && quote.affordable;
}
inline bool Allows(Obj* model,int card){return Allows(model,card,Engine::HandPlayability::Playable(card));}
static void RefinePlayable(Obj* view,bool* requested) {
    if(!requested)return;
    Core::Performance::Scope timing(Core::Performance::Area::LevelHighlight,"refine-playable");
    const bool stock=*requested;
    Obj* model=renderModel?renderModel():nullptr;
    const int card=CardId(view);
    if(!model || !DefinitionFor(Engine::EntityAccess::Find(model,card)))return;
    const auto native=Engine::HandPlayability::Playable(card);
    const bool applied=native.value_or(stock) && Allows(model,card,native);
    if(DefinitionFor(Engine::EntityAccess::Find(model,card))){static std::atomic<unsigned> samples{0};if(samples.fetch_add(1)<64)
        LOG_INFO("CustomComponents/Level/HIGHLIGHT: card=%d stock=%d nativeKnown=%d native=%d applied=%d",card,int(stock),int(native.has_value()),int(native.value_or(false)),int(applied));}
    states.Set(view,{true,stock,applied,card,Core::ModelRegistry::Identify(model,true)});
    *requested=applied;
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
    Core::Performance::Scope timing(Core::Performance::Area::LevelHighlight,"present-playability",card);
    const auto native=Engine::HandPlayability::Playable(card);
    const bool applied=CanHighlight(native.value_or(state.stock),entry,quote);
    if(entry>=0){static std::atomic<unsigned> samples{0};if(samples.fetch_add(1)<64)
        LOG_INFO("CustomComponents/Level/PRESENT: card=%d entry=%d nativeKnown=%d native=%d stock=%d quoteValid=%d affordable=%d applied=%d",card,entry,int(native.has_value()),int(native.value_or(false)),int(state.stock),int(quote.valid),int(quote.affordable),int(applied));}
    if(applied==state.applied)return;
    state.applied=applied;states.Set(view,state);Presentation::CardReadiness::Playable(view,applied,nullptr);
}
inline bool Initialize(){return Presentation::CardReadiness::modifiers.Add(RefinePlayable);}
}
