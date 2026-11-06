#include "engine/BoardPlacement.hpp"
#pragma once
#include "pick3/AdditionalSelectionSettings.hpp"
#include "pick3/DeferredLaneSelection.hpp"
#include "engine/EffectExecution.hpp"
#include "engine/MatchAccess.hpp"
#include "core/ModelRegistry.hpp"
namespace CustomComponents::Pick3::AdditionalSelection {
using List=BNM::Structures::Mono::List<int>;
using FindFn=Obj*(*)(Obj*,Obj*,int,Obj*,const void*);
inline FindFn originalFind=nullptr;
struct Receipt {Core::ModelRegistry::Token model;int effect,group,target,cardId,cardGuid;};
inline std::mutex receiptMutex;
inline std::vector<Receipt> receipts;
inline void Reset(){std::lock_guard<std::mutex> lock(receiptMutex);receipts.clear();}
inline void Remember(Obj* model,int effect,int group,int target,Obj* card=nullptr) {
    const auto token=Core::ModelRegistry::Identify(model,true);if(!token)return;
    std::lock_guard<std::mutex> lock(receiptMutex);
    receipts.erase(std::remove_if(receipts.begin(),receipts.end(),[&](auto r){return !Core::ModelRegistry::Get(r.model) || (r.model==token && r.effect==effect && r.group==group);}),receipts.end());
    if(receipts.size()>=256)receipts.erase(receipts.begin());
    receipts.push_back({token,effect,group,target,card?Engine::Match::Id(card):-1,Engine::BoardPlacement::Guid(card)});
}
inline bool Bounds(List* list){return list && list->size>=0 && list->size<=4096 && (!list->size || (list->items && list->items->capacity>=size_t(list->size)));}
inline Obj* FinderModel(Obj* finder) {
    static auto ef=BNM::Class("PvZCards.Core.Systems.Utilities","TriggeredEffectAdditionalTargetFinder").GetField("<EntityFinder>k__BackingField").cast<Obj*>();
    static auto mf=BNM::Class("PvZCards.Engine","EntityFinder").GetField("Model").cast<Obj*>();
    auto e=ef;auto m=mf;Obj* entities=finder && e.IsValid()?e[finder].Get():nullptr;
    return entities && m.IsValid()?m[entities].Get():nullptr;
}
inline void RetainOnly(Obj* object,int target) {
    auto* list=reinterpret_cast<List*>(object);if(!Bounds(list))return;
    auto remove=BNM::Class(object).GetMethod("RemoveAt",1).cast<void>();if(!remove.IsValid()) {
        // Never allow every candidate through when an explicit choice cannot be applied.
        auto clear=BNM::Class(object).GetMethod("Clear",0).cast<void>();
        if(clear.IsValid())clear[object]();
        else list->size=0; // List<int>: no managed references to release.
        LOG_ERR("CustomComponents/AdditionalSelection: missing RemoveAt; rejected target set");
        return;
    }
    for(int i=list->size-1;i>=0;--i)if(target<0 || list->items->m_Items[i]!=target)remove[object](i);
}
static Obj* Find(Obj* self,Obj* filter,int target,Obj* entity,const void* method) {
    Obj* model=FinderModel(self);
    const int lane=Engine::EntityAccess::Component(Engine::EntityAccess::Find(model,target),"Lane")?target:-1;
    // Reconstruct only this model/effect/group's authorized representative.
    // The popup's provisional scope has ended before queued effects execute.
    Obj* chosen=nullptr;
    const auto setting=Get(filter);
    const auto token=Core::ModelRegistry::Identify(model);
    const int effect=Engine::Match::Id(entity),group=Engine::EffectExecution::group;
    Receipt receipt{};bool found=false;
    {
        std::lock_guard<std::mutex> lock(receiptMutex);
        for(const auto& item:receipts)if(item.model==token && item.effect==effect && item.group==group){receipt=item;found=true;break;}
    }
    if(found && receipt.cardGuid>0) {
        chosen=setting.primary==5?Engine::BoardPlacement::Definition(receipt.cardGuid,entity):Engine::EntityAccess::Find(model,receipt.cardId);
        if(Engine::BoardPlacement::Guid(chosen)!=receipt.cardGuid)chosen=nullptr;
    }
    Engine::PlacementQueryContext::Scope placement({model,entity,chosen,lane,nullptr,false,true});
    Obj* result=originalFind(self,filter,target,entity,method);
    if(setting.mode==Inherit || setting.mode==All)return result;
    auto* list=reinterpret_cast<List*>(result);if(!Bounds(list))return result;
    int selected=-1;
    if(setting.mode==Random) {
        const int index=Engine::Match::Roll(entity,list->size);
        if(index>=0)selected=list->items->m_Items[index];
    } else if(Interactive(setting.mode)) {
        if(setting.primary==5)selected=target;
        else {
            const auto token=Core::ModelRegistry::Identify(FinderModel(self));
            const int effect=Engine::Match::Id(entity),group=Engine::EffectExecution::group;
            std::lock_guard<std::mutex> lock(receiptMutex);
            for(const auto& receipt:receipts)if(receipt.model==token && receipt.effect==effect && receipt.group==group)selected=receipt.target;
        }
    }
    RetainOnly(result,selected);return result;
}
// Query on the exact model without selecting, consuming RNG or retaining a pool list.
inline std::vector<int> Enumerate(Obj* model,int effect,int primary,Obj* chosenCard=nullptr) {
    std::vector<int> result;
    Obj* entity=Engine::EntityAccess::Find(model,effect);Obj* filter=Engine::EntityAccess::Component(entity,"PrimaryTargetFilter");
    if(!entity || !filter || !originalFind)return result;
    auto type=BNM::Class("PvZCards.Core.Systems.Utilities","TriggeredEffectAdditionalTargetFinder");
    auto lease=Engine::Match::Service(Engine::Match::Instances(entity),type);Obj* finder=lease?lease->Get():nullptr;
    if(!finder || FinderModel(finder)!=model)return result;
    Engine::PlacementQueryContext::Scope placement({model,entity,chosenCard,-1,nullptr,false,true});
    Obj* pooled=originalFind(finder,filter,primary,entity,nullptr);
    Core::ManagedRuntime::Root root(pooled);
    auto* list=reinterpret_cast<List*>(pooled);
    if(root.Get() && Bounds(list))for(int i=0;i<list->size;++i)if(list->items->m_Items[i]>=0)result.push_back(list->items->m_Items[i]);
    if(pooled)Core::ManagedPool::Return(Engine::Match::Instances(entity),pooled);
    return result;
}
inline bool Authorize(Obj* model,int effect,int group,int primary,int additional,Obj* chosenCard=nullptr) {
    auto* entity=Engine::EntityAccess::Find(model,effect);auto setting=Get(Engine::EntityAccess::Component(entity,"PrimaryTargetFilter"));
    if(!Interactive(setting.mode))return false;
    if(!chosenCard && setting.primary!=5)chosenCard=Engine::EntityAccess::Find(model,primary);
    const auto eligible=Enumerate(model,effect,primary,chosenCard);
    if(std::find(eligible.begin(),eligible.end(),additional)==eligible.end())return false;
    Remember(model,effect,group,additional,chosenCard);return true;
}
inline bool InitializeRuntime() {
    return InitializeSettings() && Core::Hooks::Install(BNM::Class("PvZCards.Core.Systems.Utilities","TriggeredEffectAdditionalTargetFinder").GetMethod("GetAdditionalTargets",3),Find,originalFind);
}
}
