#pragma once
#include "pick3/PickFromThreeCore.hpp"
namespace CustomComponents::Pick3 {
inline bool IsAutomaticUniqueChoice(const PreviewRequest& request) {
    return request.mode==SelectionMode::UniqueGuidTargets && request.requiredTargets==1 &&
        request.options.size()==1 && request.options[0].entityId>0 && request.options[0].cardGuid>0;
}
}
