#pragma once
#include <map>
#include <tuple>
#include "variables/QueuedTriggerValues.hpp"

namespace CustomComponents::Variables::EventCounter {
// An explicitly authored pre-validation counter. Ordinary SetVariable effects
// retain their normal queued execution order.
struct Component:Obj {
    Str* TriggerName=nullptr;Str* Variable=nullptr;Str* Duration=nullptr;
    int Amount=1;Obj* EventQuery=nullptr;Obj* SelfQuery=nullptr;
    BNM_CustomClass(Component,BNM::CompileTimeClassBuilder("PvZCards.Engine.Components","VariableEventCounter","EngineLib.dll").Build(),BNM::CompileTimeClassBuilder("PvZCards.Engine.Components","MarkerComponent","EngineLib.dll").Build(),{});
    BNM_CustomField(TriggerName,BNM::Defaults::Get<Str*>(),"TriggerName");
    BNM_CustomField(Variable,BNM::Defaults::Get<Str*>(),"Variable");
    BNM_CustomField(Duration,BNM::Defaults::Get<Str*>(),"Duration");
    BNM_CustomField(Amount,BNM::Defaults::Get<int>(),"Amount");
    BNM_CustomField(EventQuery,QueryType(),"EventQuery");
    BNM_CustomField(SelfQuery,QueryType(),"SelfQuery");
    void DeepCopyFrom(Obj* source,Obj* instances){auto* from=reinterpret_cast<Component*>(source);
        Reference(this,&TriggerName,from?from->TriggerName:nullptr);Reference(this,&Variable,from?from->Variable:nullptr);
        Reference(this,&Duration,from?from->Duration:nullptr);Amount=from?from->Amount:1;
        Core::ManagedQuery::CopyOwnedField(this,BNM::Class(this).GetField("EventQuery").cast<Obj*>(),from?from->EventQuery:nullptr,instances);
        Core::ManagedQuery::CopyOwnedField(this,BNM::Class(this).GetField("SelfQuery").cast<Obj*>(),from?from->SelfQuery:nullptr,instances);
    }
    void HandleReturnToPool(Obj* instances){Reference(this,&TriggerName,nullptr);Reference(this,&Variable,nullptr);Reference(this,&Duration,nullptr);Amount=1;
        Core::ManagedQuery::ClearOwnedField(this,BNM::Class(this).GetField("EventQuery").cast<Obj*>(),instances);
        Core::ManagedQuery::ClearOwnedField(this,BNM::Class(this).GetField("SelfQuery").cast<Obj*>(),instances);
    }
    BNM_CustomMethod(DeepCopyFrom,false,BNM::Defaults::Get<void>(),"DeepCopyFrom",BNM::Defaults::Get<Obj*>(),BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
    BNM_CustomMethod(HandleReturnToPool,false,BNM::Defaults::Get<void>(),"HandleReturnToPool",BNM::CompileTimeClassBuilder("PvZCards.Engine","InstanceManager","EngineLib.dll").Build());
};
using Identity=std::tuple<Core::ModelRegistry::Token,int,std::string>;
inline std::mutex commitLock;
inline std::map<Identity,unsigned long long> committed;
inline void Apply(Obj* model,Obj* instances,Obj* effect,std::string_view event,unsigned long long serial){
    if(!model || !instances || !effect || event.empty() || !serial)return;
    auto api=Engine::EntityAccess::Api();auto* entities=api.entities[model].Get();
    if(!Engine::EntityAccess::Valid(entities))return;
    const auto token=Core::ModelRegistry::Identify(model,true);
    Obj* context=nullptr;std::shared_ptr<Core::ManagedRuntime::Root> root;
    struct ReturnContext {Obj*& context;Obj* instances;~ReturnContext(){if(context)Core::ManagedPool::Return(instances,context);}} cleanup{context,instances};
    QueuedTriggerValues::ActiveScope liveValues(nullptr);
    struct Roster {const void* model=nullptr;unsigned long long event=0;std::vector<int> owners;};
    static thread_local Roster roster;
    if(roster.model!=model || roster.event!=serial){
        roster={model,serial,{}};
        for(int i=0;i<entities->size;++i)if(Engine::EntityAccess::Component(entities->items->m_Items[i],"VariableEventCounter"))roster.owners.push_back(Engine::Match::Id(entities->items->m_Items[i]));
    }
    for(int id:roster.owners){
        Obj* owner=Engine::EntityAccess::Find(model,id);
        auto* counter=reinterpret_cast<Component*>(Engine::EntityAccess::Component(owner,"VariableEventCounter"));
        if(!counter || Text(counter->TriggerName)!=event || !Name(Text(counter->Variable)) ||
           !Engine::EntityAccess::Component(owner,"LaneElement") || Engine::EntityAccess::Component(owner,"FaceDown") ||
           Engine::EntityAccess::Component(owner,"MarkedForDeath"))continue;
        const Identity key{token,Engine::Match::Id(owner),std::string(event)};
        {std::lock_guard<std::mutex> guard(commitLock);auto it=committed.find(key);if(it!=committed.end() && it->second==serial)continue;}
        if(!context){context=Core::Metadata::Bind(effectToQueryContext,effect)(instances);if(!context)return;root=std::make_shared<Core::ManagedRuntime::Root>(context);if(!root->Get())return;}
        Core::Metadata::Bind(querySelfId,context).Set(Engine::Match::Id(owner));
        if(counter->SelfQuery){auto matches=BNM::Class(counter->SelfQuery).GetMethod("Matches",2).cast<bool>();
            if(!matches.IsValid() || !Core::Metadata::Bind(matches,counter->SelfQuery)(owner,context))continue;}
        Obj* target=Engine::EntityAccess::Find(model,ContextId(context,"get_TargetId"));
        if(counter->EventQuery){auto matches=BNM::Class(counter->EventQuery).GetMethod("Matches",2).cast<bool>();
            if(!target || !matches.IsValid() || !Core::Metadata::Bind(matches,counter->EventQuery)(target,context))continue;}
        bool success=false;const auto name=Text(counter->Variable);const auto duration=Text(counter->Duration,"EndOfTurn");
        const int durationId=Engine::EffectDurations::Parse(duration);
        if(durationId==Engine::EffectDurations::Permanent){
            auto values=Stored(owner);if(values){Table updated=*values;if(Assign(updated,name,Value{Kind::Int,{counter->Amount}},"Add"))success=Write(model,owner,updated);}
        }else if(Engine::EffectDurations::Timed(durationId)){
            auto values=Timed(owner);if(values && values->size()<256){values->push_back({name,"Add",durationId,counter->Amount});success=WriteTimed(model,owner,*values);}
        }
        if(success){std::lock_guard<std::mutex> guard(commitLock);committed[key]=serial;
            static std::atomic<unsigned> samples{0};if(samples.fetch_add(1)<160)LOG_INFO("CustomComponents/Variables/EVENT_COUNTER: event=%s serial=%llu owner=%d variable=%s amount=%d",std::string(event).c_str(),serial,Engine::Match::Id(owner),name.c_str(),counter->Amount);}
    }
}
inline bool Initialize(){
    using K=Bridge::FieldKind;
    auto type=BNM::Class("PvZCards.Engine.Components","VariableEventCounter");auto base=BNM::Class("PvZCards.Engine.Components","MarkerComponent");
    if(!CustomComponentRegistry::Register({"VariableEventCounter","PvZCards.Engine.Components","EngineLib.dll","MarkerComponent","PvZCards.Engine.Components","EngineLib.dll"}) ||
       !Core::ComponentLifecycle::Patch(type,base) || !Bridge::Register("PvZCards.Engine.Components","VariableEventCounter","PvZCards.Engine.Components","MarkerComponent",{{"TriggerName",K::String},{"Variable",K::String},{"Duration",K::String},{"Amount",K::Int32}},false))return false;
    Core::ModelRegistry::retired.Add([](Core::ModelRegistry::Token token){std::lock_guard<std::mutex> guard(commitLock);std::erase_if(committed,[&](const auto& entry){return std::get<0>(entry.first)==token;});});
    return true; // Legacy schema; normal trigger timing owns dispatch.
}
}
