#include "abilities/RandomKeywordPolicy.hpp"
#pragma once
#include <cstdio>
#include "core/Performance.hpp"
#include "core/AbilityRegistry.hpp"
#include "engine/MatchAccess.hpp"
namespace CustomComponents::Abilities::RandomKeyword {
using Obj=Engine::Match::Obj;
inline constexpr int Id=22;
inline constexpr const char* ledger="sprig:rk=";
using KeywordPolicy::Option;using KeywordPolicy::Given;using KeywordPolicy::options;
inline std::vector<Given> GivenTo(Obj* target) {std::string value;return Engine::Match::Tag(target,ledger,value)?KeywordPolicy::Decode(value):std::vector<Given>{};}
inline bool Save(Obj* target,const std::vector<Given>& entries) {return Engine::Match::SetTag(target,ledger,KeywordPolicy::Encode(entries));}
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
    static auto typeMetadata=BNM::Class("PvZCards.Engine.Effects","GrantAbilityEffect");static auto grantMetadata=typeMetadata.GetField("GrantableAbilityType").cast<int>();
    auto type=typeMetadata;auto grantType=grantMetadata;
    if(!effect || !grantType.IsValid() || grantType[effect].Get()!=Id) {run();return;}
    auto modelField=BNM::Class("PvZCards.Core.Systems","GrantAbilitySystem").GetField("<Entities>k__BackingField").cast<Obj*>();
    auto targetField=BNM::Class("PvZCards.Engine","Effect").GetField("TargetId").cast<int>();auto sourceField=BNM::Class("PvZCards.Engine","Effect").GetField("SourceId").cast<int>();
    auto durationField=type.GetField("Duration").cast<int>();auto get=type.GetMethod("get_AbilityValue",0).cast<int>();auto set=type.GetMethod("set_AbilityValue",1).cast<void>();
    if(!system || !modelField.IsValid() || !targetField.IsValid() || !sourceField.IsValid() || !durationField.IsValid() || !get.IsValid() || !set.IsValid()) return;
    Obj* target=Engine::EntityAccess::Find(modelField[system].Get(),targetField[effect].Get());if(!target)return;
    int value=get[effect](),pool=value,source=sourceField[effect].Get(),duration=durationField[effect].Get();
    if(source<0 || duration<0 || duration>2)return;
    if(pool==0) pool=Engine::EntityAccess::Component(target,"Zombies")?63:15;
    if(pool<0)return;
    auto open=KeywordPolicy::Eligible(pool,[&](const auto& option){return Has(target,option);});
    if(open.empty())return;
    int roll=Engine::Match::Roll(target,int(open.size()));if(roll<0){LOG_ERR("CustomComponents/RandomKeyword: match RNG unavailable; grant rejected");return;}
    const auto chosen=*open[roll];auto prior=GivenTo(target);
    prior.erase(std::remove_if(prior.begin(),prior.end(),[&](auto entry){for(const auto& option:options)if(option.type==entry.type)return !Has(target,option);return true;}),prior.end());
    auto given=prior;given.push_back({source,chosen.type,duration});
    if(!Save(target,given))return;
    grantType[effect].Set(chosen.type);set[effect](chosen.value);
    struct Restore {Obj* effect;BNM::Field<int> type;BNM::Method<void> set;int value;~Restore(){type[effect].Set(Id);set[effect](value);}} restore{effect,grantType,set,value};
    run();
    if(!Has(target,chosen)) Save(target,prior);
}
template<class Run> void Remove(void* system,void* effectPointer,Run run) {
    Obj* effect=static_cast<Obj*>(effectPointer);static auto typeMetadata=BNM::Class("PvZCards.Engine.Effects","RemoveGrantAbilityEffect");static auto abilityMetadata=typeMetadata.GetField("GrantableAbilityType").cast<int>();
    auto type=typeMetadata;auto ability=abilityMetadata;
    if(!effect || !ability.IsValid() || ability[effect].Get()!=Id){run();return;}
    auto modelField=BNM::Class("PvZCards.Core.Systems","RemoveGrantAbilitySystem").GetField("<Model>k__BackingField").cast<Obj*>();
    auto targetField=BNM::Class("PvZCards.Engine","Effect").GetField("TargetId").cast<int>();auto sourceField=BNM::Class("PvZCards.Engine","Effect").GetField("SourceId").cast<int>();
    if(!system || !modelField.IsValid() || !targetField.IsValid() || !sourceField.IsValid())return;
    Obj* target=Engine::EntityAccess::Find(modelField[system].Get(),targetField[effect].Get());auto given=GivenTo(target);int source=sourceField[effect].Get();
    auto durationField=type.GetField("Duration").cast<int>();int duration=durationField.IsValid()?durationField[effect].Get():0;
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



