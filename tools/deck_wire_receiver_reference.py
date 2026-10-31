"""DEK1 receiver-side reference for PvZ Heroes modded deck-entry selection.

This is NOT a registered endpoint or an enabled PvP protocol extension. It
implements the on-wire identity/validation part of
inc/queries/DeckEntryWireIdentity.hpp, not simulation card materialization.

Integrators must authenticate the issuing player with the existing match
session, map that identity to the correct deck owner, and perform all mutation
inside the authoritative simulator. Never trust the owner's number in a packet.
"""

from dataclasses import dataclass
from enum import Enum
from typing import Iterable, Sequence

MAGIC = 0x44454B31  # DEK1
VERSION = 1
MASK32 = (1 << 32) - 1
MASK64 = (1 << 64) - 1
FNV_OFFSET = 14695981039346656037
FNV_PRIME = 1099511628211


@dataclass(frozen=True)
class Entry:
    owner: int
    pool: int
    index: int
    guid: int
    entity_id: int  # -1 means not materialized; never a Play.Targets value


@dataclass(frozen=True)
class Intent:
    owner: int
    pool: int
    index: int
    guid: int
    pool_fingerprint: int


class Status(str, Enum):
    READY_ENTITY = "ready_entity"
    REQUIRES_MATERIALIZATION = "requires_materialization"
    WRONG_OWNER = "wrong_owner"
    POOL_CHANGED = "pool_changed"
    ENTRY_CHANGED = "entry_changed"
    AMBIGUOUS_ENTITY = "ambiguous_entity"
    INVALID = "invalid"


@dataclass(frozen=True)
class Result:
    status: Status
    entity_id: int = -1
    entry: Entry | None = None


def _mix(hash_value: int, value: int) -> int:
    for shift in (0, 8, 16, 24):
        hash_value = ((hash_value ^ ((value >> shift) & 0xFF)) * FNV_PRIME) & MASK64
    return hash_value


def fingerprint_guid_pool(entries: Iterable[Entry], owner: int, pool: int) -> int:
    if owner < 0 or pool < 0:
        return 0
    digest = FNV_OFFSET
    count = 0
    # Input must use the authoritative OrderedCardPool iteration order.
    for e in entries:
        if e.owner != owner or e.pool != pool:
            continue
        digest = _mix(digest, e.index & MASK32)
        digest = _mix(digest, e.guid & MASK32)
        count += 1
    return _mix(digest, count)


def capture(entries: Sequence[Entry], selected: Entry) -> Intent:
    if min(selected.owner, selected.pool, selected.index) < 0 or selected.guid <= 0:
        raise ValueError("invalid physical deck entry")
    if sum(e.owner == selected.owner and e.pool == selected.pool and e.index == selected.index
           and e.guid == selected.guid for e in entries) != 1:
        raise ValueError("ambiguous or missing entry")
    return Intent(selected.owner, selected.pool, selected.index, selected.guid,
                  fingerprint_guid_pool(entries, selected.owner, selected.pool))


def encode(intent: Intent) -> tuple[int, ...]:
    if min(intent.owner, intent.pool, intent.index) < 0 or intent.guid <= 0 or not intent.pool_fingerprint:
        raise ValueError("invalid identity")
    if any(x > 0x7FFFFFFF for x in (intent.owner, intent.pool, intent.index, intent.guid)):
        raise ValueError("out of range")
    return (MAGIC, VERSION, intent.owner, intent.pool, intent.index, intent.guid,
            intent.pool_fingerprint & MASK32, (intent.pool_fingerprint >> 32) & MASK32,
            MAGIC ^ VERSION)


def decode(words: Sequence[int]) -> Intent | None:
    if len(words) != 9 or any(not isinstance(w, int) or not 0 <= w <= MASK32 for w in words):
        return None
    if words[0] != MAGIC or words[1] != VERSION or words[8] != (MAGIC ^ VERSION):
        return None
    owner, pool, index, guid = words[2:6]
    if any(n > 0x7FFFFFFF for n in (owner, pool, index, guid)) or guid == 0:
        return None
    fingerprint = words[6] | (words[7] << 32)
    if not fingerprint:
        return None
    return Intent(owner, pool, index, guid, fingerprint)


def validate_receiver(entries: Sequence[Entry], intent: Intent, authenticated_owner: int) -> Result:
    if authenticated_owner < 0 or intent.owner != authenticated_owner:
        return Result(Status.WRONG_OWNER)
    if intent.pool < 0 or intent.index < 0 or intent.guid <= 0 or not intent.pool_fingerprint:
        return Result(Status.INVALID)
    if fingerprint_guid_pool(entries, authenticated_owner, intent.pool) != intent.pool_fingerprint:
        return Result(Status.POOL_CHANGED)
    matches = [e for e in entries if (e.owner, e.pool, e.index) ==
               (authenticated_owner, intent.pool, intent.index)]
    if len(matches) != 1 or matches[0].guid != intent.guid:
        return Result(Status.ENTRY_CHANGED)
    selected = matches[0]
    if selected.entity_id < 0:
        return Result(Status.REQUIRES_MATERIALIZATION, entry=selected)
    entity_matches = [e for e in entries if e.owner == authenticated_owner and
                      e.entity_id == selected.entity_id]
    if len(entity_matches) != 1 or entity_matches[0] != selected:
        return Result(Status.AMBIGUOUS_ENTITY)
    return Result(Status.READY_ENTITY, selected.entity_id, selected)
