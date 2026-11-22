#include "abilities/RandomKeywordPolicy.hpp"
#include "engine/EffectDurations.hpp"
#pragma once
#include "abilities/CustomAbilityIds.hpp"
#include "history/DomainRecords.hpp"
#include <cstdio>
#include "core/Performance.hpp"
#include "core/AbilityRegistry.hpp"
#include "engine/MatchAccess.hpp"
namespace CustomComponents::Abilities::RandomKeyword {
using Obj=Engine::Match::Obj;
inline constexpr int Id=CustomAbilityIds::ToNative(CustomAbilityIds::Id::RandomKeyword);
inline constexpr const char* ledger="sprig:rk=";
using KeywordPolicy::Option;using KeywordPolicy::Given;using KeywordPolicy::options;
inline BNM::Field<int> grantType,grantDuration,removeType,removeDuration,effectTarget,effectSource;
inline BNM::Field<Obj*> grantModel,removeModel;
inline BNM::Method<int> grantValueGet;
inline BNM::Method<void> grantValueSet;
inline bool Initialize(){
    auto grant=BNM::Class("PvZCards.Engine.Effects","GrantAbilityEffect");
    auto remove=BNM::Class("PvZCards.Engine.Effects","RemoveGrantAbilityEffect");
    auto effect=BNM::Class("PvZCards.Engine","Effect");
    grantType=grant.GetField("GrantableAbilityType").cast<int>();
    grantDuration=grant.GetField("Duration").cast<int>();
    grantValueGet=grant.GetMethod("get_AbilityValue",0).cast<int>();
    grantValueSet=grant.GetMethod("set_AbilityValue",1).cast<void>();
    removeType=remove.GetField("GrantableAbilityType").cast<int>();
    removeDuration=remove.GetField("Duration").cast<int>();
    grantModel=BNM::Class("PvZCards.Core.Systems","GrantAbilitySystem").GetField("<Entities>k__BackingField").cast<Obj*>();
    removeModel=BNM::Class("PvZCards.Core.Systems","RemoveGrantAbilitySystem").GetField("<Model>k__BackingField").cast<Obj*>();
    effectTarget=effect.GetField("TargetId").cast<int>();effectSource=effect.GetField("SourceId").cast<int>();
    return grantType.IsValid()&&grantDuration.IsValid()&&grantValueGet.IsValid()&&grantValueSet.IsValid()&&removeType.IsValid()&&removeDuration.IsValid()&&grantModel.IsValid()&&removeModel.IsValid()&&effectTarget.IsValid()&&effectSource.IsValid();
}
inline std::vector<Given> GivenTo(Obj* target) {std::string value;return Engine::Match::Tag(target,ledger,value)?KeywordPolicy::Decode(value):std::vector<Given>{};}
inline bool Save(Obj* target,const std::vector<Given>& entries) {
    const auto before=KeywordPolicy::Encode(GivenTo(target));const auto after=KeywordPolicy::Encode(entries);
    if(!Engine::Match::SetTag(target,ledger,after))return false;
    History::AbilityPayload payload;payload.ability=Id;payload.encoding="random-keyword-policy-v1";payload.before=before;payload.after=after;
    History::Domain::Ability(target,std::move(payload));return true;
}
inline bool Has(Obj* target,const Option& option) {
    Obj* component=Engine::EntityAccess::Component(target,option.component);if(!component) return false;
    auto method=BNM::Class("PvZCards.Engine.Components",option.type==19?"Armor":"GrantableAbility").GetMethod(option.type==19?"CalculateArmor":"IsActive",0);
    return method.IsValid() && (option.type==19?method.cast<int>()[component]()>0:method.cast<bool>()[component]());
}
inline void* Placeholder(void*,bool){return nullptr;}
inline bool Register() {
    if(auto* entry=AbilityRegistry::Find(Id)) return std::string_view(entry->name)=="RandomKeyword";
    return AbilityRegistry::Register({Id,"RandomKeyword",AbilityStorage::Grantable,Placeholder,nullptr});
}
// A rewrite exists only during one execution. The queued effect remains a
// RandomKeyword effect for later executions and deep copies.
template<class Run> void Grant(void* system,void* effectPointer,Run run) {
    Obj* effect=static_cast<Obj*>(effectPointer);
    if(!effect || !grantType.IsValid() || grantType[effect].Get()!=Id) {run();return;}
    if(!system || !grantModel.IsValid() || !effectTarget.IsValid() || !effectSource.IsValid() || !grantDuration.IsValid() || !grantValueGet.IsValid() || !grantValueSet.IsValid())return;
    Obj* target=Engine::EntityAccess::Find(grantModel[system].Get(),effectTarget[effect].Get());if(!target)return;
    int value=grantValueGet[effect](),pool=value,source=effectSource[effect].Get(),duration=grantDuration[effect].Get();
    if(source<0 || !Engine::EffectDurations::Known(duration))return;
    if(pool==0) pool=Engine::EntityAccess::Component(target,"Zombies")?63:15;
    if(pool<0)return;
    auto open=KeywordPolicy::Eligible(pool,[&](const auto& option){return Has(target,option);});
    if(open.empty())return;
    int roll=Engine::Match::Roll(target,int(open.size()));if(roll<0){LOG_ERR("CustomComponents/RandomKeyword: match RNG unavailable; grant rejected");return;}
    const auto chosen=*open[roll];auto prior=GivenTo(target);
    prior.erase(std::remove_if(prior.begin(),prior.end(),[&](auto entry){for(const auto& option:options)if(option.type==entry.type)return !Has(target,option);return true;}),prior.end());
    auto given=prior;given.push_back({source,chosen.type,duration});
    if(!Save(target,given))return;
    grantType[effect].Set(chosen.type);grantValueSet[effect](chosen.value);
    struct Restore {Obj* effect;BNM::Field<int> type;BNM::Method<void> set;int value;~Restore(){type[effect].Set(Id);set[effect](value);}} restore{effect,grantType,grantValueSet,value};
    run();
    if(!Has(target,chosen)) Save(target,prior);
}
template<class Run> void Remove(void* system,void* effectPointer,Run run) {
    Obj* effect=static_cast<Obj*>(effectPointer);auto ability=removeType;
    if(!effect || !ability.IsValid() || ability[effect].Get()!=Id){run();return;}
    if(!system || !removeModel.IsValid() || !effectTarget.IsValid() || !effectSource.IsValid())return;
    Obj* target=Engine::EntityAccess::Find(removeModel[system].Get(),effectTarget[effect].Get());auto given=GivenTo(target);int source=effectSource[effect].Get();
    int duration=removeDuration.IsValid()?removeDuration[effect].Get():0;
    if(auto selected=KeywordPolicy::Take(given,source,duration)) {
        int chosen=*selected;if(!Save(target,given))return;
        ability[effect].Set(chosen);struct Restore{Obj* effect;BNM::Field<int> field;~Restore(){field[effect].Set(Id);}} restore{effect,ability};run();return;
    }
}
inline void Expire(Obj* model,int duration) {
    Core::Performance::Scope timing(Core::Performance::Area::KeywordExpiry);
    if(!model)return;static auto field=BNM::Class("PvZCards.Engine","EntityModel").GetField("Entities").cast<BNM::Structures::Mono::List<Obj*>*>();
    auto f=field;auto* list=f.IsValid()?f[model].Get():nullptr;if(!Engine::EntityAccess::Valid(list))return;
    for(int i=0;i<list->size;++i){Obj* entity=list->items->m_Items[i];auto given=GivenTo(entity);auto prior=given.size();given.erase(std::remove_if(given.begin(),given.end(),[&](auto entry){return entry.duration==duration;}),given.end());if(prior!=given.size())Save(entity,given);}
}
}



