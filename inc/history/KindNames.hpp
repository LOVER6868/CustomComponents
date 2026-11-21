#pragma once
#include "history/RecordTypes.hpp"
namespace CustomComponents::History {
inline const char* KindName(Kind kind){
    switch(kind){
    case Kind::DiscardOutsidePlay:return "DiscardOutsidePlay";
    case Kind::DeckReorder:return "DeckReorder";
    case Kind::ReturnFromDiscard:return "ReturnFromDiscard";
    case Kind::ZoneTransfer:return "ZoneTransfer";
    case Kind::Selection:return "Selection";
    case Kind::StateChange:return "StateChange";
    case Kind::CardCreated:return "CardCreated";
    case Kind::Presentation:return "Presentation";
    case Kind::SwapTransaction:return "SwapTransaction";
    case Kind::VariableChanged:return "VariableChanged";
    case Kind::LevelChanged:return "LevelChanged";
    case Kind::SelectionRequest:return "SelectionRequest";
    case Kind::SelectionResolved:return "SelectionResolved";
    case Kind::CardMetadataChanged:return "CardMetadataChanged";
    case Kind::CombatEncounter:return "CombatEncounter";
    case Kind::AbilityChanged:return "AbilityChanged";
    case Kind::DeathCancelled:return "DeathCancelled";
    case Kind::DamageRule:return "DamageRule";
    case Kind::HealingRule:return "HealingRule";
    case Kind::PresentationEvent:return "PresentationEvent";
    }
    return "Unknown";
}
}
