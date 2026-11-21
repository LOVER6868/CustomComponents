#pragma once

#include <BNM/Class.hpp>
#include <BNM/Method.hpp>
#include "core/HookRegistry.hpp"
#include "core/Performance.hpp"
#include "engine/EntityAccess.hpp"
#include "engine/MatchAccess.hpp"
#include "queries/LaneQueries.hpp"
#include "log.h"
#include "variables/Storage.hpp"

namespace CustomComponents::CostModifierLifetime {
using Obj=BNM::IL2CPP::Il2CppObject;
using ObjList=BNM::Structures::Mono::List<Obj*>;
inline Obj* Model(Obj* system,const char* systemName,const char* field){
    auto f=BNM::Class("PvZCards.Core.Systems",systemName).GetField(field).cast<Obj*>();
    return system && f.IsValid()?f[system].Get():nullptr;
}
inline Obj* Target(Obj* model,Obj* effect){
    auto field=BNM::Class("PvZCards.Engine","Effect").GetField("TargetId").cast<int>();
    return model && effect && field.IsValid()?Engine::EntityAccess::Find(model,field[effect].Get()):nullptr;
}
inline void Clear(Obj* card,int duration){
    Obj* cost=Engine::EntityAccess::Component(card,"SunCost");
    if(!cost)return;
    static auto amountMetadata=BNM::Class("PvZCards.Engine.Components","SunCost").GetField("SunCostValue").cast<Obj*>();
    auto amountField=amountMetadata;
    Obj* amount=amountField.IsValid()?amountField[cost].Get():nullptr;
    if(!amount)return;
    static auto valueType=BNM::Class("PvZCards.Engine.Components.Data","ModifiableInt");
    static auto modsMetadata=valueType.GetField("Modifiers").cast<ObjList*>();
    auto mods=modsMetadata;
    ObjList* list=mods.IsValid()?mods[amount].Get():nullptr;
    if(!Engine::EntityAccess::Valid(list) || list->size==0)return;
    static auto removeMetadata=valueType.GetMethod("RemoveModifier",2).cast<void>();
    static auto durationMetadata=BNM::Class("PvZCards.Engine.Components.Data","IntModifier").GetField("Duration").cast<int>();
    auto remove=removeMetadata;auto durationField=durationMetadata;
    Obj* instances=Engine::Match::Instances(card);
    if(!remove.IsValid() || !durationField.IsValid() || !instances)return;
    for(int i=list->size-1;i>=0;--i){
        Obj* modifier=list->items->m_Items[i];
        if(modifier && (duration<0 || durationField[modifier].Get()==duration))
            remove[amount](modifier,instances);
    }
}
inline void ClearOnAllCards(Obj* model,int duration){
    Core::Performance::Scope timing(Core::Performance::Area::Maintenance,"cost-expiry-scan");
    auto api=Engine::EntityAccess::Api();
    ObjList* entities=model && api.entities.IsValid()?api.entities[model].Get():nullptr;
    if(!Engine::EntityAccess::Valid(entities))return;
    // Enumerate all card entities, not only cards currently on the board:
    // cost modifiers may have been applied to cards in hands and decks.
    for(int i=0;i<entities->size;++i){
        Obj* card=entities->items->m_Items[i];
        if(Engine::EntityAccess::Component(card,"Card"))Clear(card,duration);
    }
}
// Disables only the stock on-play cost reset. Other PlayCardEffect
// handlers, cost payment and validation still run through the router.
inline void(*originalResetOnPlay)(Obj*,Obj*,const void*)=nullptr;
inline void NoResetOnPlay(Obj*,Obj*,const void*){}
inline void(*originalBounce)(Obj*,Obj*,const void*)=nullptr;
inline void OnBounce(Obj* self,Obj* effect,const void* method){
    Obj* card=Target(Model(self,"ReturnToHandFromPlaySystem","<Model>k__BackingField"),effect);
    if(card && Queries::LaneTopology::LaneEntity(Model(self,"ReturnToHandFromPlaySystem","<Model>k__BackingField"),card))Clear(card,-1);
    originalBounce(self,effect,method);
}
inline void(*originalDiscard)(Obj*,Obj*,const void*)=nullptr;
inline void OnDiscard(Obj* self,Obj* effect,const void* method){
    Obj* card=Target(Model(self,"DiscardFromPlaySystem","<Model>k__BackingField"),effect);
    if(card && Queries::LaneTopology::LaneEntity(Model(self,"DiscardFromPlaySystem","<Model>k__BackingField"),card))Clear(card,-1);
    originalDiscard(self,effect,method);
}
inline void(*originalTurnEnd)(Obj*,Obj*,const void*)=nullptr;
inline void OnTurnEnd(Obj* self,Obj* effect,const void* method){
    {Core::Performance::Scope timing(Core::Performance::Area::Maintenance,"native-turn-buff-removal");originalTurnEnd(self,effect,method);}
    // The stock equivalent removes Attack and Health modifiers, but omits SunCost.
    // Use the effect's InstanceManager to resolve the same live model.
    auto field=BNM::Class("PvZCards.Core.Systems","EndOfTurnBuffRemovalSystem").GetField("<EntityFinder>k__BackingField").cast<Obj*>();
    Obj* finder=field.IsValid()?field[self].Get():nullptr;
    auto modelField=BNM::Class("PvZCards.Engine","EntityFinder").GetField("Model").cast<Obj*>();
    Obj* model=finder && modelField.IsValid()?modelField[finder].Get():nullptr;
    if(model){ClearOnAllCards(model,1);Variables::ExpireTimed(model,1);} // EffectDuration.EndOfTurn
}
inline void(*originalPlayFighter)(Obj*,Obj*,const void*)=nullptr;
inline void OnPlayFighter(Obj* self,Obj* effect,const void* method){
    Core::Performance::Scope timing(Core::Performance::Area::Maintenance,"fighter-play-handler");
    Obj* model=Model(self,"PlayFighterToLanesSystem","<Model>k__BackingField");
    Obj* card=Target(model,effect);
    const bool alreadyOnBoard=card && Queries::LaneTopology::LaneEntity(model,card);
    originalPlayFighter(self,effect,method);
    // NextFighter expires only after a NEW fighter entered play.
    if(card && !alreadyOnBoard && Queries::LaneTopology::LaneEntity(model,card))
        {ClearOnAllCards(model,2);Variables::ExpireTimed(model,2);}
}
inline bool Initialize(){
    using namespace CustomComponents::Core;
    return Hooks::Install(BNM::Class("PvZCards.Core.Systems","ResetSunCostOnPlaySystem").GetMethod("ProcessEffect",1),NoResetOnPlay,originalResetOnPlay) &&
        Hooks::Install(BNM::Class("PvZCards.Core.Systems","ReturnToHandFromPlaySystem").GetMethod("ProcessEffect",1),OnBounce,originalBounce) &&
        Hooks::Install(BNM::Class("PvZCards.Core.Systems","DiscardFromPlaySystem").GetMethod("ProcessEffect",1),OnDiscard,originalDiscard) &&
        Hooks::Install(BNM::Class("PvZCards.Core.Systems","EndOfTurnBuffRemovalSystem").GetMethod("ProcessEffect",1),OnTurnEnd,originalTurnEnd) &&
        Hooks::Install(BNM::Class("PvZCards.Core.Systems","PlayFighterToLanesSystem").GetMethod("ProcessEffect",1),OnPlayFighter,originalPlayFighter);
}
}
