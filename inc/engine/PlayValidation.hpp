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
inline bool(*validDatabaseSelection)(Obj*,Obj*,Obj*)=nullptr;
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
inline Obj* SelectDatabaseOnPlay(Obj* model,Obj* card){
    if(!model || !card || !validDatabaseSelection)return nullptr;
    Obj* association=EntityAccess::Component(card,"EffectEntities");
    static auto idsMetadata=BNM::Class("PvZCards.Engine.Components","EntityAssociation").GetField("EntityIds").cast<BNM::Structures::Mono::List<int>*>();
    auto* ids=association && idsMetadata.IsValid()?idsMetadata[association].Get():nullptr;
    if(!ids || !ids->items || ids->size<0 || ids->size>int(ids->items->capacity))return nullptr;
    for(int i=0;i<ids->size;++i){
        Obj* effect=EntityAccess::Find(model,ids->items->m_Items[i]);
        if(effect && EntityAccess::Component(effect,"PlayTrigger") && validDatabaseSelection(model,card,effect) && AllowsAbility(model,card,effect))return effect;
    }
    return nullptr;
}
// Stock GetFirstOnPlayTriggeredAndTargetedEffectEntity recognizes stock
// descriptors, but a custom descriptor may carry legal *Manual* filters and
// still be overlooked (Hypno-Shroom / Escape Root). The manual targets are
// real LIVE entities. Database PickFromThree is explicitly excluded.
inline bool IsManualPlayEffect(Obj* effect){
    if(!effect || !EntityAccess::Component(effect,"PlayTrigger"))return false;
    Obj* filter=EntityAccess::Component(effect,"PrimaryTargetFilter");
    static auto selection=BNM::Class("PvZCards.Engine.Components","TargetEntityFilter").GetField("SelectionType").cast<int>();
    return filter && selection.IsValid() && selection[filter].Get()==1;
}
// Descriptors registered by CustomComponents can be absent from the stock
// first-on-play descriptor whitelist even while their primary Manual filter
// is legal. Prefer these by their authored trigger/query, not by GUID.
inline bool IsCustomManualPlayEffect(Obj* effect){
    return IsManualPlayEffect(effect) &&
        (EntityAccess::Component(effect,"CombatEffectDescriptor") ||
         EntityAccess::Component(effect,"SwapPositionEffectDescriptor") ||
         EntityAccess::Component(effect,"MoveCardToLanesEffectDescriptor"));
}
inline Obj* SelectManualOnPlay(Obj* utility,Obj* model,Obj* card,bool customOnly=false){
    if(!utility || !model || !card)return nullptr;
    Obj* association=EntityAccess::Component(card,"EffectEntities");
    static auto ids=BNM::Class("PvZCards.Engine.Components","EntityAssociation").GetField("EntityIds").cast<BNM::Structures::Mono::List<int>*>();
    auto* list=association && ids.IsValid()?ids[association].Get():nullptr;
    if(!list || !list->items || list->size<0 || list->size>int(list->items->capacity))return nullptr;
    for(int i=0;i<list->size;++i){
        Obj* effect=EntityAccess::Find(model,list->items->m_Items[i]);
        if(!IsManualPlayEffect(effect) || (customOnly && !IsCustomManualPlayEffect(effect)) || !AllowsAbility(model,card,effect))continue;
        const bool targets=HasPrimaryTargets(utility,effect),query=HasPrimaryQueryMatch(model,card,effect);
        const bool custom=IsCustomManualPlayEffect(effect);
        static std::atomic<unsigned> checks{0};if(checks.fetch_add(1)<56){
            auto id=EntityAccess::Api().id;
            LOG_INFO("CustomComponents/Targeting/ONPLAY_MANUAL: card=%d effect=%d targets=%d query=%d custom=%d",
                id[card].Get(),id[effect].Get(),int(targets),int(query),int(custom));
        }
        // The stock target list may be empty because it does not recognise a
        // custom effect descriptor. This is only the discovery step; actual
        // manual target selection/legality still goes through stock queries.
        if(query && (targets || custom))return effect;
    }
    return nullptr;
}
inline Obj* Select(Obj* self,Obj* card,const void* method,bool* hadTargets=nullptr) {
    static auto modelMetadata=BNM::Class("PvZCards.Core.Utilities","EffectEntityUtils").GetField("<EntityModel>k__BackingField").cast<Obj*>();auto modelField=modelMetadata;
    Obj* model=modelField[self].Get();
    auto id=EntityAccess::Api().id;
    if(current.resolved && current.model==model && card && current.card==id[card].Get()){if(hadTargets)*hadTargets=current.targeted;return current.ability;}
    Obj* stock=originalFirst(self,card,method);
    // Prefer an authored custom on-play manual effect over an unrelated stock
    // first-effect fallback, but only after its live primary query matches.
    if(card){
        if(Obj* manual=SelectManualOnPlay(self,model,card,true)){
            if(IsCustomManualPlayEffect(manual)){
                if(hadTargets)*hadTargets=true;
                return manual;
            }
        }
    }
    // Database PickFromThree is an EFFECT-TIME popup, never a legal target
    // during PlayCard placement.  In particular Junior's separate on-play
    // grant must not drag a 41-card CardDatabase offer onto the board.
    // The live effect's selection bridge owns the later popup.
    if(!stock){
        if(Obj* manual=SelectManualOnPlay(self,model,card)){
            if(hadTargets)*hadTargets=true;
            return manual;
        }
    }
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
        auto find=findMetadata;auto* result=find[finder](query);
        // CombatEffectDescriptor consumes two DIFFERENT manual fighters.
        // A single matching Zombie must not make the trick appear playable.
        const int required=EntityAccess::Component(effect,"CombatEffectDescriptor")?2:1;
        matches=EntityAccess::Valid(result) && result->size>=required;
    });return !error.IsValid() && matches;
}
static Obj* First(Obj* self,Obj* card,const void* method){return Select(self,card,method);}
inline bool InitializeAbilitySelection(){return Core::Hooks::Install(BNM::Class("PvZCards.Core.Utilities","EffectEntityUtils").GetMethod("GetFirstOnPlayTriggeredAndTargetedEffectEntity",{std::string_view("cardEntity")}),First,originalFirst);}
inline void Prime(Obj* validator) {
    static auto utilityMetadata=BNM::Class("PvZCards.Core.Utilities","OneTimeEffectValidator").GetField("<EffectEntityUtils>k__BackingField").cast<Obj*>();
    static auto cardMetadata=BNM::Class("PvZCards.Engine.Dtos","Play").GetField("CardId").cast<int>();
    auto utilityField=utilityMetadata;auto cardField=cardMetadata;
    if(!current.play || !current.model)return;
    Obj* card=EntityAccess::Find(current.model,cardField[current.play].Get());
    if(!card)return;
    Obj* utility=utilityField[validator].Get();if(!utility)return;
    const bool customRequirements=usesRequirements && usesRequirements(current.model,card);
    bool targeted=false;Obj* ability=Select(utility,card,nullptr,&targeted);
    // Keep ordinary stock plays out of our scoped override. Only native
    // requirements or a verified manual on-play candidate need the snapshot.
    if(!customRequirements && !IsManualPlayEffect(ability))return;
    current.card=cardField[current.play].Get();current.ability=ability;current.targeted=targeted;current.resolved=true;
}
struct Scope {
    Context previous;
    Scope(Obj* model,Obj* play):previous(current){current={model,play};before.Notify(model,play);}
    ~Scope(){current=previous;}
};
}
