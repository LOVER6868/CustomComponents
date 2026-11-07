#pragma once

namespace CustomComponents::Systems::ReturnToLaneMoveFilter {
template<class Model,class FindEntity,class HasComponent>
bool ShouldSuppress(Model model,int listenerId,int originId,FindEntity find,HasComponent has) {
    if(!model || listenerId<0 || originId<0) return false;
    const auto listener=find(model,listenerId);
    if(!listener || !has(listener,"MoveTrigger")) return false;
    const auto origin=find(model,originId);
    return origin && (has(origin,"ReturnFromDiscardPileToLaneEffectDescriptor") || has(origin,"PlayFromDeckToLaneEffectDescriptor"));
}
}
