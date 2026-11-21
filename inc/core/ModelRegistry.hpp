#pragma once
#include <cstdint>
#include <mutex>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include "core/ManagedRuntime.hpp"
#include "core/Observers.hpp"

namespace CustomComponents::Core::ModelRegistry {
using Obj=ManagedRuntime::Obj;
using Token=std::uint64_t;
struct Entry { Token token; std::shared_ptr<ManagedRuntime::Root> reference; Obj* address=nullptr; };
inline std::mutex lock;
inline std::vector<Entry> entries;
inline std::unordered_map<Token,std::shared_ptr<ManagedRuntime::Root>> byToken;
inline std::unordered_map<Obj*,Token> byObject;
inline Token nextToken=1;
inline Observers<Token> retired;
inline Token Identify(Obj* model,bool create=false) {
    if(!model || !ManagedRuntime::newGCHandle || !ManagedRuntime::getGCHandle) return 0;
    std::vector<Token> expired;
    Token result=0;
    {
        std::lock_guard<std::mutex> guard(lock);
        if(auto found=byObject.find(model);found!=byObject.end()){
            auto root=byToken.find(found->second);
            if(root!=byToken.end() && root->second->Get()==model)return found->second;
        }
        for(auto it=entries.begin();it!=entries.end();) {
            auto* object=it->reference->Get();
            if(!object) { expired.push_back(it->token);byToken.erase(it->token);byObject.erase(it->address);it=entries.erase(it); }
            else { if(object==model){result=it->token;byObject[model]=result;} ++it; }
        }
        if(!result && create) {
            auto reference=std::make_shared<ManagedRuntime::Root>(model,true);
            if(reference->Get()) { result=nextToken++;entries.push_back({result,reference,model});byToken.emplace(result,reference);byObject[model]=result; }
        }
    }
    for(auto token:expired) retired.Notify(token);
    return result;
}
inline Obj* Get(Token token) {
    std::lock_guard<std::mutex> guard(lock);
    auto it=byToken.find(token);
    return it==byToken.end()?nullptr:it->second->Get();
}
inline void Reset() {
    std::vector<Entry> prior;
    { std::lock_guard<std::mutex> guard(lock);prior.swap(entries);byToken.clear();byObject.clear(); }
    for(const auto& entry:prior) retired.Notify(entry.token);
}
}
