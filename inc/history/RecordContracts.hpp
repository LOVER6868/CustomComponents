#pragma once
#include "history/RecordTypes.hpp"
namespace CustomComponents::History {
inline bool ValidPayload(const Record& record){
    switch(record.kind){
    case Kind::SwapTransaction:return std::holds_alternative<SwapPayload>(record.payload);
    case Kind::ZoneTransfer:{
        auto* p=std::get_if<TransferPayload>(&record.payload);
        return p && p->card>=0 && p->to>=0 && record.to!=Origin::Unknown;
    }
    case Kind::CombatEncounter:{
        auto* p=std::get_if<CombatPayload>(&record.payload);
        return p && !p->phase.empty() && p->first>=0 && (p->laneMode || p->second>=0) &&
            p->attackers.size()==p->defenders.size() && p->attackers.size()==p->amounts.size();
    }
    case Kind::AbilityChanged:{auto* p=std::get_if<AbilityPayload>(&record.payload);return p && p->card>=0 && p->ability>=0;}
    case Kind::DeathCancelled:{auto* p=std::get_if<DeathPayload>(&record.payload);return p && p->card>=0;}
    case Kind::DamageRule:{auto* p=std::get_if<DamageRulePayload>(&record.payload);return p && !p->rule.empty() && !p->phase.empty();}
    case Kind::HealingRule:{auto* p=std::get_if<HealingPayload>(&record.payload);return p && p->card>=0 && p->requested>=0 && p->restored>=0 && p->bonus>=0;}
    case Kind::PresentationEvent:{auto* p=std::get_if<PresentationPayload>(&record.payload);return p && !p->system.empty() && !p->phase.empty();}
    default:return true; // Older record kinds keep their existing contracts.
    }
}
}
