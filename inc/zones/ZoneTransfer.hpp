#pragma once
namespace CustomComponents::Zones {
enum class TransferPhase { Prepare, Remove, Place, Record, Publish, Complete };
struct TransferResult {
    TransferPhase phase=TransferPhase::Prepare;
    bool committed=false;
    bool recovered=false;
};
// Ops owns engine exception boundaries and verifies live zone membership.
// Never compensate a card that already belongs to an unexpected valid zone.
template<class Ops> TransferResult ExecuteTransfer(Ops& ops) {
    if(!ops.Prepare()) return {};
    if(!ops.Remove()) return {TransferPhase::Remove,false,ops.Recover()};
    if(!ops.Place()) return {TransferPhase::Place,false,ops.Recover()};
    if(!ops.Record()) return {TransferPhase::Record,true,false};
    ops.Publish();
    return {TransferPhase::Complete,true,false};
}
}
