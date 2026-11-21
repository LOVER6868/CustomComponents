#pragma once
#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>
#include "variables/Values.hpp"

namespace CustomComponents::Variables::EventSnapshots {
using Key=std::pair<int,std::string>;
struct KeyHash {
    std::size_t operator()(const Key& key)const noexcept{
        const auto owner=std::hash<int>{}(key.first),name=std::hash<std::string>{}(key.second);
        return owner^(name+0x9e3779b9u+(owner<<6)+(owner>>2));
    }
};
// A queued event owns its baseline plus writes made by its own effect chain.
// Prediction copies own independent values; there are no managed references.
struct Snapshot {
    mutable std::mutex mutex;
    std::uint64_t model=0;
    using Table=std::unordered_map<Key,std::optional<Value>,KeyHash>;
    Table variables; // listener-local overlay, including explicit absent values
    std::shared_ptr<const Table> baseline;
    void Freeze(){std::lock_guard<std::mutex> guard(mutex);
        if(!baseline){baseline=std::make_shared<const Table>(std::move(variables));variables.clear();}}
    std::size_t Size()const{std::lock_guard<std::mutex> guard(mutex);
        auto count=baseline?baseline->size():0;
        for(const auto& entry:variables)if(!baseline || !baseline->contains(entry.first))++count;
        return count;
    }
    unsigned long long sequence=0;
    Snapshot()=default;
    Snapshot(const Snapshot& source){std::lock_guard<std::mutex> guard(source.mutex);model=source.model;baseline=source.baseline;variables=source.variables;sequence=source.sequence;}
    std::optional<std::optional<Value>> Read(int owner,std::string_view name)const{
        std::lock_guard<std::mutex> guard(mutex);auto it=variables.find({owner,std::string(name)});
        if(it!=variables.end())return std::optional<std::optional<Value>>(std::in_place,it->second);
        if(baseline){auto found=baseline->find({owner,std::string(name)});
            if(found!=baseline->end())return std::optional<std::optional<Value>>(std::in_place,found->second);}
        return {};
    }
    void Assign(int owner,std::string_view name,std::optional<Value> value){std::lock_guard<std::mutex> guard(mutex);variables[{owner,std::string(name)}]=std::move(value);}
};
using Ptr=std::shared_ptr<Snapshot>;
}
