#pragma once

// EntityModel.Entities is a managed list of entity objects, not a reliable
// substitute for a GetEntityById call on a partially-mutated match. For the
// optional PvP insertion observer, search that list directly and refuse
// missing/duplicate IDs. This performs no IL2CPP method invocation.
namespace CustomComponents::Diagnostics::PvpInsertionSafeLookup {

template<typename Entity, typename ReadId>
inline Entity* FindUnique(Entity* const* entries, int count, int wantedId,
                          ReadId readId) {
    if (!entries || count <= 0 || count > 200000 || wantedId <= 0)
        return nullptr;
    Entity* found = nullptr;
    for (int i = 0; i < count; ++i) {
        Entity* entity = entries[i];
        if (!entity || readId(entity) != wantedId) continue;
        if (found) return nullptr; // Ambiguous IDs: fail closed.
        found = entity;
    }
    return found;
}

} // namespace CustomComponents::Diagnostics::PvpInsertionSafeLookup
