#pragma once
#include <array>
#include <algorithm>
#include <chrono>
#include <mutex>
#include "log.h"
namespace CustomComponents::Core::Performance {
enum class Area {EntityCopy,StockEntityCopy,StockQueue,FrameReconciliation,EffectRouter,StockEffectRouter,SubsetFinder,CreationBinding,TargetFinder,AbilityExpiry,KeywordExpiry,Count};
inline bool enabled=false;
inline const char* names[]={"entity-copy-total","entity-copy-stock","animation-queue-stock","frame-reconciliation","effect-router-total","effect-router-stock","subset-finder","creation-binding","target-finder","ability-expiry","keyword-expiry"};
struct Sample {unsigned calls=0;double sum=0,maximum=0,selfSum=0,selfMaximum=0;};
inline std::array<Sample,static_cast<unsigned>(Area::Count)> samples;
inline std::mutex mutex;
inline unsigned slowReports=0;
inline auto lastReport=std::chrono::steady_clock::now();
struct Scope;
inline thread_local Scope* current=nullptr;
struct Scope {
    Area area;std::chrono::steady_clock::time_point start;bool active;
    Scope* parent=nullptr;double children=0;const char* label;int effect,target;
    explicit Scope(Area value,const char* detail="none",int effectId=-1,int targetId=-1)
        :area(value),active(enabled),label(detail),effect(effectId),target(targetId) {
        if(active){start=std::chrono::steady_clock::now();parent=current;current=this;}
    }
    Scope(const Scope&)=delete;
    Scope& operator=(const Scope&)=delete;
    ~Scope(){
        if(!active)return;
        const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
        const double own=std::max(0.0,ms-children);
        current=parent;if(parent)parent->children+=ms;
        bool report=false;
        {
            std::lock_guard<std::mutex> lock(mutex);auto& s=samples[static_cast<unsigned>(area)];
            ++s.calls;s.sum+=ms;s.maximum=std::max(s.maximum,ms);s.selfSum+=own;s.selfMaximum=std::max(s.selfMaximum,own);
            if(ms>=16.0 && slowReports<64){++slowReports;report=true;}
        }
        if(report)LOG_INFO("CustomComponents/PerformanceSlow: area=%s label=%s effect=%d target=%d totalMs=%.3f selfMs=%.3f nestedMs=%.3f",names[static_cast<unsigned>(area)],label?label:"none",effect,target,ms,own,children);
    }
};
inline void Report() {
    if(!enabled)return;const auto now=std::chrono::steady_clock::now();std::lock_guard<std::mutex> lock(mutex);
    if(now-lastReport<std::chrono::seconds(3))return;lastReport=now;
    for(unsigned i=0;i<samples.size();++i){auto& s=samples[i];if(s.calls)LOG_INFO("CustomComponents/Performance: area=%s calls=%u totalMs=%.3f maxMs=%.3f avgMs=%.3f selfTotalMs=%.3f selfMaxMs=%.3f",names[i],s.calls,s.sum,s.maximum,s.sum/s.calls,s.selfSum,s.selfMaximum);s={};}
}
}
