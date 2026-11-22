#pragma once
#include "engine/EffectGuards.hpp"
#include "engine/MatchAccess.hpp"
#include "core/ModelRegistry.hpp"

namespace CustomComponents::Engine::EffectContinuations {
using Obj=Match::Obj;
using Root=Core::ManagedRuntime::Root;
enum class Placement {AfterCurrentGroup,CurrentGroup};
struct Pending {unsigned long long event;Placement placement;std::shared_ptr<Root> queue,array;};
inline thread_local std::vector<Pending> pending;
inline bool Append(Obj* queue,Obj* array,Placement placement=Placement::AfterCurrentGroup){
    static auto add=[] {
        for(auto candidate:BNM::Class("PvZCards.Engine","EffectQueue").GetMethods()){
            auto* info=candidate.GetInfo();
            if(!info || std::string_view(info->name)!="AddAfterCurrentGroup" || info->parameters_count!=1)continue;
            auto* type=BNM::Class(info->parameters[0]).GetClass();
            if(type && type->rank==1)return candidate.cast<void>();
        }
        return BNM::Method<void>{};
    }();
    static auto current=[] {
        for(auto candidate:BNM::Class("PvZCards.Engine","EffectQueue").GetMethods()){
            auto* info=candidate.GetInfo();if(!info || std::string_view(info->name)!="AddToCurrentGroup" || info->parameters_count!=1)continue;
            auto* type=BNM::Class(info->parameters[0]).GetClass();if(type && type->rank==1)return candidate.cast<void>();
        }
        return BNM::Method<void>{};
    }();
    auto selected=placement==Placement::CurrentGroup?current:add;
    if(!queue || !array || !selected.IsValid())return false;
    return !BNM::TryInvoke([&]{Core::Metadata::Bind(selected,queue)(array);}).IsValid();
}
// Schedule only after the native router has flushed its triggered effects.
// Appending from a preparation callback can put a continuation BEFORE the
// start-trigger group, even though both use AddAfterCurrentGroup.
inline bool Schedule(Obj* model,Obj* instances,const std::vector<Obj*>& effects,Placement placement=Placement::AfterCurrentGroup){
    Obj* queue=Match::Provide(instances,BNM::Class("PvZCards.Engine","EffectCollection").GetMonoType());
    if(!model || !queue || Match::Provide(instances,BNM::Class("PvZCards.Engine","EntityModel").GetMonoType())!=model)return false;
    auto* array=BNM::Class("PvZCards.Engine","Effect").NewArray<Obj*>(effects.size());
    auto root=std::make_shared<Root>(reinterpret_cast<Obj*>(array));
    if(!root->Get())return false;
    for(std::size_t i=0;i<effects.size();++i){
        if(Core::ManagedRuntime::writeBarrier)Core::ManagedRuntime::writeBarrier(array,reinterpret_cast<void**>(&array->m_Items[i]),effects[i]);
        else array->m_Items[i]=effects[i];
    }
    const auto event=EffectGuards::current.event;
    if(!event)return Append(queue,reinterpret_cast<Obj*>(array),placement);
    pending.push_back({event,placement,std::make_shared<Root>(queue),std::move(root)});
    return true;
}
inline void Flush(const EffectGuards::Context& context){
    std::vector<Pending> ready;
    for(auto it=pending.begin();it!=pending.end();)if(it->event==context.event){ready.push_back(std::move(*it));it=pending.erase(it);}else ++it;
    for(auto& item:ready)if(!Append(item.queue->Get(),item.array->Get(),item.placement))LOG_ERR("CustomComponents/Continuation: enqueue failed event=%llu",context.event);
}
inline bool Initialize(){return EffectGuards::after.Add(Flush);}
}
