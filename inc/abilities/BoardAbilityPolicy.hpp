#pragma once
namespace CustomComponents::Abilities::BoardPolicy {
inline bool MayTrigger(bool stockAllows,bool boardAbility,bool spent){return stockAllows && !(boardAbility && spent);}
}
