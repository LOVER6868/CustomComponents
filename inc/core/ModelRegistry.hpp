#pragma once
#include <cstdint>
#include <mutex>
#include <vector>
#include "core/ManagedRuntime.hpp"
#include "core/Observers.hpp"

namespace CustomComponents::Core::ModelRegistry {
using Obj=ManagedRuntime::Obj;
using Token=std::uint64_t;
struct Entry { Token token; std::shared_ptr<ManagedRuntime::Root> reference; };
inline std::mutex lock;
inline std::vector<Entry> entries;
inline Token nextToken=1;
inline Observers<Token> retired;
inline Token Identify(Obj* model,bool create=false) {
    if(!model || !ManagedRuntime::newGCHandle || !ManagedRuntime::getGCHandle) return 0;
    std::vector<Token> expired;
    Token result=0;
    {
        std::lock_guard<std::mutex> guard(lock);
        for(auto it=entries.begin();it!=entries.end();) {
            auto* object=it->reference->Get();
            if(!object) { expired.push_back(it->token);it=entries.erase(it); }
            else { if(object==model) result=it->token; ++it; }
        }
        if(!result && create) {
            auto reference=std::make_shared<ManagedRuntime::Root>(model,true);
            if(reference->Get()) { result=nextToken++;entries.push_back({result,reference}); }
        }
    }
    for(auto token:expired) retired.Notify(token);
    return result;
}
inline Obj* Get(Token token) {
    std::lock_guard<std::mutex> guard(lock);
    for(const auto& entry:entries) if(entry.token==token) return entry.reference->Get();
    return nullptr;
}
inline void Reset() {
    std::vector<Entry> prior;
    { std::lock_guard<std::mutex> guard(lock);prior.swap(entries); }
    for(const auto& entry:prior) retired.Notify(entry.token);
}
}
