#pragma once
#include "history/CustomHistory.hpp"
namespace CustomComponents::History {
// A stock cursor is paused at a boundary, not consumed or rewound.
inline std::map<std::uintptr_t,std::pair<int,std::uintptr_t>> presentationWaiters;
inline bool BlocksStockLocked(std::uintptr_t model,int cursor,std::uintptr_t history){
    for(const auto& record:records)
        if(record.model==model && (!history || !record.history || record.history==history) &&
           Required(record) && !Terminal(record.delivery) &&
           record.stockHistoryAnchor>=0 && record.stockHistoryAnchor<=cursor)return true;
    return false;
}
inline bool WaitForPresentation(std::uintptr_t model,int cursor,std::uintptr_t history){
    std::lock_guard<std::mutex> guard(mutex);
    if(!BlocksStockLocked(model,cursor,history))return false;
    presentationWaiters[model]={cursor,history};return true;
}
inline bool TakePresentationResume(std::uintptr_t model,std::uintptr_t history){
    std::lock_guard<std::mutex> guard(mutex);
    auto found=presentationWaiters.find(model);if(found==presentationWaiters.end())return false;
    if(found->second.second!=history){presentationWaiters.erase(found);return false;}
    if(BlocksStockLocked(model,found->second.first,history))return false;
    presentationWaiters.erase(found);return true;
}
inline void RetirePresentationModel(std::uint64_t token){
    std::lock_guard<std::mutex> guard(mutex);
    for(const auto& [model,binding]:bindings)if(binding.token==token)presentationWaiters.erase(model);
}
}
