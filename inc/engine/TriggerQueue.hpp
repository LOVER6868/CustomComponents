#pragma once
#include <BNM/Class.hpp>
#include <memory>
#include <mutex>
#include <vector>
#include "core/HookRegistry.hpp"
#include "core/Observers.hpp"
#include "core/EnqueuePipeline.hpp"

namespace CustomComponents::Engine::TriggerQueue {
using Obj=BNM::IL2CPP::Il2CppObject;
struct Context { Obj* collection; Obj* data; };
using Filter=bool(*)(const Context&); // true requests suppression; does not recycle
using Recycle=bool(*)(const Context&);
inline std::mutex filtersLock;
inline Core::AtomicSnapshot<std::vector<Filter>> filters{std::make_shared<const std::vector<Filter>>()};
inline Recycle recycle=nullptr;
inline Core::Observers<Obj*,Obj*> beforeEnqueue;
inline void(*originalAdd)(Obj*,Obj*,const void*)=nullptr;
inline bool AddFilter(Filter filter) {
    if(!filter) return false;
    std::lock_guard<std::mutex> guard(filtersLock);
    auto prior=filters.load();
    if(std::find(prior->begin(),prior->end(),filter)!=prior->end()) return true;
    auto next=std::make_shared<std::vector<Filter>>(*prior);next->push_back(filter);
    filters.store(std::shared_ptr<const std::vector<Filter>>(next));return true;
}
static void AddHook(Obj* collection,Obj* data,const void* method) {
    const Context context{collection,data};
    const auto result=Core::Enqueue(context,*filters.load(),
        [](const Context& value){return recycle && recycle(value);},
        [](const Context& value){beforeEnqueue.Notify(value.collection,value.data);},
        [&](const Context& value){originalAdd(value.collection,value.data,method);});
    if(result==Core::EnqueueResult::RecycleUnavailable)
        LOG_ERR("CustomComponents/TriggerQueue: suppression could not resolve owning pool; delegating unchanged");
}
inline bool Initialize() {
    auto add=BNM::Class("PvZCards.Engine","TriggeredEffectCollection").GetMethod("Add",1);
    return Core::Hooks::Install(add,AddHook,originalAdd);
}
}
