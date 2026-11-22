#pragma once
#include "history/RecordWriter.hpp"

namespace CustomComponents::History::Domain {
using Obj=Writer::Obj;
inline std::uint64_t Emit(Obj* model,Kind kind,Payload payload,int card=-1,
                         std::uint64_t correlation=0,Stage stage=Stage::Committed){
    Record record;
    record.kind=kind;record.payload=std::move(payload);record.entityId=card;
    record.visual=VisualPolicy::NoVisual;record.correlationId=correlation;record.stage=stage;
    return Writer::Commit(std::move(record),model);
}
inline void Ability(Obj* card,AbilityPayload payload){
    if(!card || payload.before==payload.after)return;
    payload.card=Engine::Match::Id(card);
    Emit(Writer::Model(card),Kind::AbilityChanged,std::move(payload),Engine::Match::Id(card));
}
inline void Damage(Obj* model,const char* rule,const char* phase,int target,
                   int before,int after,int originalTarget=-1,bool cancelled=false){
    Emit(model,Kind::DamageRule,DamageRulePayload{rule,phase,originalTarget,target,before,after,cancelled},target);
}
inline void Presentation(Obj* model,const char* system,const char* phase,
                         std::vector<int> entities,std::uint64_t correlation=0,
                         float first=0,float second=0,float seconds=0){
    const int card=entities.empty()?-1:entities.front();
    Emit(model,Kind::PresentationEvent,PresentationPayload{system,phase,std::move(entities),first,second,seconds},card,correlation);
}
} // namespace CustomComponents::History::Domain
