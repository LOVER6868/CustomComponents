#pragma once
#include <string_view>
#include <BNM/Class.hpp>
#include <BNM/Field.hpp>
#include <BNM/Image.hpp>
#include <unordered_map>
#include "core/HookRegistry.hpp"
#include "core/Observers.hpp"
#include "engine/EffectGuards.hpp"
#include "engine/MatchAccess.hpp"
#include "core/Performance.hpp"

namespace CustomComponents::Engine::TriggerDispatch {
using Obj=BNM::IL2CPP::Il2CppObject;
struct Context {Obj* model;Obj* instances;Obj* effect;std::string_view event;unsigned long long serial;int listenerSource=-1;};
inline Core::Observers<const Context&> before,after;
struct Scope {Context context;explicit Scope(Context value):context(value){before.Notify(context);}~Scope(){after.Notify(context);}};
inline BNM::Field<Obj*> instancesField,finderField;
inline void (*originalProcess)(Obj*,Obj*,const void*)=nullptr;
using ProcessFn=decltype(originalProcess);
struct NativeProducer {ProcessFn original;std::string event;};
inline std::unordered_map<BNM::IL2CPP::Il2CppClass*,NativeProducer> producers;
static void Process(Obj* producer,Obj* effect,const void* method){
    Obj* finder=producer?Core::Metadata::Bind(finderField,producer).Get():nullptr;
    // TriggerSystem<T> uses an EFFECT type. Its GenericEffectEntityFinder<T>
    // carries the TRIGGER marker. Do not infer marker names from effect names.
    auto* generic=finder && finder->klass?finder->klass->generic_class:nullptr;
    auto* args=generic?generic->context.class_inst:nullptr;
    std::string_view event;
    ProcessFn invoke=originalProcess;
    if(auto found=producers.find(producer?producer->klass:nullptr);found!=producers.end()){
        invoke=found->second.original;
    }
    if(event.empty() && args && args->type_argc==1){auto* type=BNM::Class(args->type_argv[0]).GetClass();if(type && type->name)event=type->name;}
    Core::Performance::Scope timing(Core::Performance::Area::NativeTrigger,event.empty()?"unknown":event.data());
    const auto current=EffectGuards::current;
    Obj* model=current.router?Core::ModelRegistry::Get(RouterContext::RouterModel(current.router)):nullptr;
    Obj* instances=producer?Core::Metadata::Bind(instancesField,producer).Get():nullptr;
    Scope scope({model,instances,effect,event,current.event});
    static std::atomic<unsigned> traces{0};
    if(traces.fetch_add(1)<16)LOG_INFO("CustomComponents/TriggerDispatch/NATIVE: event=%s serial=%llu model=%p instances=%p",std::string(event).c_str(),current.event,model,instances);
    if(invoke)invoke(producer,effect,method);
}
inline bool Initialize(){
    auto definition=BNM::Class("PvZCards.Core.Systems","TriggerSystem`1");
    auto producer=definition.GetGeneric({BNM::Class("System","Object").GetCompileTimeClass()});
    instancesField=producer.GetField("<Instances>k__BackingField").cast<Obj*>();
    finderField=producer.GetField("<Finder>k__BackingField").cast<Obj*>();
    if(!instancesField.IsValid() || !finderField.IsValid() || instancesField.GetOffset()<sizeof(Obj) ||
       !Core::Hooks::Install(producer.GetMethod("ProcessEffect",1),Process,originalProcess))return false;
    // A successful hook on T=Object does not prove that the game's closed
    // generic producers use that body. Resolve the actual trigger instances;
    // HookRegistry reuses an existing trampoline when bodies are shared.
    auto effectBase=BNM::Class("PvZCards.Engine","Effect").GetClass();
    if(!effectBase)return false;
    for(auto trigger:BNM::Image("EngineLib.dll").GetClasses()){
        auto* type=trigger.GetClass();if(!type)continue;
        bool isEffect=false;
        for(auto* parent=type;parent;parent=parent->parent)if(parent==effectBase){isEffect=true;break;}
        if(!isEffect || type==effectBase)continue;
        auto closed=definition.GetGeneric({trigger.GetCompileTimeClass()});
        auto entry=closed.GetMethod("ProcessEffect",1);
        if(!closed.IsValid() || !entry.IsValid() || !entry.GetInfo()->methodPointer)continue;
        ProcessFn original=nullptr;
        if(!Core::Hooks::Install(entry,Process,original))return false;
        producers.emplace(closed.GetClass(),NativeProducer{original,type->name});
    }
    LOG_INFO("CustomComponents/TriggerDispatch/INIT: closedProducers=%zu",producers.size());
    return !producers.empty();
}
}
