#pragma once
#include <algorithm>
#include <mutex>
#include <vector>
#include "core/ManagedRuntime.hpp"
namespace CustomComponents::Core {
// Sidecars for extensions of stock objects: never resize a managed class.
// Owners must erase on pool return and copy at their deep-copy boundary.
template<class Value> class WeakObjectSettings {
    struct Entry {std::shared_ptr<ManagedRuntime::Root> key;Value value;};
    std::mutex mutex;
    std::vector<Entry> entries;
public:
    Value Get(ManagedRuntime::Obj* object) {
        if(!object)return {};
        std::lock_guard<std::mutex> lock(mutex);
        for(const auto& entry:entries)if(entry.key->Get()==object)return entry.value;
        return {};
    }
    void Set(ManagedRuntime::Obj* object,Value value,bool retain=true) {
        if(!object)return;
        std::lock_guard<std::mutex> lock(mutex);
        entries.erase(std::remove_if(entries.begin(),entries.end(),[&](const auto& entry){auto* p=entry.key->Get();return !p || p==object;}),entries.end());
        if(!retain)return;
        auto root=std::make_shared<ManagedRuntime::Root>(object,true);
        if(root->Get())entries.push_back({root,value});
    }
};
}
