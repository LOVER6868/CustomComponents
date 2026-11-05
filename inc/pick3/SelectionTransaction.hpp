#pragma once
// 2.16: ID-only provenance. Runtime adapters own model/queue GC handles.
// Stock acceptance and completed model commit are intentionally distinct.
#include <memory>
#include <atomic>
#include <mutex>
#include <vector>
#include <algorithm>
#include "pick3/PickedFromThreeSelection.hpp"
#ifndef CUSTOMCOMPONENTS_PF3_TRANSACTION_PIPELINE
#define CUSTOMCOMPONENTS_PF3_TRANSACTION_PIPELINE 1
#endif
#ifndef CUSTOMCOMPONENTS_PF3_TRANSACTION_ROUTING
#define CUSTOMCOMPONENTS_PF3_TRANSACTION_ROUTING 1
#endif
namespace CustomComponents::Pick3::Transaction {
using Model = std::uint64_t;
struct Bindings {
    std::mutex lock;
    std::vector<int> produced;
    bool cancelled=false;
};
struct Value {
    Model model=0;
    std::uint64_t epoch=0, sequence=0;
    int effect=-1, group=-1, source=-1;
    std::vector<Picked::SelectedCard> cards;
    std::shared_ptr<Bindings> bindings;
};
using Ref=std::shared_ptr<const Value>;
inline std::mutex lock;
inline std::atomic<std::uint64_t> epoch{1};
inline constexpr std::size_t capacity=1024;
struct EventKey { Model model=0; std::uint64_t sequence=0; };
inline std::vector<EventKey> emitted;
inline std::vector<Ref> values;
struct Context {
    int effect=-1, source=-1, target=-1, springboard=-1;
    bool operator==(const Context&) const = default;
};
struct Listener { Model model; int effect,source; Context event; Ref value; std::uint64_t instance=0; };
inline std::vector<Listener> listeners;
struct EffectKey {
    Model model=0;
    int effect=-1,group=-1,source=-1,target=-1;
    std::uint64_t instance=0; // runtime GC-handle ticket, never a pooled address
    bool operator==(const EffectKey&) const = default;
};
struct Queued { EffectKey key; Ref value; };
inline std::vector<Queued> queued;
struct Active { Ref value; int effect=-1, contextEffect=-1; };
inline thread_local Active active;
inline bool Current(const Ref& r) { return r && r->epoch==epoch && r->model && r->sequence; }
struct Scoped {
    Active previous;
    explicit Scoped(Ref r={},int effect=-1,int contextEffect=-1):previous(active) {
        active={Current(r)?std::move(r):Ref{},effect,contextEffect};
    }
    ~Scoped(){ active=previous; }
    Scoped(const Scoped&)=delete;
    Scoped& operator=(const Scoped&)=delete;
};
inline Ref Find(Model model,int effect,int group,std::uint64_t sequence) {
    std::lock_guard<std::mutex> guard(lock);
    for(const auto& r:values) if(Current(r) && r->model==model && r->effect==effect &&
        r->group==group && r->sequence==sequence) return r;
    return {};
}
inline bool HasAccepted(Model model,int effect,int group) {
    std::lock_guard<std::mutex> guard(lock);
    for(const auto& value:values)
        if(Current(value) && value->model==model && value->effect==effect && value->group==group) return true;
    return false;
}
inline Ref Make(Model model,int source,const Picked::SelectionSnapshot& s) {
    if(!model || source<=0 || !s.sequence || s.selection.effectEntityId<=0 ||
       s.selection.effectGroupId<0 || s.selection.cards.empty() || s.selection.cards.size()>3 ||
       s.observation==Picked::SnapshotObservation::CallbackFailureObserved ||
       s.observation==Picked::SnapshotObservation::ErasedBeforeCommitProof) return {};
    for(const auto& c:s.selection.cards) if(c.cardGuid<=0 || c.entityId==0 || c.entityId<-1) return {};
    std::lock_guard<std::mutex> guard(lock);
    for(const auto& r:values) if(r->model==model && r->sequence==s.sequence) {
        if(r->source!=source || r->effect!=s.selection.effectEntityId || r->group!=s.selection.effectGroupId ||
           r->cards.size()!=s.selection.cards.size()) return {};
        for(std::size_t i=0;i<r->cards.size();++i)
            if(r->cards[i].entityId!=s.selection.cards[i].entityId || r->cards[i].cardGuid!=s.selection.cards[i].cardGuid) return {};
        return r;
    }
    if(values.size()>=capacity) return {};
    auto cards=s.selection.cards;
    // Compatibility history is shared by predictions. Never import another
    // model's produced-entity ID from it. Live choice identities are retained.
    for(auto& c:cards) if(c.entityId==-1) c.boundEntityId=-1;
    auto bindings=std::make_shared<Bindings>();bindings->produced.resize(cards.size(),-1);
    auto r=std::make_shared<const Value>(Value{model,epoch.load(),s.sequence,
        s.selection.effectEntityId,s.selection.effectGroupId,source,std::move(cards),bindings});
    values.push_back(r);return r;
}
inline bool BindProduced(const Ref& r,int guid,int entity) {
    if(!Current(r) || guid<=0 || entity<=0) return false;
    std::lock_guard<std::mutex> guard(r->bindings->lock);
    if(r->bindings->cancelled) return false;
    std::size_t count=0,index=0;
    for(std::size_t i=0;i<r->cards.size();++i)
        if(r->cards[i].entityId==-1 && r->cards[i].cardGuid==guid) {++count;index=i;}
    if(count!=1) return false;
    auto& bound=r->bindings->produced[index];
    if(bound>0 && bound!=entity) return false;
    bound=entity;return true;
}
inline int LiveTarget(const Ref& r,std::size_t index) {
    if(!Current(r) || index>=r->cards.size()) return -1;
    std::lock_guard<std::mutex> guard(r->bindings->lock);
    if(r->bindings->cancelled) return -1;
    return r->cards[index].entityId>0?r->cards[index].entityId:r->bindings->produced[index];
}
inline void Reject(const Ref& r) {
    if(!Current(r)) return;
    std::lock_guard<std::mutex> guard(r->bindings->lock);r->bindings->cancelled=true;
}
inline bool Resolve(Model model,int effect,int candidate,bool guidDomain,bool& out) {
#if CUSTOMCOMPONENTS_PF3_TRANSACTION_PIPELINE && CUSTOMCOMPONENTS_PF3_TRANSACTION_ROUTING
    const auto& a=active;
    if(!Current(a.value) || !model || a.value->model!=model || effect<=0 ||
       (effect!=a.effect && effect!=a.contextEffect)) return false;
    out=false;
    std::lock_guard<std::mutex> guard(a.value->bindings->lock);
    if(a.value->bindings->cancelled) return true;
    for(std::size_t i=0;i<a.value->cards.size();++i) {
        const auto& c=a.value->cards[i];
        if(candidate>0 && (guidDomain ? candidate==c.cardGuid :
           candidate==(c.entityId>0?c.entityId:a.value->bindings->produced[i]))) out=true;
    }
    return true; // an authoritative negative overrides a stale positive too
#else
    (void)model;(void)effect;(void)candidate;(void)guidDomain;(void)out;
    return false;
#endif
}
inline bool ClaimEvent(const Ref& r) {
    std::lock_guard<std::mutex> guard(lock);
    if(!Current(r) || emitted.size()>=capacity) return false;
    for(const auto& k:emitted) if(k.model==r->model && k.sequence==r->sequence) return false;
    emitted.push_back({r->model,r->sequence}); return true;
}
inline bool AttachListener(const Ref& r,int listener,int source,Context event,std::uint64_t instance=0) {
    std::lock_guard<std::mutex> guard(lock);
    if(!Current(r) || listener<=0 || source<=0 || listeners.size()>=capacity) return false;
    listeners.push_back({r->model,listener,source,event,r,instance});return true;
}
inline Ref TakeListener(Model model,int listener,int source,Context event,std::uint64_t instance=0) {
    std::lock_guard<std::mutex> guard(lock);
    Ref result;std::size_t count=0;
    for(const auto& k:listeners)
        if(k.model==model && k.effect==listener && k.source==source && k.event==event && k.instance==instance) {
            ++count;result=k.value;
        }
    // Remove all colliding tickets: ambiguous generations cannot become valid
    // later merely because one ticket was consumed first.
    listeners.erase(std::remove_if(listeners.begin(),listeners.end(),[&](const Listener& k){
        return k.model==model && k.effect==listener && k.source==source && k.event==event && k.instance==instance;
    }),listeners.end());
    return count==1 && Current(result)?result:Ref{};
}
inline bool AttachEffect(EffectKey key,const Ref& r) {
    std::lock_guard<std::mutex> guard(lock);
    if(!Current(r) || key.model!=r->model || key.effect<=0 || key.group < -1 || (key.group==-1 && !key.instance) || queued.size()>=capacity) return false;
    // A second insertion of the same effect/generation is idempotent.
    for(const auto& k:queued) if(k.key==key && k.value==r) return true;
    queued.push_back({key,r});return true;
}
inline Ref TakeEffect(EffectKey key) {
    std::lock_guard<std::mutex> guard(lock);
    Ref result;std::size_t count=0;
    for(const auto& k:queued) if(k.key==key) {++count;result=k.value;}
    queued.erase(std::remove_if(queued.begin(),queued.end(),[&](const Queued& k){return k.key==key;}),queued.end());
    return count==1 && Current(result)?result:Ref{};
}
inline void Reset() {
    std::lock_guard<std::mutex> guard(lock);
    ++epoch; emitted.clear();listeners.clear();queued.clear();values.clear();active={};
}
} // namespace CustomComponents::Pick3::Transaction
