#include "queries/DeckEntryWireIdentity.hpp"
#include <cassert>
using namespace CustomComponents::Queries;
int main(){
 std::vector<DeckEntries::Selection> sender={{5,0,0,673,-1},{5,0,1,673,-1},{5,0,2,317,41},{7,0,0,710,-1}};
 DeckWire::Intent i;
 assert(DeckWire::Capture(sender,sender[1],i));
 const auto wire=DeckWire::Encode(i); DeckWire::Intent decoded;
 assert(DeckWire::Decode(wire,decoded));
 // Entity ids are intentionally NOT part of wire identity.
 auto receiver=sender; receiver[2].entityId=88;
 assert(DeckWire::ValidateReceiver(receiver,decoded,5).status==DeckWire::Status::RequiresMaterialization);
 assert(DeckWire::ValidateReceiver(receiver,decoded,7).status==DeckWire::Status::WrongOwner);
 receiver[1].entityId=123;
 assert(DeckWire::ValidateReceiver(receiver,decoded,5).status==DeckWire::Status::ReadyEntity);
 // The receiver can allocate different live IDs without changing DEK1 identity.
 auto diffModel=sender; diffModel[2].entityId=987;
 auto liveIntent=DeckWire::Intent{};
 assert(DeckWire::Capture(sender,sender[2],liveIntent));
 auto liveResolved=DeckWire::ValidateReceiver(diffModel,liveIntent,5);
 assert(liveResolved.status==DeckWire::Status::ReadyEntity && liveResolved.entityId==987);
 assert(DeckEntries::ResolveAcrossModels(diffModel,5,0,0,673).status==
        DeckEntries::ResolveStatus::VirtualEntry);
 auto colliding=diffModel; colliding[1].entityId=987;
 assert(DeckEntries::ResolveAcrossModels(colliding,5,0,2,317).status==
        DeckEntries::ResolveStatus::AmbiguousEntity);
 assert(DeckEntries::ResolveAcrossModels(diffModel,7,0,2,317).status==
        DeckEntries::ResolveStatus::EntryChanged);
 receiver[0].guid=999;
 assert(DeckWire::ValidateReceiver(receiver,decoded,5).status==DeckWire::Status::PoolChanged);
 auto bad=wire; bad[0]=0;
 assert(!DeckWire::Decode(bad,decoded));
 auto duplicate=sender; duplicate[1].entityId=41;
 assert(DeckWire::ValidateReceiver(duplicate,i,5).status==DeckWire::Status::AmbiguousEntity);
}
