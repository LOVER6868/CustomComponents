// Included inside ExtraCombat. Lane-only/all attacks use the native ordinary
// attack generator, so the selected second fighter cannot leak into their aim.
inline bool LaneEncounter(Obj* fighter){
    return Abilities::LaneAttacks::OnlyActive(fighter) ||
           Engine::EntityAccess::Component(fighter,"AttacksInAllLanes");
}
inline std::vector<Listener> LaneListeners(Obj* model,const CombatStepEffect* step){
    std::vector<Listener> listeners;const int first=step->Read("First");
    Obj* fighter=Engine::EntityAccess::Find(model,first);if(!fighter)return listeners;
    const int home=step->Read("FirstLane");listeners.push_back({first,home});
    Obj* only=Abilities::LaneAttacks::OnlyActive(fighter);
    const int mask=only?Abilities::LaneAttacks::Mask(only):0;
    const bool all=!only && Engine::EntityAccess::Component(fighter,"AttacksInAllLanes");
    const int owner=Engine::BoardPlacement::Owner(fighter);
    const auto api=Engine::EntityAccess::Api();const auto* entities=Core::Metadata::Bind(api.entities,model).Get();if(!Engine::EntityAccess::Valid(entities))return listeners;
    std::unordered_set<int> attackLanes;
    for(int i=0;i<entities->size;++i){Obj* lane=Engine::EntityAccess::Component(entities->items->m_Items[i],"Lane");if(!lane)continue;
        const int order=Field(lane,"CombatOrderIndex");
        if(order>=0 && order<31 && (all || (mask&(1<<order))))attackLanes.insert(Engine::Match::Id(entities->items->m_Items[i]));
    }
    for(int i=0;i<entities->size;++i){Obj* card=entities->items->m_Items[i];const int id=Engine::Match::Id(card);if(id==first)continue;
        if(Engine::EntityAccess::Component(card,"Environment")){
            const int lane=EnvironmentLane(card);if(lane==home || attackLanes.count(lane))listeners.push_back({id,lane});
        }else if(Engine::BoardPlacement::Owner(card)!=owner && LiveFighter(model,id)){
            const int lane=Lane(model,id);if(attackLanes.count(lane))listeners.push_back({id,lane});
        }
    }
    std::stable_sort(listeners.begin()+1,listeners.end(),[&](const Listener& a,const Listener& b){
        return std::pair(Field(Engine::EntityAccess::Component(Engine::EntityAccess::Find(model,a.lane),"Lane"),"CombatOrderIndex"),a.card)<std::pair(Field(Engine::EntityAccess::Component(Engine::EntityAccess::Find(model,b.lane),"Lane"),"CombatOrderIndex"),b.card);
    });
    return listeners;
}
inline bool BuildLaneEncounter(Obj* model,Obj* instances,CombatStepEffect* step,EffectBatch& effects){
    const int first=step->Read("First");if(!LiveFighter(model,first))return false;
    const auto type=BNM::Class("PvZCards.Core.Systems.Utilities","FighterDamageEffectCreator");
    auto service=Engine::Match::Service(instances,type);auto create=type.GetMethod("CreateEffectsForFighter",1).cast<Obj*>();
    if(!service || !create.IsValid())return false;
    Obj* generated=Core::Metadata::Bind(create,service->Get())(first);Core::ManagedRuntime::Root root(generated);
    const auto* list=reinterpret_cast<ObjList*>(generated);if(!Engine::EntityAccess::Valid(list))return false;
    for(int i=0;i<list->size;++i)if(Obj* effect=list->items->m_Items[i])effects.push_back(effect);
    for(const auto listener:LaneListeners(model,step)){
        if(listener.card!=first && LiveFighter(model,listener.card))
            if(!BuildAttackEffects(model,instances,listener.card,first,effects))return false;
    }
    return true;
}
