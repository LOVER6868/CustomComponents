#pragma once
#include "variables/Resolver.hpp"
#include "abilities/SelfTeleport.hpp"
namespace CustomComponents::Variables::Reactive {
using Desired=std::map<int,std::vector<int>>;
inline thread_local bool reconciling=false;
inline thread_local int calculating=0;
inline Desired Grants(Obj* model){
    if(!model || calculating)return {};auto& index=ModelIndex(model);if(!Engine::SimulationChanges::depth && index.desired)return *index.desired;struct Guard{Guard(){++calculating;}~Guard(){--calculating;}}guard;Desired desired;auto effects=ModelIndex(model).grants;
    for(int id:effects){Obj* entity=Find(model,id);Obj* descriptor=Engine::EntityAccess::Component(entity,"GrantAbilityEffectDescriptor");if(Field(descriptor,"GrantableAbilityType")!=Abilities::SelfTeleport::Id)continue;
        Context c{model,Source(entity),-1,id};Obj* filter=Engine::EntityAccess::Component(entity,"PrimaryTargetFilter");if(!BindingTargets(filter))continue;
        auto query=BNM::Class("PvZCards.Engine.Components","EntityFilter").GetField("Query").cast<Obj*>();QueryScope scope(c);auto targets=scope.Find(query[filter].Get());
        for(Obj* target:targets){if(!Engine::EntityAccess::Component(target,"Card"))continue;int targetId=Engine::Match::Id(target);c.target=targetId;if(Conditions(c,entity,target,"Hand,Board"))desired[targetId].push_back(id);}
    }if(!Engine::SimulationChanges::depth)ModelIndex(model).desired=desired;return desired;
}
inline bool Active(Obj* model,Obj* card){if(Abilities::SelfTeleport::Direct(card))return true;auto desired=Grants(model);auto found=desired.find(Engine::Match::Id(card));return found!=desired.end() && !found->second.empty();}
inline void Reconcile(Obj* model){
    if(!model || reconciling)return;struct Guard{Guard(){reconciling=true;}~Guard(){reconciling=false;}}guard;
    auto desired=Grants(model);auto& index=ModelIndex(model);auto cards=index.grantedTargets;
    if(!index.grantsInspected){cards=index.cards;index.grantsInspected=true;}
    index.grantedTargets.clear();for(auto& [id,sources]:desired){cards.push_back(id);index.grantedTargets.push_back(id);}std::sort(cards.begin(),cards.end());cards.erase(std::unique(cards.begin(),cards.end()),cards.end());auto counterField=GrantableAbilityRuntime::countersField;auto entriesField=GrantableAbilityRuntime::entriesField;
    for(int id:cards){Obj* card=Find(model,id);auto wanted=desired.find(id);auto* state=static_cast<Obj*>(Abilities::SelfTeleport::Get(card,wanted!=desired.end()));if(!state)continue;
        Obj* group=static_cast<Obj*>(counterField[state].Get());auto* entries=group?static_cast<BNM::Structures::Mono::List<GrantableAbilityRuntime::CounterEntry>*>(entriesField[group].Get()):nullptr;
        std::vector<int> existing;if(entries && entries->items && entries->size>=0 && entries->size<=int(entries->items->capacity))for(int n=0;n<entries->size;++n)if(entries->items->m_Items[n].sourceId<=-2)existing.push_back(-entries->items->m_Items[n].sourceId-2);
        for(int source:existing)if(wanted==desired.end() || std::find(wanted->second.begin(),wanted->second.end(),source)==wanted->second.end()){auto remove=GrantableAbilityRuntime::removeCounter;remove[state](0,-source-2);}
        if(wanted!=desired.end())for(int source:wanted->second)if(source<=INT32_MAX-2 && std::find(existing.begin(),existing.end(),source)==existing.end()){auto add=GrantableAbilityRuntime::addCounter;add[state](0,-source-2,1);}
    }
}
inline bool Prepare(const Engine::EffectGuards::Context& c){
    if(!c.effect || !c.effect->klass || std::string_view(c.effect->klass->name)!="GrantAbilityEffect" || Field(c.effect,"GrantableAbilityType")!=Abilities::SelfTeleport::Id)return true;
    Obj* model=Core::ModelRegistry::Get(Engine::RouterContext::RouterModel(c.router));Obj* entity=Find(model,Field(c.effect,"EffectEntityId"));return !Engine::EntityAccess::Component(entity,"Continuous");
}
}
