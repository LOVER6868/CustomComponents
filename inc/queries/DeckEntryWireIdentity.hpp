#pragma once
// Explicit deck-entry identity protocol for a FUTURE dedicated gameplay
// command. Do not place these words inside a stock SelectTarget Play: Targets
// contain entity IDs and CardId has stock meanings.
#include "queries/DeckEntryExecutionBridge.hpp"
#include <array>
#include <cstdint>
#include <vector>

namespace CustomComponents::Queries::DeckWire {
using DeckEntries::Selection;
inline constexpr std::uint32_t kMagic=0x44454B31u; // DEK1
inline constexpr std::uint32_t kVersion=1u;
// Fixed-width payload, independent of struct padding/host endianness.
using Words=std::array<std::uint32_t,9>;
struct Intent {
    int ownerEntityId=-1;
    int poolOrdinal=-1;
    int entryIndex=-1;
    int guid=-1;
    std::uint64_t orderedGuidFingerprint=0;
};
inline std::uint64_t FingerprintGuidPool(const std::vector<Selection>& snapshot,int owner,int pool) {
    if(owner<0 || pool<0) return 0;
    std::uint64_t h=UINT64_C(14695981039346656037);
    std::uint32_t count=0;
    for(const auto& e:snapshot) {
        if(e.ownerEntityId!=owner || e.poolOrdinal!=pool) continue;
        h=DeckExecution::Mix(h,static_cast<std::uint32_t>(e.entryIndex));
        h=DeckExecution::Mix(h,static_cast<std::uint32_t>(e.guid));
        ++count;
    }
    return DeckExecution::Mix(h,count);
}
inline bool Capture(const std::vector<Selection>& snapshot,const Selection& selected,Intent& out) {
    if(selected.ownerEntityId<0 || selected.poolOrdinal<0 || selected.entryIndex<0 || selected.guid<=0) return false;
    const auto r=DeckEntries::ResolveSaved(snapshot,selected);
    if(r.status!=DeckEntries::ResolveStatus::VirtualEntry && r.status!=DeckEntries::ResolveStatus::FoundEntity) return false;
    const auto digest=FingerprintGuidPool(snapshot,selected.ownerEntityId,selected.poolOrdinal);
    if(!digest) return false;
    out={selected.ownerEntityId,selected.poolOrdinal,selected.entryIndex,selected.guid,digest};
    return true;
}
inline Words Encode(const Intent& intent) {
    return {kMagic,kVersion,static_cast<std::uint32_t>(intent.ownerEntityId),
        static_cast<std::uint32_t>(intent.poolOrdinal),static_cast<std::uint32_t>(intent.entryIndex),
        static_cast<std::uint32_t>(intent.guid),static_cast<std::uint32_t>(intent.orderedGuidFingerprint),
        static_cast<std::uint32_t>(intent.orderedGuidFingerprint>>32),kMagic^kVersion};
}
inline bool Decode(const Words& words,Intent& out) {
    if(words[0]!=kMagic || words[1]!=kVersion || words[8]!=(kMagic^kVersion) ||
       words[2]>0x7fffffffu || words[3]>0x7fffffffu || words[4]>0x7fffffffu ||
       words[5]==0 || words[5]>0x7fffffffu) return false;
    out={static_cast<int>(words[2]),static_cast<int>(words[3]),static_cast<int>(words[4]),
         static_cast<int>(words[5]),static_cast<std::uint64_t>(words[6]) |
         (static_cast<std::uint64_t>(words[7])<<32)};
    return out.orderedGuidFingerprint!=0;
}
enum class Status { ReadyEntity, RequiresMaterialization, WrongOwner, PoolChanged, EntryChanged, AmbiguousEntity, Invalid };
struct Result {Status status=Status::Invalid; int entityId=-1; Selection entry{};};
// Validate in the EXECUTING simulation, with the deck owner independently
// authenticated from the command's issuing player. Never trust owner supplied
// in a client packet by itself.
inline Result ValidateReceiver(const std::vector<Selection>& snapshot,const Intent& intent,int authenticatedOwner) {
    if(authenticatedOwner<0 || intent.ownerEntityId!=authenticatedOwner) return {Status::WrongOwner};
    if(intent.poolOrdinal<0 || intent.entryIndex<0 || intent.guid<=0 || !intent.orderedGuidFingerprint) return {};
    if(FingerprintGuidPool(snapshot,authenticatedOwner,intent.poolOrdinal)!=intent.orderedGuidFingerprint)
        return {Status::PoolChanged};
    const auto resolved=DeckEntries::ResolveAcrossModels(
        snapshot,authenticatedOwner,intent.poolOrdinal,intent.entryIndex,intent.guid);
    switch(resolved.status) {
        case DeckEntries::ResolveStatus::FoundEntity:
            return {Status::ReadyEntity,resolved.entityId,resolved.entry};
        case DeckEntries::ResolveStatus::VirtualEntry:
            return {Status::RequiresMaterialization,-1,resolved.entry};
        case DeckEntries::ResolveStatus::AmbiguousEntity:
            return {Status::AmbiguousEntity};
        default: return {Status::EntryChanged};
    }
}
} // namespace CustomComponents::Queries::DeckWire
