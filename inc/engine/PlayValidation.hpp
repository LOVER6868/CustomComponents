#pragma once
#include "core/Observers.hpp"
#include "core/CandidateRules.hpp"
#include "core/HookRegistry.hpp"
#include "engine/EntityAccess.hpp"
#include "engine/MatchAccess.hpp"
#include "core/ManagedPool.hpp"
#include "core/ValidationPolicy.hpp"
#include <BNM/Il2CppHeaders.hpp>
namespace CustomComponents::Engine::PlayValidation {
using Obj=BNM::IL2CPP::Il2CppObject;
struct Context {Obj* model=nullptr;Obj* play=nullptr;Obj* ability=nullptr;int card=-1;bool resolved=false,targeted=false;};
inline thread_local Context current;
inline Core::Observers<Obj*,Obj*> before;
inline Core::CandidateRules<Context> rules;
struct Ability {Obj* model;Obj* card;Obj* effect;};
inline Core::CandidateRules<Ability> abilityRules;
inline bool(*usesRequirements)(Obj*,Obj*)=nullptr;
inline bool(*deferPrimaryQuery)(Obj*)=nullptr;
inline bool Allows(){if(current.resolved && current.targeted && !current.ability)return false;return Core::CandidateRules<Context>::Allows(rules.Read(),current,0);}
inline bool AllowsAbility(Obj* model,Obj* card,Obj* effect){return Core::CandidateRules<Ability>::Allows(abilityRules.Read(),{model,card,effect},0);}
inline Obj*(*originalFirst)(Obj*,Obj*,const void*)=nullptr;
inline bool HasPrimaryTargets(Obj* utility,Obj* effect) {
    static auto queryMetadata=BNM::Class("PvZCards.Core.Utilities","EffectEntityUtils").GetMethod("GetValidTargetsForEffectEntity",1).cast<Obj*>();
    static auto instancesMetadata=BNM::Class("PvZCards.Core.Utilities","EffectEntityUtils").GetField("<Instances>k__BackingField").cast<Obj*>();
    auto query=queryMetadata;auto instancesField=instancesMetadata;auto id=EntityAccess::Api().id;
    if(!effect || !query.IsValid() || !instancesField.IsValid())return false;
    Obj* instances=instancesField[utility].Get();if(!instances)return false;
    Obj* result=nullptr;auto exception=BNM::TryInvoke([&]{result=query[utility](id[effect].Get());});
    Core::ManagedRuntime::Root root(result);
    auto* list=reinterpret_cast<BNM::Structures::Mono::List<int>*>(result);
    const bool valid=!exception.IsValid() && list && list->items && list->size>0 && list->size<=int(list->items->capacity);
    if(result)Core::ManagedPool::Return(instances,result);
    return valid;
}
inline bool HasPrimaryQueryMatch(Obj*,Obj*,Obj*);
inline Obj* Select(Obj* self,Obj* card,const void* method,bool* hadTargets=nullptr) {
    static auto modelMetadata=BNM::Class("PvZCards.Core.Utilities","EffectEntityUtils").GetField("<EntityModel>k__BackingField").cast<Obj*>();auto modelField=modelMetadata;
    Obj* model=modelField[self].Get();
    auto id=EntityAccess::Api().id;
    if(current.resolved && current.model==model && card && current.card==id[card].Get()){if(hadTargets)*hadTargets=current.targeted;return current.ability;}
    Obj* stock=originalFirst(self,card,method);
    if(hadTargets)*hadTargets=stock!=nullptr;
    if(!usesRequirements || !usesRequirements(model,card))return stock;
    struct Restore {Context previous=current;~Restore(){current=previous;}}restore;
    if(current.model!=model)current={model,nullptr};
    // Requirements are the last gate for each native on-play candidate.
    // Their success never replaces target discovery or later validation.
    if(stock && Core::Validation::AfterNative(HasPrimaryTargets(self,stock) && HasPrimaryQueryMatch(model,card,stock),[&]{return AllowsAbility(model,card,stock);}))return stock;
    Obj* association=EntityAccess::Component(card,"EffectEntities");
    static auto idsMetadata=BNM::Class("PvZCards.Engine.Components","EntityAssociation").GetField("EntityIds").cast<BNM::Structures::Mono::List<int>*>();auto ids=idsMetadata;
    auto* list=association?ids[association].Get():nullptr;
    static auto targetedMetadata=BNM::Class("PvZCards.Core.Utilities","EffectEntityUtils").GetMethod("IsOnPlayTargetedEffect",1).cast<bool>();auto targeted=targetedMetadata;
    if(!list || !list->items || list->size<0 || list->size>int(list->items->capacity))return nullptr;
    for(int i=0;i<list->size;++i){int id=list->items->m_Items[i];Obj* effect=EntityAccess::Find(model,id);if(effect && effect!=stock && targeted[self](id)){if(hadTargets)*hadTargets=true;if(Core::Validation::AfterNative(HasPrimaryTargets(self,effect) && HasPrimaryQueryMatch(model,card,effect),[&]{return AllowsAbility(model,card,effect);}))return effect;}}
    return nullptr;
}
// PF3's native initial target can be a casting zone. That is not proof
// that this particular ability has any cards matching its primary query.
inline bool HasPrimaryQueryMatch(Obj* model,Obj* card,Obj* effect){
    Obj* filter=EntityAccess::Component(effect,"PrimaryTargetFilter");if(!filter)return true;
    static auto selectionMetadata=BNM::Class("PvZCards.Engine.Components","TargetEntityFilter").GetField("SelectionType").cast<int>();auto selection=selectionMetadata;
    if((selection.IsValid() && selection[filter].Get()==5) || (deferPrimaryQuery && deferPrimaryQuery(filter)))return true;
    static auto queryMetadata=BNM::Class("PvZCards.Engine.Components","EntityFilter").GetField("Query").cast<Obj*>();auto queryField=queryMetadata;
    Obj* query=queryField[filter].Get();if(!query)return true;
    static auto finderType=BNM::Class("PvZCards.Engine","EntityFinder");
    static auto findMetadata=finderType.GetMethod("FindEntities",1).cast<EntityAccess::List*>();
    static auto contextMetadata=finderType.GetField("QueryContext").cast<Obj*>();
    bool matches=false;auto error=BNM::TryInvoke([&]{
        auto root=std::make_shared<Core::ManagedRuntime::Root>(finderType.CreateNewObjectParameters(model));Obj* finder=root->Get();if(!finder)return;
        auto contextField=contextMetadata;Obj* context=contextField[finder].Get();if(!context)return;
        auto contextType=BNM::Class("PvZCards.Engine","EntityQueryContext");
        contextType.GetField("SelfId").cast<int>()[context].Set(Match::Id(card));
        auto source=contextType.GetMethod("set_SourceId",1).cast<void>();auto target=contextType.GetMethod("set_TargetId",1).cast<void>();auto owner=contextType.GetMethod("set_EffectEntityId",1).cast<void>();
        source[context](Match::Id(card));target[context](Match::Id(card));owner[context](Match::Id(effect));
        auto find=findMetadata;auto* result=find[finder](query);matches=EntityAccess::Valid(result) && result->size>0;
    });return !error.IsValid() && matches;
}
static Obj* First(Obj* self,Obj* card,const void* method){return Select(self,card,method);}
inline bool InitializeAbilitySelection(){return Core::Hooks::Install(BNM::Class("PvZCards.Core.Utilities","EffectEntityUtils").GetMethod("GetFirstOnPlayTriggeredAndTargetedEffectEntity",{BNM::Class("PvZCards.Engine","Entity").GetCompileTimeClass()}),First,originalFirst);}
inline void Prime(Obj* validator) {
    static auto utilityMetadata=BNM::Class("PvZCards.Core.Utilities","OneTimeEffectValidator").GetField("<EffectEntityUtils>k__BackingField").cast<Obj*>();
    static auto cardMetadata=BNM::Class("PvZCards.Engine.Dtos","Play").GetField("CardId").cast<int>();
    auto utilityField=utilityMetadata;auto cardField=cardMetadata;
    if(!current.play || !current.model || !usesRequirements)return;
    Obj* card=EntityAccess::Find(current.model,cardField[current.play].Get());
    if(!card || !usesRequirements(current.model,card))return;
    Obj* utility=utilityField[validator].Get();if(!utility)return;
    bool targeted=false;Obj* ability=Select(utility,card,nullptr,&targeted);
    current.card=cardField[current.play].Get();current.ability=ability;current.targeted=targeted;current.resolved=true;
}
struct Scope {
    Context previous;
    Scope(Obj* model,Obj* play):previous(current){current={model,play};before.Notify(model,play);}
    ~Scope(){current=previous;}
};
}
