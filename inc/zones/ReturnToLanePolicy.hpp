#pragma once

namespace CustomComponents::Systems::ReturnToLanePolicy {
enum class CardKind { Unsupported, Fighter, Environment };
inline constexpr CardKind Classify(bool fighter,bool environment) {
    if(fighter==environment) return CardKind::Unsupported;
    return environment ? CardKind::Environment : CardKind::Fighter;
}
inline constexpr bool AllowsTerrain(CardKind kind,bool water,bool aquatic) {
    return kind==CardKind::Environment || (kind==CardKind::Fighter && (!water || aquatic));
}
// Environment replacement is handled by EnvironmentPlacer, outside the
// player-to-fighter map. Unknown fighter occupancy always fails closed.
inline constexpr bool HasSlot(CardKind kind,int occupants,bool plant,bool incomingTeamup,bool residentTeamup) {
    if(kind==CardKind::Environment) return true;
    if(kind!=CardKind::Fighter || occupants<0 || occupants>=2) return false;
    return occupants==0 || (plant && (incomingTeamup || residentTeamup));
}
}
