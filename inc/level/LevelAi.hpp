#pragma once
#include <atomic>
#include "level/LevelRuntime.hpp"
#include "engine/HandPlayability.hpp"
#include "core/Performance.hpp"

namespace CustomComponents::Level::Ai {
// Extend the stock play list; the stock scorer decides between the resulting
// modes. Every clone keeps the original target and placement information.
inline void Expand(Obj* facade,int player,Obj* raw){
    Core::Performance::Scope aiCost(Core::Performance::Area::AiCustomHook,"level-expand-valid-plays");
    auto modelField=BNM::Class("PvZCards.Core.Utilities","GameFacade").GetField("<EntityModel>k__BackingField").cast<Obj*>();
    Obj* model=facade && modelField.IsValid()?modelField[facade].Get():nullptr;
    auto* list=reinterpret_cast<BNM::Structures::Mono::List<Obj*>*>(raw);
    if(!model || !Engine::EntityAccess::Valid(list))return;
    auto clone=BNM::Class("PvZCards.Engine.Dtos","Play").GetMethod("Clone",1).cast<Obj*>();
    auto add=BNM::Class(raw).GetMethod("Add",1).cast<void>();
    if(!clone.IsValid() || !add.IsValid())return;
    const int count=list->size;int added=0;
    for(int i=0;i<count;++i){
        Obj* play=list->items->m_Items[i];
        if(!play || PlayInt(play,"Type")!=3 || PlayInt(play,"PlayerId")!=player)continue;
        Obj* card=Engine::EntityAccess::Find(model,PlayInt(play,"CardId"));
        auto definition=DefinitionFor(card);if(!definition)continue;
        Obj* instances=Engine::Match::Instances(card);if(!instances)continue;
        for(int entry=0;entry<int(definition->entries.size());++entry){
            Core::Performance::Scope quoteCost(Core::Performance::Area::LevelQuote,"ai-QuoteFor",Engine::Match::Id(card),entry);
            auto quote=QuoteFor(model,card,entry);if(!quote.valid || !quote.affordable)continue;
            Obj* copy=nullptr;
            {Core::Performance::Scope copyCost(Core::Performance::Area::AiModelCopy,"level-play-clone");copy=clone[play](instances);}
            Core::ManagedRuntime::Root root(copy);if(!copy || !root.Get())continue;
            Plays::choices.Set(copy,{true,true,entry});
            Plays::frontendChoices.Set(copy,false,false);
            add[raw](copy);++added;
        }
    }
    static std::atomic<unsigned> samples{0};
    if(samples.fetch_add(1)<80)LOG_INFO("CustomComponents/Level/AI_OPTIONS: model=%p player=%d stock=%d added=%d",model,player,count,added);
}
inline bool Initialize(){return Engine::HandPlayability::afterValidPlays.Add(Expand);}
}
