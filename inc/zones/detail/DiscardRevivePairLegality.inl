// Shared, read-only *physical* legality test for a discard -> lane return.
// Deliberately does NOT evaluate a PickFromThree primary/secondary filter: that
// requires the originating effect query context and the same simulation model.
// No remove, place, instantiate, record, trigger or pooled list retention here.
struct DiscardRevivePairLegality {
    bool legal = false;
    bool discard = false;
    bool cleanSource = false;
    bool laneExists = false;
    bool capacity = false;
    int ownerFighters = -1;
    const char* reason = "UNAVAILABLE";
};

inline DiscardRevivePairLegality CheckDiscardRevivePair(
    Obj* model, int cardId, int laneId, int expectedPlayerId) {
    DiscardRevivePairLegality result;
    if (!model || cardId < 0 || laneId < 0) { result.reason = "INVALID_ARGUMENT"; return result; }
    DiscardContext context;
    if (!ResolveDiscardContext(model, cardId, context) || !context.card ||
        !context.discardPile || context.playerId != expectedPlayerId) {
        result.reason = "DISCARD_CONTEXT"; return result;
    }
    result.discard = DiscardContains(context.discardPile, cardId);
    result.cleanSource = result.discard &&
        FindComponent(context.card, "DiscardPileElement") &&
        !FindComponent(context.card, "LaneElement") &&
        !FindComponent(context.card, "HandElement") &&
        !FindComponent(context.card, "DeckElement");
    if (!result.cleanSource) { result.reason = "SOURCE_ZONE"; return result; }
    Obj* laneEntity = FindEntity(model, laneId);
    Obj* lane = laneEntity ? FindComponent(laneEntity, "Lane") : nullptr;
    result.laneExists = lane != nullptr;
    if (!lane) {
        result.reason = "LANE_UNAVAILABLE"; return result;
    }
    // Environments occupy a separate slot. The stock placer replaces the
    // previous environment; friendly fighter capacity is irrelevant.
    const bool environment = FindComponent(context.card, "Environment") != nullptr;
    const bool fighter = FindComponent(context.card, "Fighter") != nullptr;
    const auto kind=ReturnToLanePolicy::Classify(fighter,environment);
    if(kind==ReturnToLanePolicy::CardKind::Unsupported) { result.reason = "CARD_TYPE"; return result; }
    if(environment) {
        result.capacity = laneEnvironmentId.IsValid() && environmentPlaceCard.IsValid() &&
            environmentPlacerCtor.IsValid() && createEnvironmentRecordSetCard.IsValid();
        result.legal = result.capacity;
        result.reason = result.legal ? "LEGAL_ENVIRONMENT_PAIR" : "ENVIRONMENT_UNAVAILABLE";
        return result;
    }
    if(!laneGetNumFightersForPlayer.IsValid()) { result.reason = "CAPACITY_UNAVAILABLE"; return result; }
    result.ownerFighters = laneGetNumFightersForPlayer[lane](expectedPlayerId);
    if (result.ownerFighters < 0 || result.ownerFighters >= 2) {
        result.reason = "CAPACITY"; return result;
    }
    if (result.ownerFighters == 1) {
        const bool incomingPlant = FindComponent(context.card, "Plants") != nullptr;
        const bool incomingTeamup = incomingPlant &&
            FindComponent(context.card, "Teamup") != nullptr;
        bool residentTeamup = false;
        auto getResidents = BNM::Class("PvZCards.Engine.Components", "Lane")
            .GetMethod("GetFightersForPlayer", 1).cast<MonoList<int>*>();
        if (getResidents.IsValid()) {
            auto* residents = getResidents[lane](expectedPlayerId);
            if (residents && residents->items && residents->size == 1) {
                Obj* resident = FindEntity(model, residents->items->m_Items[0]);
                residentTeamup = resident && incomingPlant &&
                    FindComponent(resident, "Plants") &&
                    FindComponent(resident, "Teamup");
            }
        }
        if (!ReturnToLanePolicy::HasSlot(kind,result.ownerFighters,incomingPlant,incomingTeamup,residentTeamup)) {
            result.reason = "TEAMUP"; return result;
        }
    }
    result.capacity = true;
    result.legal = true;
    result.reason = "LEGAL_PHYSICAL_PAIR";
    return result;
}
