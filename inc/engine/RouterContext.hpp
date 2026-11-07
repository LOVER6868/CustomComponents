#pragma once
#include "core/ModelRegistry.hpp"
namespace CustomComponents::Engine::RouterContext {
using Obj=Core::ManagedRuntime::Obj;
struct RouterRoot { std::shared_ptr<Core::ManagedRuntime::Root> reference; Core::ModelRegistry::Token model; };
inline std::mutex rootsLock;
inline std::vector<RouterRoot> routers;
inline void RegisterRouter(Obj* router,Obj* model) {
    const auto token=Core::ModelRegistry::Identify(model,true);
    if(!router || !token) return;
    std::lock_guard<std::mutex> guard(rootsLock);
    for(auto it=routers.begin();it!=routers.end();) {
        auto* object=it->reference->Get();
        if(!object) it=routers.erase(it);
        else { if(object==router) {it->model=token;return;} ++it; }
    }
    auto reference=std::make_shared<Core::ManagedRuntime::Root>(router,true);
    if(reference->Get()) routers.push_back({reference,token});
}
inline Core::ModelRegistry::Token RouterModel(Obj* router) {
    std::lock_guard<std::mutex> guard(rootsLock);
    for(const auto& entry:routers) if(entry.reference->Get()==router) return entry.model;
    return 0;
}
}
