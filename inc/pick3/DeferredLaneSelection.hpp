#pragma once
#include "pick3/AdditionalSelectionSettings.hpp"
namespace CustomComponents::Pick3::AdditionalSelection {
using QueryList=BNM::Structures::Mono::List<Obj*>;
inline bool PickedLaneDependency(Obj* query,bool insideCard=false,unsigned depth=0) {
    if(!query || !query->klass || !query->klass->name || depth>=16)return false;
    const std::string_view name=query->klass->name;
    if(insideCard && (name=="PickedFromThreeQuery" || name=="PickedFromThreeEntityQuery"))return true;
    if(name=="OpenLaneForCardQuery") {
        auto field=BNM::Class(query).GetField("CardQuery").cast<Obj*>();
        return field.IsValid() && PickedLaneDependency(field[query].Get(),true,depth+1);
    }
    if(name=="NotQuery") {
        auto field=BNM::Class(query).GetField("Query").cast<Obj*>();
        return field.IsValid() && PickedLaneDependency(field[query].Get(),insideCard,depth+1);
    }
    if(name=="CompositeAllQuery" || name=="CompositeAnyQuery") {
        auto field=BNM::Class(query).GetField("Queries").cast<QueryList*>();
        auto* list=field.IsValid()?field[query].Get():nullptr;
        if(!list || !list->items || list->size<0 || list->size>int(list->items->capacity))return false;
        for(int i=0;i<list->size;++i)if(PickedLaneDependency(list->items->m_Items[i],insideCard,depth+1))return true;
    }
    return false;
}
inline bool DefersPickedLane(Obj* filter) {
    const auto setting=Get(filter);
    if(setting.first || setting.mode!=Manual || setting.primary<3 || setting.primary>5)return false;
    auto field=BNM::Class("PvZCards.Engine.Components","TargetEntityFilter").GetField("AdditionalTargetQuery").cast<Obj*>();
    return filter && field.IsValid() && PickedLaneDependency(field[filter].Get());
}
inline std::shared_ptr<Core::ManagedRuntime::Root> LaneProxy() {
    auto type=BNM::Class("PvZCards.Engine.Queries","HasComponentQuery");
    auto root=std::make_shared<Core::ManagedRuntime::Root>(type.CreateNewObjectParameters());
    auto field=type.GetField("ComponentType").cast<void*>();auto lane=BNM::Class("PvZCards.Engine.Components","Lane");
    if(!root->Get() || !field.IsValid() || !lane.IsValid())return {};
    field[root->Get()].Set(lane.GetMonoType());return root;
}
}
