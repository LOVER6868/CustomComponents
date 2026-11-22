#pragma once
#include <atomic>
#include <chrono>
#include <mutex>
#include "log.h"
#include "engine/CustomRecords.hpp"
namespace CustomComponents::Diagnostics::HistoryTrace {
inline bool enabled=false;
struct Budget {std::mutex mutex;std::chrono::steady_clock::time_point window=std::chrono::steady_clock::now();unsigned lines=0,dropped=0;};
inline Budget lifecycleBudget,queryBudget;
inline bool Allow(bool detail=false){if(!enabled)return false;
    auto& budget=detail?queryBudget:lifecycleBudget;
    std::lock_guard<std::mutex> guard(budget.mutex);
    const auto now=std::chrono::steady_clock::now();
    if(now-budget.window>=std::chrono::seconds(3)){
        if(budget.dropped)LOG_INFO("CustomHistory/TRACE_SUPPRESSED: detail=%d lines=%u",int(detail),budget.dropped);
        budget.lines=0;budget.dropped=0;budget.window=now;
    }
    if(budget.lines++<(detail?48u:512u))return true;
    ++budget.dropped;return false;
}
inline long long Micros(){return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
inline void Record(const char* stage,const Engine::CustomRecords::Record& r,int cursor){
    if(Allow())LOG_INFO("CustomHistory/%s: us=%lld serial=%llu model=%p boundary=%d cursor=%d kind=%u card=%d owner=%d effect=%d from=%d to=%d delivery=%u",
        stage,Micros(),(unsigned long long)r.serial,reinterpret_cast<void*>(r.model),r.stockHistoryAnchor,cursor,
        unsigned(r.kind),r.entityId,r.ownerId,r.effectId,r.fromZoneId,r.toZoneId,unsigned(r.delivery));
}
inline void Animation(const char* stage,void* queue,void* animation,const char* type,int card=-1){
    if(Allow())LOG_INFO("CustomHistory/%s: us=%lld queue=%p animation=%p type=%s card=%d",stage,Micros(),queue,animation,type?type:"unknown",card);
}
}
