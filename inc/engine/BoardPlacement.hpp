#pragma once
#include "engine/BoardPlacementPolicy.hpp"
#include "engine/PlacementQueryContext.hpp"
#include "engine/CardTypes.hpp"
#include "engine/MatchAccess.hpp"
namespace CustomComponents::Engine::BoardPlacement {
using Obj=EntityAccess::Obj;using List=EntityAccess::List;
inline Obj* Model(Obj* context) {
    if(PlacementQueryContext::active.model)return PlacementQueryContext::active.model;
    static auto field=BNM::Class("PvZCards.Engine","EntityQueryContext").GetField("<Model>k__BackingField").cast<Obj*>();auto f=field;
    return context && f.IsValid()?f[context].Get():nullptr;
}
inline int Guid(Obj* card){auto* c=EntityAccess::Component(card,"Card");static auto field=BNM::Class("PvZCards.Engine.Components","Card").GetField("Guid").cast<int>();auto f=field;return c && f.IsValid()?f[c].Get():-1;}
inline bool Active(Obj* card,const char* ability) {
    auto* c=EntityAccess::Component(card,ability);static auto method=BNM::Class("PvZCards.Engine.Components","GrantableAbility").GetMethod("IsActive",0).cast<bool>();auto m=method;
    return c && m.IsValid() && m[c]();
}
inline int Owner(Obj* card){Obj* c=EntityAccess::Component(card,"Owner");static auto field=BNM::Class("PvZCards.Engine.Components","Owner").GetField("OwnerId").cast<int>();auto f=field;return c && f.IsValid()?f[c].Get():-1;}
inline int Player(Obj* model,Obj* card,Obj* context) {
    if(!model || !card)return -1;
    const int owner=Owner(card);
    if(EntityAccess::Find(model,Match::Id(card))==card && EntityAccess::Component(EntityAccess::Find(model,owner),"Player"))return owner;
    const bool plant=EntityAccess::Component(card,"Plants"),zombie=EntityAccess::Component(card,"Zombies");
    if(plant==zombie)return -1;
    auto api=EntityAccess::Api();auto* list=api.entities[model].Get();if(!EntityAccess::Valid(list))return -1;
    int player=-1;
    for(int i=0;i<list->size;++i){Obj* entity=list->items->m_Items[i];if(EntityAccess::Component(entity,"Player") && EntityAccess::Component(entity,plant?"Plants":"Zombies")){if(player>=0)return -1;player=Match::Id(entity);}}
    return player;
}
inline void(*augment)(Obj*,BoardPlacementPolicy::Card&)=nullptr;
inline bool Fits(Obj* model,Obj* card,Obj* laneEntity,Obj* context) {
    if(!model || !card || !laneEntity)return false;
    Obj* lane=EntityAccess::Component(laneEntity,"Lane");if(!lane)return false;
    BoardPlacementPolicy::Card traits{CardTypes::Fighter(card),EntityAccess::Component(card,"Environment")!=nullptr,EntityAccess::Component(card,"Plants")!=nullptr,Active(card,"Teamup"),Active(card,"Aquatic")};
    if(augment)augment(PlacementQueryContext::active.effect,traits);
    const int player=Player(model,card,context);if(player<0)return false;
    static auto count=BNM::Class("PvZCards.Engine.Components","Lane").GetMethod("GetNumFightersForPlayer",1).cast<int>();
    static auto fighters=BNM::Class("PvZCards.Engine.Components","Lane").GetMethod("GetFightersForPlayer",1).cast<BNM::Structures::Mono::List<int>*>();
    auto n=count;auto f=fighters;if(!n.IsValid() || !f.IsValid())return false;
    const int size=n[lane](player);bool teamup=false;
    if(size==1){auto* ids=f[lane](player);if(ids && ids->items && ids->size==1)teamup=Active(EntityAccess::Find(model,ids->items->m_Items[0]),"Teamup");}
    return BoardPlacementPolicy::Fits(traits,size,teamup,EntityAccess::Component(laneEntity,"WaterTerrain")!=nullptr);
}
inline bool Matches(Obj* query,Obj* candidate,Obj* context) {
    if(!query || !candidate)return false;
    auto method=BNM::Class(query).GetMethod("Matches",2).cast<bool>();return method.IsValid() && method[query](candidate,context);
}
inline std::shared_ptr<Core::ManagedRuntime::Root> Context(Obj* model,Obj* effect,int target=-1) {
    auto type=BNM::Class("PvZCards.Engine","EntityQueryContext");auto root=std::make_shared<Core::ManagedRuntime::Root>(type.CreateNewObjectParameters());
    Obj* context=root->Get();if(!context)return {};
    auto modelField=type.GetField("<Model>k__BackingField").cast<Obj*>();auto self=type.GetField("SelfId").cast<int>();
    auto targetSet=type.GetMethod("set_TargetId",1).cast<void>();auto sourceSet=type.GetMethod("set_SourceId",1).cast<void>();auto effectSet=type.GetMethod("set_EffectEntityId",1).cast<void>();
    Obj* link=EntityAccess::Component(effect,"EffectEntity");auto source=BNM::Class("PvZCards.Engine.Components","EffectEntity").GetField("SourceId").cast<int>();
    if(!modelField.IsValid() || !self.IsValid() || !targetSet.IsValid() || !sourceSet.IsValid() || !effectSet.IsValid())return {};
    const int id=link && source.IsValid()?source[link].Get():-1;
    modelField[context].Set(model);self[context].Set(id);sourceSet[context](id);effectSet[context](Match::Id(effect));targetSet[context](target);return root;
}
inline Obj* Database(Obj* effect=nullptr) {
    if(!effect)effect=PlacementQueryContext::active.effect;
    static auto type=BNM::Class("PvZCards.Core.Utilities","CardDatabase");
    Obj* database=nullptr;
    if(effect){const auto error=BNM::TryInvoke([&]{database=Match::Provide(Match::Instances(effect),type.GetMonoType());});if(error.IsValid())database=nullptr;}
    static auto get=type.GetMethod("GetInstance",0).cast<Obj*>();auto g=get;
    return database?database:g.IsValid()?g():nullptr;
}
inline Obj* Definition(int guid,Obj* effect=nullptr) {
    static auto find=BNM::Class("PvZCards.Core.Utilities","CardDatabase").GetMethod("GetBackingEntityByGuid",1).cast<Obj*>();auto f=find;
    Obj* db=Database(effect);return db && f.IsValid()?f[db](guid):nullptr;
}
inline std::vector<Obj*> Cards(Obj* query,Obj* context,bool live) {
    std::vector<Obj*> result;if(!query)return result;
    Obj* model=Model(context);if(!model)return result;
    if(!live){
        Obj* database=Database();
        static auto find=BNM::Class("PvZCards.Core.Utilities","CardDatabase").GetMethod("FindEntities",1).cast<Obj*>();auto f=find;
        Obj* found=database && context && f.IsValid()?f[database](query):nullptr;
        Core::ManagedRuntime::Root lifetime(found);
        auto* cards=reinterpret_cast<List*>(found);if(!EntityAccess::Valid(cards))return result;
        for(int i=0;i<cards->size;++i)if(Guid(cards->items->m_Items[i])>0)result.push_back(cards->items->m_Items[i]);
        return result;
    }
    auto api=EntityAccess::Api();auto* list=model && api.entities.IsValid()?api.entities[model].Get():nullptr;if(!EntityAccess::Valid(list))return result;
    auto matches=BNM::Class(query).GetMethod("Matches",2).cast<bool>();if(!matches.IsValid())return result;
    for(int i=0;i<list->size;++i){Obj* card=list->items->m_Items[i];if(Guid(card)>0 && matches[query](card,context))result.push_back(card);}
    return result;
}
}
