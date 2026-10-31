#pragma once
// Deterministic, read-only validation boundary for actions against ordered-deck
// entries. It intentionally does NOT encode a selection as stock Play.Targets:
// those fields are entity IDs, not pool positions. Materialization must be
// performed by a separately verified stock simulation operation.
#include "queries/DeckEntryResolver.hpp"
#include <vector>
#include <cstdint>

namespace CustomComponents::Queries::DeckExecution {
using DeckEntries::Selection;

enum class Status {
    ReadyEntity, RequiresMaterialization, EntryChanged, AmbiguousEntity,
    InvalidSelection, WrongOwner, ModelMismatch
};
struct Request {
    int expectedOwnerEntityId = -1;
    Selection selected{};
    // A per-model, per-owner ordered-deck checksum guards against resolving a
    // pool index after an unnoticed shuffle, draw, insertion or replacement.
    std::uint64_t poolFingerprint = 0;
};
struct Result {
    Status status = Status::InvalidSelection;
    int entityId = -1;
    Selection entry{};
};
inline std::uint64_t Mix(std::uint64_t h, std::uint32_t value) {
    // FNV-1a on the four bytes makes hashing independent of native endianness.
    for (int shift=0; shift<32; shift+=8) {
        h ^= (value >> shift) & 0xffu;
        h *= UINT64_C(1099511628211);
    }
    return h;
}
inline std::uint64_t FingerprintOwner(const std::vector<Selection>& entries,
                                      int ownerEntityId) {
    if (ownerEntityId < 0) return 0;
    std::uint64_t h=UINT64_C(14695981039346656037);
    unsigned count=0;
    for (const auto& e : entries) {
        if (e.ownerEntityId != ownerEntityId) continue;
        h=Mix(h,static_cast<std::uint32_t>(e.poolOrdinal));
        h=Mix(h,static_cast<std::uint32_t>(e.entryIndex));
        h=Mix(h,static_cast<std::uint32_t>(e.guid));
        h=Mix(h,static_cast<std::uint32_t>(e.entityId));
        ++count;
    }
    h=Mix(h,count);
    return h;
}
inline Request Capture(const std::vector<Selection>& snapshot,
                       const Selection& selection) {
    Request request;
    request.expectedOwnerEntityId=selection.ownerEntityId;
    request.selected=selection;
    request.poolFingerprint=FingerprintOwner(snapshot,selection.ownerEntityId);
    return request;
}
inline Result ValidateForExecution(const std::vector<Selection>& executingSnapshot,
                                    const Request& request,
                                    int executingOwnerEntityId) {
    if (request.expectedOwnerEntityId < 0 || executingOwnerEntityId < 0 ||
        request.selected.ownerEntityId < 0 || request.selected.poolOrdinal < 0 ||
        request.selected.entryIndex < 0 || request.selected.guid <= 0 ||
        request.selected.entityId < -1 || request.poolFingerprint == 0)
        return {};
    if (request.expectedOwnerEntityId != executingOwnerEntityId ||
        request.selected.ownerEntityId != executingOwnerEntityId)
        return {Status::WrongOwner};
    if (FingerprintOwner(executingSnapshot,executingOwnerEntityId) !=
        request.poolFingerprint)
        return {Status::ModelMismatch};
    const auto resolution=DeckEntries::ResolveSaved(executingSnapshot,request.selected);
    switch (resolution.status) {
        case DeckEntries::ResolveStatus::FoundEntity:
            return {Status::ReadyEntity,resolution.entityId,resolution.entry};
        case DeckEntries::ResolveStatus::VirtualEntry:
            return {Status::RequiresMaterialization,-1,resolution.entry};
        case DeckEntries::ResolveStatus::EntryChanged:
            return {Status::EntryChanged};
        case DeckEntries::ResolveStatus::AmbiguousEntity:
            return {Status::AmbiguousEntity};
        default: return {};
    }
}
} // namespace CustomComponents::Queries::DeckExecution
