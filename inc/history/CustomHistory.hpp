#pragma once
// Native value history. Simulation never calls frontend processors or observers.
#include <algorithm>
#include <deque>
#include <map>
#include <limits>
#include <set>
#include <chrono>
#include <mutex>
#include <vector>
#include "history/RecordTypes.hpp"
#include "history/RecordContracts.hpp"
namespace CustomComponents::History {
using Processor=bool(*)(const Record&);using Observer=void(*)(const Event&);
struct Handler {Kind kind;Processor processor;};
struct Binding {std::uint64_t epoch=0,key=0,generation=1,token=0;std::uintptr_t history=0,scene=0;};
inline std::mutex mutex;
inline std::deque<Record> records;
inline std::deque<Event> journal;
inline std::map<std::uintptr_t,Binding> bindings;
inline std::set<std::uintptr_t> pendingModels;
inline std::vector<Handler> handlers;
struct Subscription {Observer observer;std::uint64_t cursor;};
inline std::vector<Subscription> observers;
inline constexpr std::size_t kMaxRecords=4096,kJournalCapacity=32768;
inline std::uint64_t nextSerial=0,nextEpoch=0,nextModelKey=0,nextEvent=0,dropped=0;
inline std::uintptr_t activeModel=0;
// Diagnostic sink may not reenter storage. Recorder observers use the journal.
inline void (*trace)(const char*,const Record&,int)=nullptr;
inline void Trace(const char* label,const Record& r,int cursor=-1){if(trace)trace(label,r,cursor);}
inline void PublishLocked(const Record& r){const auto now=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();journal.push_back({++nextEvent,r,std::uint64_t(now)});if(journal.size()>kJournalCapacity)journal.pop_front();}
inline void CancelLocked(std::uintptr_t model,Reason reason){for(auto& r:records)if(r.model==model && !Terminal(r.delivery)){r.delivery=Delivery::Cancelled;r.reason=reason;PublishLocked(r);Trace("CANCELLED",r);}pendingModels.erase(model);}
inline Binding& BindLocked(std::uintptr_t model,std::uintptr_t history=0,std::uint64_t token=0){
    auto& b=bindings[model];
    if(!b.epoch || (token && b.token && b.token!=token)){if(b.epoch)CancelLocked(model,Reason::ModelRetired);b={++nextEpoch,++nextModelKey,1,token,history,0};}
    if(token)b.token=token;
    if(history && b.history && history!=b.history){CancelLocked(model,Reason::HistoryReplaced);++b.generation;}
    if(history)b.history=history;return b;
}
inline void BindHistory(std::uintptr_t model,std::uintptr_t history){if(!model)return;std::lock_guard<std::mutex> guard(mutex);BindLocked(model,history);}
inline void ActivateScene(std::uintptr_t model,std::uintptr_t scene){
    std::lock_guard<std::mutex> guard(mutex);
    if(activeModel && activeModel!=model)CancelLocked(activeModel,Reason::SceneEnded);
    activeModel=model;if(!model)return;auto& b=BindLocked(model);
    if(b.scene && b.scene!=scene){CancelLocked(model,Reason::SceneEnded);b.epoch=++nextEpoch;}b.scene=scene;
}
inline bool Register(Kind kind,Processor fn){if(!fn)return false;std::lock_guard<std::mutex> guard(mutex);for(auto h:handlers)if(h.kind==kind)return h.processor==fn;handlers.push_back({kind,fn});return true;}
inline std::uint64_t Commit(Record r){
    if(!r.model || (r.stage!=Stage::Committed && r.stage!=Stage::PartialFailure)){Trace("REJECT",r);return 0;}
    if(!ValidPayload(r)){Trace("INVALID_PAYLOAD",r);return 0;}
    std::lock_guard<std::mutex> guard(mutex);auto& b=BindLocked(r.model,r.history,r.modelToken);
    r.epoch=b.epoch;r.modelKey=b.key;r.historyGeneration=b.generation;if(!r.history)r.history=b.history;
    r.serial=++nextSerial;if(!r.correlationId)r.correlationId=r.serial;r.delivery=r.visual==VisualPolicy::NoVisual?Delivery::SkippedNoVisual:Delivery::Pending;
    if(r.kind==Kind::DiscardOutsidePlay && std::holds_alternative<std::monostate>(r.payload))r.payload=DiscardPayload{r.entityId,r.ownerId,r.fromZoneId,r.toZoneId,false};
    // Bound completed diagnostics, never evict undelivered functional work.
    while(records.size()>=kMaxRecords){auto it=std::find_if(records.begin(),records.end(),[](const auto& x){return Terminal(x.delivery);});if(it==records.end())break;records.erase(it);++dropped;}
    if(r.delivery==Delivery::Pending)pendingModels.insert(r.model);
    records.push_back(r);PublishLocked(r);Trace("COMMIT",r);return r.serial;
}
inline bool Transition(std::uint64_t id,Delivery state,Reason reason=Reason::None){
    std::lock_guard<std::mutex> guard(mutex);for(auto& r:records)if(r.serial==id){
        if(Terminal(r.delivery))return r.delivery==state;
        if(state==Delivery::Started && r.delivery!=Delivery::Processed)return false;
        if(state==Delivery::VisualComplete && r.delivery!=Delivery::Processed && r.delivery!=Delivery::Started)return false;
        r.delivery=state;r.reason=reason;PublishLocked(r);Trace(state==Delivery::VisualComplete?"COMPLETE":state==Delivery::Started?"STARTED":state==Delivery::Failed?"FAILED":"CANCELLED",r);return true;
    }return false;
}
inline bool MarkVisualComplete(std::uint64_t id){return Transition(id,Delivery::VisualComplete);}
inline bool MarkStarted(std::uint64_t id){return Transition(id,Delivery::Started);}
inline bool Fail(std::uint64_t id,Reason reason){return Transition(id,Delivery::Failed,reason);}
inline bool Cancel(std::uint64_t id,Reason reason){return Transition(id,Delivery::Cancelled,reason);}
inline bool HasRecord(std::uint64_t id){std::lock_guard<std::mutex> guard(mutex);return std::any_of(records.begin(),records.end(),[&](auto& r){return r.serial==id;});}
inline bool IsTerminal(std::uint64_t id){std::lock_guard<std::mutex> guard(mutex);for(auto& r:records)if(r.serial==id)return Terminal(r.delivery);return true;}
// Enqueue before the stock entry at position; visuals follow stock queue lifecycle.
inline std::size_t ProcessReady(std::uintptr_t model,int position,int count,std::uintptr_t history=0){
    if(!model || position<0 || count<0)return 0;
    {std::lock_guard<std::mutex> guard(mutex);if(!pendingModels.contains(model))return 0;}
    static thread_local bool dispatching=false;if(dispatching)return 0;
    struct Guard {bool& v;Guard(bool& x):v(x){v=true;}~Guard(){v=false;}} guard(dispatching);
    std::size_t done=0;
    for(;;){Record r;Processor processor=nullptr;
        {std::lock_guard<std::mutex> lock(mutex);auto& b=BindLocked(model,history);auto it=records.end();
            for(auto candidate=records.begin();candidate!=records.end();++candidate){
                if(candidate->model!=model || candidate->epoch!=b.epoch || candidate->historyGeneration!=b.generation || candidate->delivery!=Delivery::Pending)continue;
                if(candidate->history && history && candidate->history!=history)continue;
                const auto boundary=[](const Record& r){return r.stockHistoryAnchor<0?std::numeric_limits<int>::max():r.stockHistoryAnchor;};
                if(it==records.end() || std::pair{boundary(*candidate),candidate->serial}<std::pair{boundary(*it),it->serial})it=candidate;
            }
            if(it==records.end()){pendingModels.erase(model);break;}if(it->stockHistoryAnchor>=0?position<it->stockHistoryAnchor:position<count)break;
            for(auto h:handlers)if(h.kind==it->kind)processor=h.processor;
            if(Required(*it) && !processor){Trace("MISSING_PROCESSOR",*it,position);break;}
            it->delivery=Delivery::Processing;r=*it;
        }
        Trace("DISPATCH",r,position);const bool ok=processor?processor(r):true;
        {std::lock_guard<std::mutex> lock(mutex);auto it=std::find_if(records.begin(),records.end(),[&](auto& x){return x.serial==r.serial;});if(it==records.end())break;
            if(it->delivery==Delivery::Processing){it->delivery=ok?(Required(*it)?Delivery::Processed:Delivery::SkippedNoVisual):Delivery::Pending;PublishLocked(*it);}Trace(ok?"ACCEPTED":"RETRY",*it,position);
        }
        if(!ok)break;++done;
    }return done;
}
inline std::vector<Record> Snapshot(std::uint64_t after=0){std::lock_guard<std::mutex> guard(mutex);std::vector<Record> out;for(auto& r:records)if(r.serial>after)out.push_back(r);return out;}
struct EventBatch {std::vector<Event> events;std::uint64_t lastSequence=0,lostBeforeRead=0;};
inline std::uint64_t CurrentSequence(){std::lock_guard<std::mutex> guard(mutex);return nextEvent;}
inline EventBatch ReadAfter(std::uint64_t cursor){std::lock_guard<std::mutex> guard(mutex);EventBatch out;out.lastSequence=nextEvent;if(journal.empty() || cursor>=nextEvent)return out;const auto first=journal.front().sequence;if(cursor+1<first)out.lostBeforeRead=first-cursor-1;for(auto seq=std::max(cursor+1,first);seq<=nextEvent;++seq)out.events.push_back(journal[std::size_t(seq-first)]);return out;}
inline bool Subscribe(Observer fn){if(!fn)return false;std::lock_guard<std::mutex> guard(mutex);for(auto s:observers)if(s.observer==fn)return true;observers.push_back({fn,nextEvent});return true;}
inline void Unsubscribe(Observer fn){std::lock_guard<std::mutex> guard(mutex);std::erase_if(observers,[&](auto& s){return s.observer==fn;});}
inline void PumpObservers(){std::vector<Subscription> subscriptions;{std::lock_guard<std::mutex> guard(mutex);subscriptions=observers;}for(auto s:subscriptions){auto batch=ReadAfter(s.cursor);for(const auto& e:batch.events)s.observer(e);std::lock_guard<std::mutex> guard(mutex);for(auto& live:observers)if(live.observer==s.observer)live.cursor=batch.lastSequence;}}
inline void RetireModel(std::uint64_t token){std::lock_guard<std::mutex> guard(mutex);for(auto& [model,b]:bindings)if(b.token==token)CancelLocked(model,Reason::ModelRetired);}
inline void ResetScene(std::uintptr_t model=0){std::lock_guard<std::mutex> guard(mutex);if(activeModel)CancelLocked(activeModel,Reason::SceneEnded);activeModel=model;if(model)BindLocked(model);}
}
