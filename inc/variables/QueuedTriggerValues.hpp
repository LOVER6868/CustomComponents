#pragma once
#include "core/Performance.hpp"
// All stock TriggerSystem<T> specializations use the same generic ProcessEffect
// code path. Capture variable values when each TriggerData is emitted, associate
// them with the created TriggeredEffect, and restore them only while that exact
// triggered effect is resolving. This works for EVERY trigger marker, not only
// PlayTrigger. No managed Entity/Context pointer is kept in a snapshot.
#include <mutex>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include "core/HookRegistry.hpp"
#include "core/Observers.hpp"
#include "engine/TriggerQueue.hpp"
#include "engine/EffectCreationEvents.hpp"
#include "engine/EffectLifecycleEvents.hpp"
#include "engine/EffectQueue.hpp"
#include "core/ModelRegistry.hpp"
#include "core/SceneViewContext.hpp"
#include "variables/Resolver.hpp"
#include "variables/EventSnapshot.hpp"
#include "engine/SimulationChanges.hpp"
#include "engine/EffectGuards.hpp"
#include "engine/RouterContext.hpp"
namespace CustomComponents::Variables::QueuedTriggerValues {
using Obj=BNM::IL2CPP::Il2CppObject;
using Key=EventSnapshots::Key;
using KeyHash=EventSnapshots::KeyHash;
using Snapshot=EventSnapshots::Snapshot;
using Ptr=EventSnapshots::Ptr;
inline Ptr ForModel(Ptr snapshot,Obj* model){
    if(!snapshot || !model)return snapshot;
    auto token=Core::ModelRegistry::Identify(model,true);
    if(!token)return snapshot;
    if(token==snapshot->model)return snapshot;
    auto copy=std::make_shared<Snapshot>(*snapshot);copy->model=token;
    return copy;
}
inline Ptr ForInstances(Ptr snapshot,Obj* instances){
    if(!snapshot || !instances)return snapshot;
    static const auto type=BNM::Class("PvZCards.Engine","EntityModel");
    return ForModel(snapshot,type.IsValid()?Engine::Match::Provide(instances,type.GetMonoType()):nullptr);
}
inline std::mutex lock;
inline std::unordered_map<const void*,Ptr> byTrigger,byEffect;
inline std::atomic<unsigned long long> serial{0};
inline thread_local Obj* currentModel=nullptr;
inline thread_local Ptr active;
inline thread_local unsigned long long captureSerial=0;
inline thread_local std::string captureKind;
using QueueModelLookup=Obj*(*)(Obj*);
inline QueueModelLookup queueModelLookup=nullptr;
struct ActiveScope {Ptr previous=active;explicit ActiveScope(Ptr snapshot){active=std::move(snapshot);}~ActiveScope(){active=std::move(previous);}};
struct ModelScope {Obj* previous=currentModel;explicit ModelScope(Obj* model){currentModel=model;}~ModelScope(){currentModel=previous;}};

inline Ptr Lookup(const std::unordered_map<const void*,Ptr>& entries,const void* key) {
    auto it=entries.find(key);return it==entries.end()?Ptr{}:it->second;
}
// Outer optional distinguishes "not a frozen variable" from "frozen but absent".
inline std::optional<std::optional<Value>> Frozen(Obj* model,Obj* owner,std::string_view name) {
    if(!active || !model || !owner || !Name(name) ||
       (active->model && Core::ModelRegistry::Identify(model,false)!=active->model))return {};
    if(auto value=active->Read(Engine::Match::Id(owner),name))return value;
    if(Engine::CardStatValues::IsStat(name))return {};
    for(int id:ModelIndex(model).setters){auto* descriptor=reinterpret_cast<Descriptor*>(Engine::EntityAccess::Component(Find(model,id),"SetVariableEffectDescriptor"));
        if(descriptor && Text(descriptor->Variable)==name)return {}; // Derived continuous values remain live.
    }
    return std::optional<std::optional<Value>>(std::in_place,std::nullopt);
}
inline Ptr Capture(Obj* model){
    Core::Performance::Scope timing(Core::Performance::Area::VariableSnapshot);
    if(!model)return {};
    // A single game event can enqueue several stacked TriggerData objects
    // through multiple Add calls. They all see the same model revision until
    // resolution begins, so reuse one immutable snapshot for that revision.
    struct Cache {Core::ModelRegistry::Token model{};std::uint64_t revision=~std::uint64_t{};unsigned long long event=0;std::string kind;Ptr snapshot;};
    static thread_local Cache cache;
    const auto token=Core::ModelRegistry::Identify(model,true);
    const auto revision=Engine::SimulationChanges::Revision(model);
    const auto event=captureSerial?captureSerial:Engine::EffectGuards::current.event;
    if(cache.model==token && cache.revision==revision && cache.event==event && cache.kind==captureKind)return cache.snapshot;
    // Resolve owners while walking EntityModel.Entities. Calling Find(model,id)
    // for every owner would rescan the same list and make this path quadratic.
    auto api=Engine::EntityAccess::Api();
    Engine::EntityAccess::List* entities=nullptr;
    if(api.entities.IsValid())entities=api.entities[model].Get();
    if(!Engine::EntityAccess::Valid(entities))return {};
    auto frame=std::make_shared<Snapshot>();frame->model=token;frame->sequence=++serial;
    const bool hasContinuousSetters=!ModelIndex(model).setters.empty();
    for(int i=0;i<entities->size;++i){
        Obj* owner=entities->items->m_Items[i];
        if(!owner || !Engine::EntityAccess::Component(owner,"Variables"))continue;
        const int id=Engine::Match::Id(owner);
        if(id<0)continue;
        auto stored=Stored(owner);auto timed=Timed(owner);
        if(!stored || !timed)continue;
        Table effective=*stored;
        if(!ApplyTimed(effective,*timed))continue;
        const Key ownerKey{Engine::Match::Id(owner),{}};
        auto capture=[&](const std::string& name){
            Key key=ownerKey;key.second=name;
            if(frame->variables.find(key)!=frame->variables.end())return;
            std::optional<Value> value;
            if(hasContinuousSetters)value=Read(model,owner,name);
            else if(auto found=effective.find(name);found!=effective.end())value=found->second;
            frame->variables.emplace(std::move(key),std::move(value));
        };
        // Avoid a temporary unordered_set for every entity. Stored and timed
        // entries can overlap; map insertion naturally keeps the first value
        // and the snapshot still contains each variable only once.
        for(const auto& [name,value]:*stored){
            (void)value;
            capture(name);
        }
        for(const auto& row:*timed){
            capture(row.name);
        }
    }
    frame->Freeze();
    cache={token,revision,event,captureKind,frame};
    return frame;
}
// TriggerQueue is shared by stock and custom trigger markers. Observe accepted
// TriggerData here instead of detouring an open generic body at a fixed RVA.
static void BeforeEnqueue(Obj* collection,Obj* trigger){
    Obj* model=currentModel;
    if(!model && queueModelLookup)model=queueModelLookup(collection);
    if(!model){const auto& scope=Engine::EffectGuards::current;
        if(scope.router)model=Core::ModelRegistry::Get(Engine::RouterContext::RouterModel(scope.router));}
    // Begin() captured the completed pre-trigger state for this event. Reuse
    // that frame even if nested native work changes the live model revision.
    Ptr snapshot=trigger && model?(captureSerial && active && active->model==Core::ModelRegistry::Identify(model,false)?active:Capture(model)):Ptr{};
    // Each queued listener owns a chain. Writes by one listener must not alter
    // the condition snapshot of a different listener from the same event.
    if(snapshot)snapshot=std::make_shared<Snapshot>(*snapshot);
    if(trigger){std::lock_guard<std::mutex> guard(lock);
        if(snapshot)byTrigger[trigger]=snapshot;
        else byTrigger.erase(trigger);}
    static std::atomic<unsigned> logs{0};
    if(reinterpret_cast<std::uintptr_t>(model)==Core::SceneViewContext::ModelIdentity() && logs.fetch_add(1)<256)
        LOG_INFO("CustomComponents/TriggerSnapshots/QUEUED: sequence=%llu vars=%zu trigger=%p model=%p listener=%d source=%d context=%p",
            snapshot?snapshot->sequence:0,snapshot?snapshot->Size():0,trigger,
            model,trigger?Core::Metadata::Bind(Engine::TriggerQueue::listener,trigger).Get():-1,
            trigger?Core::Metadata::Bind(Engine::TriggerQueue::source,trigger).Get():-1,
            trigger?Core::Metadata::Bind(Engine::TriggerQueue::eventContext,trigger).Get():nullptr);
}
// Stock copies TriggerData while forming effects / cloning game state.
using TriggerCopyFn=Obj*(*)(void*,Obj*,const void*);
inline TriggerCopyFn originalTriggerCopy=nullptr;
static Obj* TriggerCopy(void* self,Obj* instances,const void* method){
    Obj* copy=originalTriggerCopy(self,instances,method);
    Ptr snapshot;{std::lock_guard<std::mutex> guard(lock);snapshot=Lookup(byTrigger,self);}
    snapshot=ForInstances(snapshot,instances);
    if(copy){std::lock_guard<std::mutex> guard(lock);
        if(snapshot)byTrigger[copy]=snapshot;else byTrigger.erase(copy);}
    return copy;
}
using TriggerCopyFromFn=void(*)(void*,Obj*,const void*);
inline TriggerCopyFromFn originalTriggerCopyFrom=nullptr;
static void TriggerCopyFrom(void* self,Obj* source,const void* method){
    originalTriggerCopyFrom(self,source,method);
    std::lock_guard<std::mutex> guard(lock);
    if(auto snapshot=Lookup(byTrigger,source))byTrigger[self]=snapshot;
    else byTrigger.erase(self);
}
using CreateEffectFn=Obj*(*)(void*,Obj*,const void*);
inline CreateEffectFn originalCreateEffect=nullptr;
// Pick3 attaches its provenance using this shared creation hook.
inline Core::Observers<Obj*,Obj*,Obj*> afterCreate;
static Obj* CreateEffect(void* self,Obj* trigger,const void* method){
    Ptr snapshot;{std::lock_guard<std::mutex> guard(lock);snapshot=Lookup(byTrigger,trigger);}
    ActiveScope creationSnapshot(snapshot);
    Obj* effect=originalCreateEffect(self,trigger,method);
    if(effect){std::lock_guard<std::mutex> guard(lock);
        if(snapshot)byEffect[effect]=snapshot;else byEffect.erase(effect);}
    afterCreate.Notify(static_cast<Obj*>(self),trigger,effect);
    static std::atomic<unsigned> createdLogs{0};
    if(snapshot && createdLogs.fetch_add(1)<192 &&
       reinterpret_cast<std::uintptr_t>(Core::ModelRegistry::Get(snapshot->model))==Core::SceneViewContext::ModelIdentity())
        LOG_INFO("CustomComponents/TriggerSnapshots/EFFECT_BOUND: trigger=%p effect=%p snapshot=%llu listener=%d",
            trigger,effect,snapshot->sequence,
            trigger?Core::Metadata::Bind(Engine::TriggerQueue::listener,trigger).Get():-1);
    return effect;
}
using EffectDeepCopyFn=void(*)(void*,Obj*,Obj*,const void*);
inline EffectDeepCopyFn originalEffectDeepCopy=nullptr;
static void EffectDeepCopy(void* self,Obj* source,Obj* instances,const void* method){
    originalEffectDeepCopy(self,source,instances,method);
    Engine::EffectLifecycleEvents::copied.Notify(source,static_cast<Obj*>(self));
    Ptr snapshot;{std::lock_guard<std::mutex> guard(lock);snapshot=Lookup(byEffect,source);}
    // Native derived copies tail-call the base implementation with unused
    // arguments elided. Its InstanceManager register can hold unrelated data.
    // Keep the captured values and let the resolver bind its actual model.
    const auto& context=Engine::EffectGuards::current;
    if(context.router)
        snapshot=ForModel(snapshot,Core::ModelRegistry::Get(Engine::RouterContext::RouterModel(context.router)));
    std::lock_guard<std::mutex> guard(lock);
    if(snapshot)byEffect[self]=snapshot;
    else byEffect.erase(self);
}
// DiscardedSystem owns the single TriggeredEffectResolverSystem.ProcessEffect
// hook. It calls this adapter around the original resolver. No second detour.
inline bool resolverOwnerConnected=false;
inline Obj* (*originalCreateEffects)(Obj*,Obj*,Obj*,const void*)=nullptr;
static Obj* CreateEffects(Obj* creator,Obj* entity,Obj* context,const void* method){
    Obj* result=originalCreateEffects(creator,entity,context,method);
    Engine::EffectCreationEvents::created.Notify(entity,context,result);
    auto* effects=reinterpret_cast<Engine::EntityAccess::List*>(result);
    if(active && Engine::EntityAccess::Valid(effects)){
        std::lock_guard<std::mutex> guard(lock);
        static thread_local Core::ModelRegistry::Token traceModel=0;
        static thread_local unsigned traceCount=0;
        if(traceModel!=active->model){traceModel=active->model;traceCount=0;}
        for(int i=0;i<effects->size;++i)if(Obj* effect=effects->items->m_Items[i]){
            byEffect[effect]=active;
            if(effect->klass && std::string_view(effect->klass->name)=="DrawCardAbilityEffect" &&
               reinterpret_cast<std::uintptr_t>(Core::ModelRegistry::Get(active->model))==Core::SceneViewContext::ModelIdentity() && traceCount++<64)
                LOG_INFO("CustomComponents/TriggerSnapshots/DRAW_CREATED: listener=%d snapshot=%llu runtime=%p",Engine::Match::Id(entity),active->sequence,effect);
        }
    }
    return result;
}
inline thread_local std::vector<Ptr> executing;
inline void BeginEffect(const Engine::EffectGuards::Context& context){
    executing.push_back(active);
    std::lock_guard<std::mutex> guard(lock);
    if(auto snapshot=Lookup(byEffect,context.effect))active=std::move(snapshot);
}
inline void EndEffect(const Engine::EffectGuards::Context&){if(!executing.empty()){active=std::move(executing.back());executing.pop_back();}}
inline void CommitVariable(Obj* model,Obj* owner,std::string_view name){
    if(!active || active->model!=Core::ModelRegistry::Identify(model,false))return;
    const auto value=Read(model,owner,name);
    const int id=Engine::Match::Id(owner);
    active->Assign(id,name,value);
}
inline void (*originalEffectReturn)(Obj*,Obj*,const void*)=nullptr;
inline void (*originalTriggerReturn)(Obj*,Obj*,const void*)=nullptr;
inline void TriggerReturn(Obj* trigger,Obj* instances,const void* method){
    {std::lock_guard<std::mutex> guard(lock);byTrigger.erase(trigger);}
    originalTriggerReturn(trigger,instances,method);
}
inline void EffectReturn(Obj* effect,Obj* instances,const void* method){
    {std::lock_guard<std::mutex> guard(lock);byEffect.erase(effect);}
    Engine::EffectLifecycleEvents::returned.Notify(effect);
    originalEffectReturn(effect,instances,method);
}
template<class Action> inline void DuringResolve(Obj* effect,Action&& action){
    Ptr snapshot;{std::lock_guard<std::mutex> guard(lock);snapshot=Lookup(byEffect,effect);}
    ActiveScope scope(snapshot);
    static std::atomic<unsigned> logs{0};
    if(snapshot && reinterpret_cast<std::uintptr_t>(Core::ModelRegistry::Get(snapshot->model))==Core::SceneViewContext::ModelIdentity() && logs.fetch_add(1)<256)
        LOG_INFO("CustomComponents/TriggerSnapshots/RESOLVE: effect=%p snapshot=%llu listenerState=%p",effect,snapshot->sequence,snapshot.get());
    std::forward<Action>(action)();
    // A resolver pass may suspend for a target choice. Keep the event's
    // snapshot until pool return, rather than losing it before the resume.
}
inline void Inherit(Obj* source,Obj* target){
    if(!target)return;Ptr snapshot;
    {std::lock_guard<std::mutex> guard(lock);snapshot=Lookup(byEffect,source);}
    if(!snapshot)snapshot=active;
    if(snapshot){std::lock_guard<std::mutex> guard(lock);byEffect[target]=std::move(snapshot);}
}
inline void (*originalDrawIds)(Obj*,Obj*,Obj*,const void*)=nullptr;
static void DrawIds(Obj* self,Obj* source,Obj* child,const void* method){originalDrawIds(self,source,child,method);Inherit(source,child);}
inline bool Initialize(){
    eventVariable=Frozen;
    Engine::EffectGuards::beforeValidation.Add(BeginEffect);
    Engine::EffectGuards::afterValidation.Add(EndEffect);
    Engine::EffectQueue::Initialize();
    Engine::EffectQueue::beforePush.Add([](Obj*,Obj* effect,int,Obj*){
        if(!effect || !active)return;
        std::lock_guard<std::mutex> guard(lock);if(!byEffect.contains(effect))byEffect[effect]=active;
    });
    if(!Core::Hooks::Install(BNM::Class("PvZCards.Core.Systems","DrawCardAbilitySystem").GetMethod("CopyDefaultIds",2),DrawIds,originalDrawIds)){
        LOG_ERR("CustomComponents/Variables: draw continuation identity hook failed");return false;
    }
    onVariableOwnerWritten=RememberVariableOwner;
    onVariableAssigned=CommitVariable;
    Core::ModelRegistry::retired.Add([](auto token){std::lock_guard<std::mutex> guard(lock);
        std::erase_if(byTrigger,[&](const auto& entry){return entry.second && entry.second->model==token;});
        std::erase_if(byEffect,[&](const auto& entry){return entry.second && entry.second->model==token;});});
    auto type=BNM::Class("PvZCards.Core.Systems","TriggeredEffectQueuingSystem");
    auto trigger=BNM::Class("PvZCards.Engine","TriggerData");
    auto effect=BNM::Class("PvZCards.Engine","Effect");
    // Mandatory hooks. The resolver owner is registered in startup stage 3.
    // Do not attempt to install a competing detour on that method.
    const bool ready=Engine::TriggerQueue::Initialize() &&
        Engine::TriggerQueue::beforeEnqueue.Add(BeforeEnqueue) &&
        Core::Hooks::Install(trigger.GetMethod("Copy",1),TriggerCopy,originalTriggerCopy) &&
        Core::Hooks::Install(trigger.GetMethod("CopyFrom",1),TriggerCopyFrom,originalTriggerCopyFrom) &&
        Core::Hooks::Install(trigger.GetMethod("HandleReturnToPool",1),TriggerReturn,originalTriggerReturn) &&
        Core::Hooks::Install(type.GetMethod("CreateTriggeredEffectFrom",1),CreateEffect,originalCreateEffect) &&
        Core::Hooks::Install(effect.GetMethod("DeepCopyFrom",2),EffectDeepCopy,originalEffectDeepCopy) &&
        Core::Hooks::Install(effect.GetMethod("HandleReturnToPool",1),EffectReturn,originalEffectReturn) &&
        Core::Hooks::Install(BNM::Class("PvZCards.Core.Systems.Utilities","EffectCreator").GetMethod("CreateEffects",2),CreateEffects,originalCreateEffects) &&
        Engine::EffectGuards::before.Add(BeginEffect) && Engine::EffectGuards::after.Add(EndEffect) &&
        resolverOwnerConnected;
    LOG_INFO("CustomComponents/TriggerSnapshots/INIT: allTriggerTypes=%d enqueue=%d copy=%d create=%d resolve=%d",int(ready),int(Engine::TriggerQueue::originalAdd!=nullptr),int(originalTriggerCopy!=nullptr),int(originalCreateEffect!=nullptr),int(resolverOwnerConnected));
    return ready;
}
}
