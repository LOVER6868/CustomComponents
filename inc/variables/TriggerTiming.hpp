#pragma once
#include <map>
#include "variables/EventCounter.hpp"
#include "engine/TriggerDispatch.hpp"
#include "engine/TriggerEligibility.hpp"

namespace CustomComponents::Variables::TriggerTiming {
struct Previous {QueuedTriggerValues::Ptr snapshot;unsigned long long serial;std::string kind;};
inline thread_local std::vector<Previous> previous;
inline std::mutex committedLock;
inline std::map<std::pair<Core::ModelRegistry::Token,int>,unsigned long long> committed;
inline BNM::Method<bool> validate;
inline BNM::Method<Obj*> create;
inline BNM::Field<int> selection;
inline BNM::Field<Obj*> runtimeContext;
inline BNM::Method<void> copyContext;
inline bool Early(Obj* entity){auto* descriptor=reinterpret_cast<Descriptor*>(Engine::EntityAccess::Component(entity,"SetVariableEffectDescriptor"));return descriptor && Text(descriptor->ExecutionTiming)=="BeforeTriggers";}
inline bool AllowQueued(const Engine::TriggerEligibility::Context& context,int){return executingBeforeTriggers || !Early(context.effectEntity);}
inline bool Listens(Obj* entity,std::string_view event){
    if(Engine::EntityAccess::Component(entity,std::string(event).c_str()))return true;
    Obj* custom=Engine::EntityAccess::Component(entity,"CustomTrigger");
    if(!custom)return false;
    auto name=BNM::Class(custom).GetField("TriggerName").cast<Str*>();
    return name.IsValid() && Text(Core::Metadata::Bind(name,custom).Get())==event;
}
inline void Apply(const Engine::TriggerDispatch::Context& event){
    Core::Performance::Scope timing(Core::Performance::Area::TriggerPreparation,"early-variable-effects");
    if(!event.model || !event.instances || !event.effect || event.event.empty() || !event.serial)return;
    auto api=Engine::EntityAccess::Api();auto* entities=api.entities[event.model].Get();if(!Engine::EntityAccess::Valid(entities))return;
    // Snapshot IDs: native effect creation and variable writes can add components.
    struct Roster {Core::ModelRegistry::Token model{};unsigned long long serial=0;std::vector<int> ids;};
    static thread_local Roster roster;
    const auto modelToken=Core::ModelRegistry::Identify(event.model,true);
    if(roster.model!=modelToken || roster.serial!=event.serial){
        roster={modelToken,event.serial,{}};
        for(int i=0;i<entities->size;++i){Obj* entity=entities->items->m_Items[i];if(Early(entity))roster.ids.push_back(Engine::Match::Id(entity));}
    }
    std::vector<int> setters;
    for(int id:roster.ids)if(Listens(Engine::EntityAccess::Find(event.model,id),event.event))setters.push_back(id);
    if(setters.empty())return;
    auto validator=Engine::Match::Service(event.instances,BNM::Class("PvZCards.Core.Systems.Utilities","TriggerValidator"));
    auto creator=Engine::Match::Service(event.instances,BNM::Class("PvZCards.Core.Systems.Utilities","EffectCreator"));
    if(!validator || !creator)return;
    Obj* context=Core::Metadata::Bind(effectToQueryContext,event.effect)(event.instances);Core::ManagedRuntime::Root root(context);if(!root.Get())return;
    struct Release {Obj* instances;Obj* value;~Release(){Core::ManagedPool::Return(instances,value);}} release{event.instances,context};
    if(NativeContextModel(context)!=event.model){LOG_ERR("CustomComponents/Variables/BEFORE_TRIGGERS: model mismatch");return;}
    const auto token=Core::ModelRegistry::Identify(event.model,true);
    struct Execution {bool before=executingBeforeTriggers;Execution(){executingBeforeTriggers=true;}~Execution(){executingBeforeTriggers=before;}} execution;
    for(int id:setters){
        Obj* entity=Engine::EntityAccess::Find(event.model,id);const int owner=Source(entity);
        if(event.listenerSource>=0 && event.listenerSource!=owner)continue;
        const auto key=std::pair{token,id};
        {std::lock_guard<std::mutex> guard(committedLock);auto it=committed.find(key);if(it!=committed.end() && it->second==event.serial)continue;}
        Obj* filter=Engine::EntityAccess::Component(entity,"PrimaryTargetFilter");
        if(!filter || Core::Metadata::Bind(selection,filter).Get()!=0){LOG_ERR("CustomComponents/Variables/BEFORE_TRIGGERS: effect=%d requires automatic All targeting",id);continue;}
        Core::Metadata::Bind(querySelfId,context).Set(owner);
        // The native validator evaluates every Condition, including independent
        // trigger-source/target filters and arbitrary nested query expressions.
        if(!Core::Metadata::Bind(validate,validator->Get())(entity,context))continue;
        {std::lock_guard<std::mutex> guard(committedLock);committed[key]=event.serial;}
        Obj* raw=Core::Metadata::Bind(create,creator->Get())(entity,context);Core::ManagedRuntime::Root effectsRoot(raw);
        auto* effects=reinterpret_cast<Engine::EntityAccess::List*>(raw);
        if(!Engine::EntityAccess::Valid(effects))continue;
        int count=0;
        for(int i=0;i<effects->size;++i){Obj* runtime=effects->items->m_Items[i];
            if(runtime && runtime->klass && std::string_view(runtime->klass->name)=="SetVariableEffect"){
                if(Obj* payload=Core::Metadata::Bind(runtimeContext,runtime).Get())Core::Metadata::Bind(copyContext,payload)(context);
                Prepare({Engine::EffectGuards::current.router,runtime,event.serial});++count;
            }
            if(runtime)Core::ManagedPool::Return(event.instances,runtime);
        }
        Core::ManagedPool::Return(event.instances,raw);
        LOG_INFO("CustomComponents/Variables/BEFORE_TRIGGERS: event=%s serial=%llu effect=%d generated=%d",std::string(event.event).c_str(),event.serial,id,count);
    }
}
inline void Begin(const Engine::TriggerDispatch::Context& event){
    previous.push_back({QueuedTriggerValues::active,QueuedTriggerValues::captureSerial,std::move(QueuedTriggerValues::captureKind)});
    QueuedTriggerValues::active.reset();QueuedTriggerValues::captureSerial=event.serial;QueuedTriggerValues::captureKind=event.event;
    const auto error=BNM::TryInvoke([&]{
        EventCounter::Apply(event.model,event.instances,event.effect,event.event,event.serial);
        Apply(event);
    });
    if(error.IsValid())LOG_ERR("CustomComponents/Variables: pre-trigger variable update failed");
    QueuedTriggerValues::active=QueuedTriggerValues::Capture(event.model);
    static std::atomic<unsigned> logged{0};
    if(logged.fetch_add(1)<192 && event.model &&
       reinterpret_cast<std::uintptr_t>(event.model)==Core::SceneViewContext::ModelIdentity())
        LOG_INFO("CustomComponents/TriggerSnapshots/EVENT_CAPTURE: event=%s serial=%llu snapshot=%llu variables=%zu revision=%llu",
            std::string(event.event).c_str(),event.serial,
            QueuedTriggerValues::active?QueuedTriggerValues::active->sequence:0,
            QueuedTriggerValues::active?QueuedTriggerValues::active->Size():0,
            static_cast<unsigned long long>(Engine::SimulationChanges::Revision(event.model)));
}
inline void End(const Engine::TriggerDispatch::Context&){if(!previous.empty()){auto value=std::move(previous.back());previous.pop_back();QueuedTriggerValues::active=std::move(value.snapshot);QueuedTriggerValues::captureSerial=value.serial;QueuedTriggerValues::captureKind=std::move(value.kind);}}
inline bool Initialize(){
    validate=BNM::Class("PvZCards.Core.Systems.Utilities","TriggerValidator").GetMethod("ShouldTriggerEffect",2).cast<bool>();
    create=BNM::Class("PvZCards.Core.Systems.Utilities","EffectCreator").GetMethod("CreateEffects",2).cast<Obj*>();
    selection=BNM::Class("PvZCards.Engine.Components","PrimaryTargetFilter").GetField("SelectionType").cast<int>();
    runtimeContext=BNM::Class("PvZCards.Engine","Effect").GetField("Context").cast<Obj*>();
    // sprig-lint: allow names both CopyFrom overloads name their argument context; BNM type selection disambiguates EntityQueryContext
    copyContext=BNM::Class("PvZCards.Engine","EffectContext").GetMethod("CopyFrom",{BNM::Class("PvZCards.Engine","EntityQueryContext").GetCompileTimeClass()}).cast<void>();
    Core::ModelRegistry::retired.Add([](auto token){std::lock_guard<std::mutex> guard(committedLock);std::erase_if(committed,[&](const auto& row){return row.first.first==token;});});
    return validate.IsValid() && create.IsValid() && selection.IsValid() && runtimeContext.IsValid() && copyContext.IsValid() && Engine::TriggerDispatch::Initialize() &&
        Engine::TriggerDispatch::before.Add(Begin) && Engine::TriggerDispatch::after.Add(End) && Engine::TriggerEligibility::rules.Add(AllowQueued);
}
}
