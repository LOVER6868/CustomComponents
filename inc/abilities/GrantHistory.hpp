#pragma once
#include "history/DomainRecords.hpp"
#include "abilities/GrantableAbilityRuntime.hpp"
#include "core/AbilityRegistry.hpp"
#include "history/Serialization.hpp"

namespace CustomComponents::Abilities::GrantHistory {
using Obj=History::Writer::Obj;
// Compact ordered storage snapshots preserve source and duration, not just IsActive.
inline std::string Capture(Obj* card,const AbilityAdapter& adapter){
    if(!card || Core::Performance::aiDepth)return {};
    History::Wire::Object state;
    std::string rows="[";
    auto append=[&](History::Wire::Object row){if(rows.size()>1)rows+=',';rows+=row.Finish();};
    if(adapter.storage==AbilityStorage::Numeric){
        void* value=adapter.numericValue(card,false);if(!value)return "null";
        state.Add("value",NumericAbility::calculate(value));
        auto modifiers=NumericAbility::modifiersField;
        Obj* list=static_cast<Obj*>(modifiers[value].Get());
        auto count=list?BNM::Class(list).GetMethod("get_Count",0).cast<int>():BNM::Method<int>{};
        auto item=list?BNM::Class(list).GetMethod("get_Item",1).cast<Obj*>():BNM::Method<Obj*>{};
        const int size=count.IsValid()?count[list]():0;
        if(item.IsValid())for(int i=0;i<size;++i){
            Obj* modifier=item[list](i);if(!modifier)continue;
            auto group=NumericAbility::groupField;auto duration=NumericAbility::durationField;
            auto original=NumericAbility::originalAmount;auto amount=NumericAbility::additiveAmount;
            auto activation=NumericAbility::activationField;
            auto* key=group[modifier].Get();
            History::Wire::Object row;row.Add("group",key?key->str():std::string{});
            row.Add("duration",duration[modifier].Get());row.Add("original",original[modifier].Get());
            row.Add("additive",amount[modifier].Get());row.Add("activation",activation[modifier].Get());append(std::move(row));
        }
    }else{
        Obj* component=static_cast<Obj*>(adapter.component(card,false));if(!component)return "null";
        auto counters=GrantableAbilityRuntime::countersField;auto entries=GrantableAbilityRuntime::entriesField;
        Obj* group=static_cast<Obj*>(counters[component].Get());
        auto* list=group?static_cast<BNM::Structures::Mono::List<GrantableAbilityRuntime::CounterEntry>*>(entries[group].Get()):nullptr;
        state.Add("active",GrantableAbilityRuntime::Active(component));
        state.Add("publicComponent",Engine::EntityAccess::Component(card,adapter.name)!=nullptr);
        if(Engine::EntityAccess::Valid(list))for(int i=0;i<list->size;++i){
            const auto& counter=list->items->m_Items[i];History::Wire::Object row;
            row.Add("duration",counter.duration);row.Add("source",counter.sourceId);row.Add("value",counter.value);append(std::move(row));
        }
    }
    state.Raw("entries",rows+"]");return state.Finish();
}
inline void Changed(Obj* card,const AbilityAdapter& adapter,std::string before,
                    int source,int duration,int operation,bool expiry=false){
    if(Core::Performance::aiDepth)return;
    History::AbilityPayload payload;
    payload.ability=adapter.id;payload.source=source;payload.duration=duration;payload.operation=operation;payload.expired=expiry;
    payload.before=std::move(before);payload.after=Capture(card,adapter);
    History::Domain::Ability(card,std::move(payload));
}
}
