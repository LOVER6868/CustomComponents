#include "queries/DeckEntryExecutionBridge.hpp"
#include <cassert>
#include <iostream>
using namespace CustomComponents::Queries;
int main() {
    const std::vector<DeckEntries::Selection> deck = {
        {10,0,0,673,-1}, {10,0,1,673,-1}, {10,0,2,317,44},
        {20,0,0,673,51}};
    auto virtualChoice=DeckExecution::Capture(deck,deck[1]);
    auto liveChoice=DeckExecution::Capture(deck,deck[2]);
    assert(DeckExecution::ValidateForExecution(deck,virtualChoice,10).status ==
           DeckExecution::Status::RequiresMaterialization);
    auto ready=DeckExecution::ValidateForExecution(deck,liveChoice,10);
    assert(ready.status==DeckExecution::Status::ReadyEntity && ready.entityId==44);
    assert(DeckExecution::ValidateForExecution(deck,liveChoice,20).status ==
           DeckExecution::Status::WrongOwner);
    auto moved=deck;
    // Unchanged copied snapshot must validate independent of its allocation.
    assert(DeckExecution::ValidateForExecution(moved,virtualChoice,10).status ==
           DeckExecution::Status::RequiresMaterialization);
    moved=deck; moved[1].guid=999;
    assert(DeckExecution::ValidateForExecution(moved,virtualChoice,10).status ==
           DeckExecution::Status::ModelMismatch);
    moved=deck; moved[1].entityId=62;
    assert(DeckExecution::ValidateForExecution(moved,virtualChoice,10).status ==
           DeckExecution::Status::ModelMismatch);
    moved=deck; moved.erase(moved.begin());
    assert(DeckExecution::ValidateForExecution(moved,virtualChoice,10).status ==
           DeckExecution::Status::ModelMismatch);
    moved=deck; moved[3].guid=999; // other owner is isolated
    assert(DeckExecution::ValidateForExecution(moved,liveChoice,10).status ==
           DeckExecution::Status::ReadyEntity);
    moved=deck; moved.push_back({10,0,3,317,44});
    assert(DeckExecution::ValidateForExecution(moved,liveChoice,10).status ==
           DeckExecution::Status::ModelMismatch);
    assert(deck[1].entityId == -1 && deck.size()==4);
    std::cout << "PASS: PvP execution boundary, snapshot fingerprint, ownership, virtual and live\n";
}
