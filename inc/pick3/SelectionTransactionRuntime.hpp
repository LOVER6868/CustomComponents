#pragma once
#include <BNM/Class.hpp>
#include <BNM/Field.hpp>
#include <BNM/Method.hpp>
#include "core/SceneViewContext.hpp"
#include "pick3/SelectionTransaction.hpp"
#include "pick3/SelectionSubmissionLedger.hpp"
namespace CustomComponents::Pick3::TransactionRuntime {
using Obj=BNM::IL2CPP::Il2CppObject;
namespace GC=CustomComponents::Core::SceneViewContext;
struct ModelRoot { Transaction::Model token; std::uint32_t handle; };
struct RouterRoot { std::uint32_t handle; Transaction::Model model; };
inline std::mutex rootsLock;
inline std::vector<ModelRoot> models;
inline std::vector<RouterRoot> routers;
inline Transaction::Model nextModel=1;
inline BNM::Field<Obj*> queryModel;
inline BNM::Class modifySunCostEffectClass;
inline bool pipelineReady=false;
inline Transaction::Model ModelOf(Obj* model,bool create=false) {
    if(!model || !GC::newGCHandle || !GC::getGCHandle || !GC::freeGCHandle) return 0;
    std::lock_guard<std::mutex> guard(rootsLock);
    for(const auto& m:models) if(GC::getGCHandle(m.handle)==model) return m.token;
    if(!create || models.size()>=256) return 0;
    const auto handle=GC::newGCHandle(model,false);
    if(!handle) return 0;
    const auto token=nextModel++;
    models.push_back({token,handle});return token;
}
inline void RegisterRouter(Obj* router,Obj* model) {
    const auto token=ModelOf(model,true);
    if(!router || !token) return;
    std::lock_guard<std::mutex> guard(rootsLock);
    for(auto& r:routers) if(GC::getGCHandle(r.handle)==router) {r.model=token;return;}
    if(routers.size()>=256) return;
    const auto handle=GC::newGCHandle(router,false);
    if(handle) routers.push_back({handle,token});
}
inline Transaction::Model RouterModel(Obj* router) {
    std::lock_guard<std::mutex> guard(rootsLock);
    if(!GC::getGCHandle) return 0;
    for(const auto& r:routers) if(GC::getGCHandle(r.handle)==router) return r.model;
    return 0;
}
inline int IntAt(Obj* object,std::size_t offset) {
    return object?*reinterpret_cast<const int*>(reinterpret_cast<const std::uint8_t*>(object)+offset):-1;
}
inline Obj* ObjectAt(Obj* object,std::size_t offset) {
    return object?*reinterpret_cast<Obj* const*>(reinterpret_cast<const std::uint8_t*>(object)+offset):nullptr;
}
inline Transaction::Context ReadContext(Obj* context) {
    return {IntAt(context,0x18),IntAt(context,0x14),IntAt(context,0x10),IntAt(context,0x1C)};
}
inline Transaction::EffectKey Key(Transaction::Model model,Obj* effect) {
    return {model,IntAt(effect,0x30),IntAt(effect,0x3C),IntAt(effect,0x24),IntAt(effect,0x28)};
}
inline bool Resolve(Obj* context,int effect,int candidate,bool guidDomain,bool& out) {
    if(!pipelineReady || !context || !queryModel.IsValid() || !Transaction::Current(Transaction::active.value)) return false;
    return Transaction::Resolve(ModelOf(queryModel[context].Get()),effect,candidate,guidDomain,out);
}
inline Transaction::Ref Capture(Obj* model,int effect,int group,int source,std::uint64_t sequence=0) {
#if !CUSTOMCOMPONENTS_PF3_TRANSACTION_PIPELINE
    (void)model;(void)effect;(void)group;(void)source;(void)sequence;return {};
#else
    // Reading a staged ledger entry is never sufficient to create provenance.
    // Only the model-side acceptance adapter creates canonical transactions.
    if(!sequence) return {};
    auto value=Transaction::Find(ModelOf(model),effect,group,sequence);
    return value && value->source==source?value:Transaction::Ref{};
#endif
}
struct EffectRoot {
    Transaction::EffectKey key;
    std::uint32_t handle;
    int contextEffect=-1;
};
inline std::mutex effectsLock;
inline std::vector<EffectRoot> effectRoots;
inline std::uint64_t nextEffect=1;
inline void AttachInsertedEffect(Obj* effect) {
    const auto value=Transaction::active.value;
    if(!effect || !Transaction::Current(value) || !GC::newGCHandle || !GC::getGCHandle || !GC::freeGCHandle) return;
    std::lock_guard<std::mutex> guard(effectsLock);
    auto key=Key(value->model,effect);
    if(key.effect<=0) return;
    for(const auto& r:effectRoots) if(GC::getGCHandle(r.handle)==effect) return;
    if(effectRoots.size()>=Transaction::capacity) return;
    const auto handle=GC::newGCHandle(effect,false);
    if(!handle) return;
    key.instance=nextEffect++;
    if(!Transaction::AttachEffect(key,value)) {
        LOG_WARN("CustomComponents/Pick3/PF3_TX: phase=ATTACH_REJECTED seq=%llu effect=%d group=%d",(unsigned long long)value->sequence,key.effect,key.group);
        GC::freeGCHandle(handle);return;
    }
    effectRoots.push_back({key,handle,Transaction::active.contextEffect});
    LOG_INFO("CustomComponents/Pick3/PF3_TX: phase=DESCENDANT_ATTACHED seq=%llu effect=%d group=%d instance=%llu",(unsigned long long)value->sequence,key.effect,key.group,(unsigned long long)key.instance);
}
struct AutomaticChoice {
    Transaction::Model model=0; int effect=-1,source=-1,target=-1,guid=-1;
    Transaction::Context context; Transaction::Ref value; bool notified=false;
};
struct AutomaticRoot { std::uint32_t handle; std::shared_ptr<AutomaticChoice> choice; Transaction::EffectKey key; };
inline std::mutex automaticLock;
inline std::vector<AutomaticRoot> automaticRoots;
inline void AttachAutomaticEffects(Obj* model,int effect,int source,int target,int guid,Transaction::Context context,Obj* result) {
    if(!pipelineReady || Transaction::Current(Transaction::active.value) || !model || effect<=0 || source<=0 || target<=0 || guid<=0 || !GC::newGCHandle) return;
    auto* list=reinterpret_cast<BNM::Structures::Mono::List<Obj*>*>(result);
    if(!list || !list->items || list->size<=0 || list->size>32 || list->size>int(list->items->capacity)) return;
    static auto choose=BNM::Class("PvZCards.Engine.Effects","ChooseTargetBlockingEffect");
    static auto phase=choose.GetField("Phase").cast<int>();
    if(!choose.IsValid() || !phase.IsValid()) return;
    for(int i=0;i<list->size;++i) {
        Obj* item=list->items->m_Items[i];
        if(!item || IntAt(item,0x30)!=effect || IntAt(item,0x24)!=source ||
           (BNM::IsA(item,choose) && phase[item].Get()==0)) return;
    }
    auto choice=std::make_shared<AutomaticChoice>();
    choice->model=ModelOf(model,true);choice->effect=effect;choice->source=source;choice->target=target;choice->guid=guid;context.effect=effect;context.source=source;context.target=target;choice->context=context;
    if(!choice->model) return;
    std::lock_guard<std::mutex> guard(automaticLock);
    if(automaticRoots.size()+list->size>Transaction::capacity) return;
    for(const auto& root:automaticRoots) for(int i=0;i<list->size;++i)
        if(GC::getGCHandle(root.handle)==list->items->m_Items[i]) return; // nested creator observers see the same list
    for(int i=0;i<list->size;++i) {
        auto h=GC::newGCHandle(list->items->m_Items[i],false);
        if(h) automaticRoots.push_back({h,choice,Key(choice->model,list->items->m_Items[i])});
    }
    LOG_INFO("CustomComponents/Pick3/PF3_TX: phase=AUTO_CREATED effect=%d primary=%d guid=%d effects=%d",effect,target,guid,list->size);
}
struct EffectLink { Transaction::Ref value; int contextEffect=-1; std::shared_ptr<AutomaticChoice> automatic; };
inline EffectLink TakeInsertedEffect(Transaction::Model model,Obj* effect) {
    if(!model || !effect || !GC::getGCHandle) return {};
    std::lock_guard<std::mutex> guard(effectsLock);
    for(auto it=effectRoots.begin();it!=effectRoots.end();++it) {
        if(GC::getGCHandle(it->handle)!=effect) continue;
        auto observed=Key(model,effect);observed.instance=it->key.instance;
        const auto expected=it->key;
        const int contextEffect=it->contextEffect;
        GC::freeGCHandle(it->handle);effectRoots.erase(it);
        const auto value=Transaction::TakeEffect(expected);
        // Group assignment happens after creation; the rooted object and other
        // identity fields must still match when it reaches the router.
        if(expected.group==-1 && observed.group>=0) observed.group=-1;
        const bool identityMatches=observed==expected && Transaction::Current(value);
        LOG_INFO("CustomComponents/Pick3/PF3_TX: phase=DESCENDANT_ROUTED seq=%llu effect=%d finalGroup=%d identityMatches=%d",
            (unsigned long long)(value?value->sequence:0),IntAt(effect,0x30),IntAt(effect,0x3C),int(identityMatches));
        return identityMatches?EffectLink{value,contextEffect}:EffectLink{};
    }
    return {};
}
inline bool IsScopedChild(Transaction::Model model,Obj* effect,const Transaction::Ref& value) {
    if(!model || !effect || !Transaction::Current(value) || value->model!=model) return false;
    const int source=IntAt(effect,0x24);
    const int effectEntity=IntAt(effect,0x30);
    return source>0 && effectEntity>0 &&
        (effectEntity==Transaction::active.effect ||
         effectEntity==Transaction::active.contextEffect);
}
inline EffectLink ResolveEffectLink(Obj* router,Obj* effect) {
    const auto model=RouterModel(router);
    auto link=TakeInsertedEffect(model,effect);
    if(!Transaction::Current(link.value)) {
        std::shared_ptr<AutomaticChoice> choice;
        {std::lock_guard<std::mutex> guard(automaticLock);
         for(auto it=automaticRoots.begin();it!=automaticRoots.end();++it) if(GC::getGCHandle(it->handle)==effect) {
             auto observed=Key(model,effect);
             const auto expected=it->key;
             if(expected.group==-1 && observed.group>=0) observed.group=-1;
             if(observed==expected) choice=it->choice;
             else LOG_WARN("CustomComponents/Pick3/PF3_TX: phase=AUTO_IDENTITY_REJECTED effect=%d group=%d",IntAt(effect,0x30),IntAt(effect,0x3C));
             GC::freeGCHandle(it->handle);automaticRoots.erase(it);break;
         }}
        if(choice && choice->model==model && IntAt(effect,0x30)==choice->effect && IntAt(effect,0x24)==choice->source && IntAt(effect,0x3C)>=0) {
            std::lock_guard<std::mutex> guard(automaticLock);
            if(!choice->value && !Transaction::HasAccepted(model,choice->effect,IntAt(effect,0x3C))) {
                std::uint64_t sequence=0;
                if(Picked::Store(choice->effect,IntAt(effect,0x3C),{{choice->target,choice->guid,-1}},"STOCK_AUTO_CREATED",&sequence)) {
                    Picked::SelectionSnapshot snapshot;snapshot.sequence=sequence;
                    snapshot.selection={choice->effect,IntAt(effect,0x3C),{{choice->target,choice->guid,-1}}};
                    choice->value=Transaction::Make(model,choice->source,snapshot);
                }
            }
            if(Transaction::Current(choice->value)) link={choice->value,choice->context.effect,choice};
        }
    }
    if(!Transaction::Current(link.value) &&
       IsScopedChild(model,effect,Transaction::active.value)) {
        // Some stock effect-collection paths process a same-source child
        // without passing through EffectQueue.PushEffectToList. Preserve its
        // exact enclosing selection transaction in that case. Model, source,
        // and effect-entity identity must all agree.
        link.value=Transaction::active.value;
        link.contextEffect=Transaction::active.contextEffect;
        LOG_INFO("CustomComponents/Pick3/PF3_TX: phase=DESCENDANT_SCOPE_INHERITED seq=%llu effect=%d source=%d model=%llu",
            (unsigned long long)link.value->sequence,IntAt(effect,0x30),
            IntAt(effect,0x24),(unsigned long long)model);
    }
    return link;
}
struct ScopedRouter {
    EffectLink link;
    Transaction::Scoped scope;
    ScopedRouter(Obj* router,Obj* effect)
        :link(ResolveEffectLink(router,effect)),
         scope(link.value,IntAt(effect,0x30),link.contextEffect) {}
};
inline void AuditBoundCostTarget(Transaction::Model model,Obj* effect) {
    namespace TX=CustomComponents::Pick3::Transaction;
    if(!effect || !modifySunCostEffectClass.IsValid() || !BNM::IsA(effect,modifySunCostEffectClass)) return;
    const auto value=TX::active.value;
    const int effectId=IntAt(effect,0x30);
    if(!TX::Current(value) || value->model!=model || IntAt(effect,0x24)<=0 ||
       (effectId!=TX::active.effect && effectId!=TX::active.contextEffect)) return;
    const int oldTarget=IntAt(effect,0x28);
    bool matched=false;
    if(!TX::Resolve(model,effectId,oldTarget,false,matched) || matched || value->cards.size()!=1) return;
    const int selected=TX::LiveTarget(value,0);
    if(selected<=0) return;
    LOG_INFO("CustomComponents/Pick3/PF3_TX: phase=COST_TARGET_MISMATCH seq=%llu effect=%d group=%d source=%d actualTarget=%d selectedTarget=%d boundary=EFFECT_ROUTER changed=0",
        (unsigned long long)value->sequence,effectId,IntAt(effect,0x3C),IntAt(effect,0x24),oldTarget,selected);
}
// Only survives a synchronous stock selection-service call. The model-side
// acceptance hook checks the actual phase and exact selected-target list.
inline thread_local Submission submitted;
struct ScopedSubmission {
    Submission prior;
    explicit ScopedSubmission(Submission next):prior(submitted){submitted=std::move(next);}
    ~ScopedSubmission(){submitted=std::move(prior);}
    ScopedSubmission(const ScopedSubmission&)=delete;
};
inline void (*onStockAccepted)(Obj*,Obj*,Transaction::Ref,Transaction::Context,int)=nullptr;
inline void CompleteAutomaticSelection(const EffectLink& link,Obj* effect) {
    if(!link.automatic || !Transaction::Current(link.value) || !onStockAccepted) return;
    {std::lock_guard<std::mutex> guard(automaticLock);
     if(link.automatic->notified) return;
     link.automatic->notified=true;}
    Obj* model=nullptr;
    {std::lock_guard<std::mutex> guard(rootsLock);
     for(const auto& root:models) if(root.token==link.value->model) {model=GC::getGCHandle(root.handle);break;}}
    if(model) {
        LOG_INFO("CustomComponents/Pick3/PF3_TX: phase=AUTO_EVENT_RELEASE seq=%llu effect=%d group=%d primary=%d",(unsigned long long)link.value->sequence,link.value->effect,link.value->group,link.automatic->target);
        onStockAccepted(model,effect,link.value,link.automatic->context,link.value->group);
    }
}
struct AwaitingCreation { Transaction::Ref value; Transaction::Context context; int group; };
inline std::mutex awaitingLock;
inline std::vector<AwaitingCreation> awaitingCreation;
inline void AwaitCreation(Transaction::Ref value,Transaction::Context context,int group) {
    std::lock_guard<std::mutex> guard(awaitingLock);
    if(!Transaction::Current(value) || awaitingCreation.size()>=Transaction::capacity) return;
    for(const auto& item:awaitingCreation)
        if(item.value->model==value->model && item.value->sequence==value->sequence) return;
    awaitingCreation.push_back({std::move(value),context,group});
}
// Recover only an already accepted database selection. History identifies the
// generation; this model-owned ledger supplies the authority and the Ref.
inline Transaction::Ref FindAwaitingCreation(Obj* model,int effect,int group,std::uint64_t sequence,int guid) {
    const auto token=ModelOf(model);
    if(!token || effect<=0 || group<0 || !sequence || guid<=0) return {};
    std::lock_guard<std::mutex> guard(awaitingLock);
    Transaction::Ref found;
    for(const auto& item:awaitingCreation) {
        const auto& value=item.value;
        if(!Transaction::Current(value) || value->model!=token || value->effect!=effect ||
           value->group!=group || item.group!=group || value->sequence!=sequence) continue;
        unsigned matches=0;
        for(const auto& card:value->cards) if(card.entityId==-1 && card.cardGuid==guid) ++matches;
        if(matches!=1 || found) return {}; // ambiguous picks cannot inherit a latest GUID
        found=value;
    }
    return found;
}
inline void Produced(Obj* model,Obj* producingEffect,int effect,int group,std::uint64_t sequence) {
    const auto token=ModelOf(model);
    if(!token || !onStockAccepted) return;
    AwaitingCreation found{};
    {
        std::lock_guard<std::mutex> guard(awaitingLock);
        const auto it=std::find_if(awaitingCreation.begin(),awaitingCreation.end(),[&](const auto& item){
            return item.value->model==token && item.value->sequence==sequence &&
                item.value->effect==effect && item.group==group;
        });
        if(it==awaitingCreation.end() || !Transaction::Current(it->value) ||
           Transaction::LiveTarget(it->value,0)<=0) return;
        found=*it;awaitingCreation.erase(it);
    }
    auto value=Capture(model,effect,group,found.value->source,sequence);
    if(value) {
        LOG_INFO("CustomComponents/Pick3/PF3_TX: phase=DATABASE_EVENT_RELEASE seq=%llu effect=%d group=%d target=%d",
            (unsigned long long)sequence,effect,group,Transaction::LiveTarget(value,0));
        onStockAccepted(model,producingEffect,value,found.context,group);
    }
}
struct TriggerRoot {
    Transaction::Model model;
    int listener,source;
    Transaction::Context context;
    std::uint64_t ticket;
    std::uint32_t handle;
};
inline std::mutex triggersLock;
inline std::vector<TriggerRoot> triggerRoots;
inline std::uint64_t nextTrigger=1;
inline void AttachTriggerData(Obj* data,const Transaction::Ref& value) {
    if(!data || !Transaction::Current(value) || !GC::newGCHandle || !GC::freeGCHandle) return;
    const int listener=IntAt(data,0x10),source=IntAt(data,0x14);
    const auto context=ReadContext(ObjectAt(data,0x18));
    std::lock_guard<std::mutex> guard(triggersLock);
    if(triggerRoots.size()>=Transaction::capacity) return;
    const auto handle=GC::newGCHandle(data,false);
    if(!handle) return;
    const auto ticket=nextTrigger++;
    if(!Transaction::AttachListener(value,listener,source,context,ticket)) {GC::freeGCHandle(handle);return;}
    triggerRoots.push_back({value->model,listener,source,context,ticket,handle});
    LOG_INFO("CustomComponents/Pick3/PF3_TX: phase=LISTENER_SELECTION_ATTACHED seq=%llu listener=%d source=%d ticket=%llu",(unsigned long long)value->sequence,listener,source,(unsigned long long)ticket);
}
inline void RebindTriggerData(Transaction::Model model,Obj* original,Obj* copy) {
    if(!model || !original || !copy || !GC::getGCHandle) return;
    std::lock_guard<std::mutex> guard(triggersLock);
    for(auto& r:triggerRoots) {
        if(r.model!=model || GC::getGCHandle(r.handle)!=original) continue;
        if(IntAt(copy,0x10)!=r.listener || IntAt(copy,0x14)!=r.source ||
           !(ReadContext(ObjectAt(copy,0x18))==r.context)) continue;
        const auto handle=GC::newGCHandle(copy,false);
        if(!handle) continue;
        GC::freeGCHandle(r.handle);r.handle=handle;
        LOG_INFO("CustomComponents/Pick3/PF3_TX: phase=TRIGGER_COPY_BOUND listener=%d ticket=%llu",r.listener,(unsigned long long)r.ticket);
    }
}
inline Transaction::Ref TakeTriggerData(Transaction::Model model,Obj* data,
                                        int listener,int source,Transaction::Context context) {
    if(!data || !model || !GC::getGCHandle) return {};
    std::lock_guard<std::mutex> guard(triggersLock);
    std::size_t count=0;
    Transaction::Ref result;
    for(auto it=triggerRoots.begin();it!=triggerRoots.end();) {
        if(it->model!=model || GC::getGCHandle(it->handle)!=data) {++it;continue;}
        ++count;
        const auto r=*it;
        GC::freeGCHandle(it->handle);it=triggerRoots.erase(it);
        auto value=Transaction::TakeListener(model,r.listener,r.source,r.context,r.ticket);
        if(listener==r.listener && source==r.source && context==r.context) result=value;
    }
    return count==1?result:Transaction::Ref{};
}
inline void (*originalTriggerAdd)(Obj*,Obj*,const void*)=nullptr;
inline Obj* (*originalCreateTriggered)(Obj*,Obj*,const void*)=nullptr;
// A consumed trigger must also be returned to its owning stock pool.
inline bool (*consumeTriggerData)(Obj*,Obj*)=nullptr;
static void TriggerAddHook(Obj* self,Obj* data,const void* method) {
    if(consumeTriggerData && consumeTriggerData(self,data)) return;
    AttachTriggerData(data,Transaction::active.value);
    originalTriggerAdd(self,data,method);
}
static Obj* CreateTriggeredHook(Obj* self,Obj* data,const void* method) {
    Obj* effect=originalCreateTriggered(self,data,method);
    // Stock copies TriggerData into the runtime TriggeredEffect. Transfer the
    // ticket before the input may be returned to the pool. Rooted identities,
    // not matching context values, distinguish identical concurrent listeners.
    RebindTriggerData(ModelOf(ObjectAt(self,0x30)),data,ObjectAt(effect,0x48));
    return effect;
}
inline Obj* (*originalFindTargets)(Obj*,Obj*,Obj*,const void*)=nullptr;
static Obj* FindTargetsHook(Obj* self,Obj* entity,Obj* context,const void* method) {
    Obj* result=originalFindTargets(self,entity,context,method);
    const auto value=Transaction::active.value;
    if(!Transaction::Current(value)) return result;
    auto* list=reinterpret_cast<BNM::Structures::Mono::List<int>*>(result);
    const int count=list ? list->size : 0;
    LOG_INFO("CustomComponents/Pick3/PF3_TX: phase=TARGETS seq=%llu listener=%d count=%d",(unsigned long long)value->sequence,Transaction::active.effect,count);
    if(list && list->items && count>=0 && count<=int(list->items->capacity))
        for(int i=0;i<count;++i) {
            const int id=list->items->m_Items[i]; bool matched=false;
            Transaction::Resolve(value->model,Transaction::active.effect,id,false,matched);
            LOG_INFO("CustomComponents/Pick3/PF3_TX: phase=TARGET seq=%llu entity=%d selected=%d",(unsigned long long)value->sequence,id,int(matched));
        }
    return result;
}
inline Obj* (*originalResolverCreateEffects)(Obj*,Obj*,Obj*,const void*)=nullptr;
static Obj* ResolverCreateEffectsHook(Obj* self,Obj* entity,Obj* context,const void* method) {
    const auto value=Transaction::active.value;
    const int listener=Transaction::active.effect;
    Obj* effectContext=ObjectAt(context,0x18);
    // Use the context the stock creator actually queries, including its producer
    // identity, rather than assuming it equals the listener definition ID.
    Transaction::Scoped scope(value,listener,IntAt(effectContext,0x18));
    Obj* result=originalResolverCreateEffects(self,entity,context,method);
    if(!Transaction::Current(value)) return result;
    auto* list=reinterpret_cast<BNM::Structures::Mono::List<Obj*>*>(result);
    const int count=list ? list->size : 0;
    LOG_INFO("CustomComponents/Pick3/PF3_TX: phase=CREATED_EFFECTS seq=%llu listener=%d contextEffect=%d count=%d",
        (unsigned long long)value->sequence,listener,IntAt(effectContext,0x18),count);
    if(list && list->items && count>=0 && count<=int(list->items->capacity))
        for(int i=0;i<count;++i) {
            Obj* effect=list->items->m_Items[i];
            LOG_INFO("CustomComponents/Pick3/PF3_TX: phase=CREATED_EFFECT seq=%llu index=%d effect=%d source=%d target=%d group=%d cost=%d",
                (unsigned long long)value->sequence,i,IntAt(effect,0x30),IntAt(effect,0x24),IntAt(effect,0x28),IntAt(effect,0x3C),
                int(effect && modifySunCostEffectClass.IsValid() && BNM::IsA(effect,modifySunCostEffectClass)));
            AttachInsertedEffect(effect);
        }
    return result;
}
inline bool externalRouterHook=false;
inline void (*originalRouter)(void*,void*,const void*)=nullptr;
static void RouterHook(void* self,void* effect,const void* method) {
    auto* router=static_cast<Obj*>(self);
    auto* runtimeEffect=static_cast<Obj*>(effect);
    ScopedRouter scope(router,runtimeEffect);
    AuditBoundCostTarget(RouterModel(router),runtimeEffect);
    originalRouter(self,effect,method);
    CompleteAutomaticSelection(scope.link,runtimeEffect);
}
inline bool Initialize() {
    // Return-to-lane trigger filtering shares this hook but is independent of
    // optional PickFromThree transaction routing.
    auto add=BNM::Class("PvZCards.Engine","TriggeredEffectCollection").GetMethod("Add",1);
    if(add.IsValid() && !originalTriggerAdd) BNM::BasicHook(add,TriggerAddHook,originalTriggerAdd);
    if(!originalTriggerAdd) LOG_ERR("CustomComponents/ReturnToLane: move trigger filter unavailable");
#if !CUSTOMCOMPONENTS_PF3_TRANSACTION_PIPELINE
    return true;
#endif
    queryModel=BNM::Class("PvZCards.Engine","EntityQueryContext").GetField("<Model>k__BackingField").cast<Obj*>();
    modifySunCostEffectClass=BNM::Class("PvZCards.Engine.Effects","ModifySunCostEffect");
    if(!externalRouterHook && !originalRouter) {
        auto method=BNM::Class("PvZCards.Engine","EffectRouter").GetMethod("ProcessEffect",1);
        if(method.IsValid()) BNM::BasicHook(method,RouterHook,originalRouter);
    }
    auto create=BNM::Class("PvZCards.Core.Systems","TriggeredEffectQueuingSystem").GetMethod("CreateTriggeredEffectFrom",1);
    if(create.IsValid() && add.IsValid()) {
        if(!originalCreateTriggered) BNM::BasicHook(create,CreateTriggeredHook,originalCreateTriggered);
        if(originalCreateTriggered && !originalTriggerAdd) BNM::BasicHook(add,TriggerAddHook,originalTriggerAdd);
    }
    auto targets=BNM::Class("PvZCards.Core.Systems.Utilities","TargetEntityFinder").GetMethod("FindTargets",2);
    if(targets.IsValid() && !originalFindTargets) BNM::BasicHook(targets,FindTargetsHook,originalFindTargets);
    LOG_INFO("CustomComponents/Pick3/PF3_TX: phase=TARGET_HOOK installed=%d",int(originalFindTargets!=nullptr));
    auto creator=BNM::Class("PvZCards.Core.Systems","TriggeredEffectResolverSystem").GetMethod("CreateEffects",2);
    if(creator.IsValid() && !originalResolverCreateEffects)
        BNM::BasicHook(creator,ResolverCreateEffectsHook,originalResolverCreateEffects);
    LOG_INFO("CustomComponents/Pick3/PF3_TX: phase=CREATION_HOOK installed=%d",int(originalResolverCreateEffects!=nullptr));
    pipelineReady=queryModel.IsValid() && (externalRouterHook || originalRouter) && originalTriggerAdd && originalCreateTriggered && originalResolverCreateEffects;
    LOG_INFO("CustomComponents/Pick3/PF3_TX: phase=INIT version=2.23 ready=%d router=%d queryModel=%d triggerAdd=%d triggerCopy=%d costEffectClass=%d",int(pipelineReady),int(externalRouterHook || originalRouter),int(queryModel.IsValid()),int(originalTriggerAdd!=nullptr),int(originalCreateTriggered!=nullptr),int(modifySunCostEffectClass.IsValid()));
    return pipelineReady;
}
inline void Reset() {
    ResetSubmissions();
    Transaction::Reset();
    {std::lock_guard<std::mutex> guard(automaticLock);
     if(GC::freeGCHandle) for(const auto& root:automaticRoots) GC::freeGCHandle(root.handle);
     automaticRoots.clear();}
    {std::lock_guard<std::mutex> guard(awaitingLock);awaitingCreation.clear();}
    {
        std::lock_guard<std::mutex> guard(triggersLock);
        if(GC::freeGCHandle) for(const auto& trigger:triggerRoots) GC::freeGCHandle(trigger.handle);
        triggerRoots.clear();
    }
    {
        std::lock_guard<std::mutex> guard(effectsLock);
        if(GC::freeGCHandle) for(const auto& effect:effectRoots) GC::freeGCHandle(effect.handle);
        effectRoots.clear();
    }
    std::lock_guard<std::mutex> guard(rootsLock);
    if(GC::freeGCHandle) {
        for(const auto& r:routers) GC::freeGCHandle(r.handle);
        for(const auto& m:models) GC::freeGCHandle(m.handle);
    }
    routers.clear();models.clear(); // tokens are never reused across matches
}
} // namespace CustomComponents::Pick3::TransactionRuntime
