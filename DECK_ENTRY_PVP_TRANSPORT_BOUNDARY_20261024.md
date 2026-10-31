# PvP physical deck-entry identity: protocol boundary

Introduces `DeckEntryWireIdentity.hpp`, with a fixed nine-word, versioned
identity and receiver-side validation. `InDeckQuery.hpp` exposes the model-aware
capture/validation wrappers. No stock play fields are repurposed.

The encoded owner, ordered-pool ordinal, entry index, card GUID and ordered GUID
fingerprint identify a single *current* physical deck copy. Live entity IDs are
not encoded, because entity allocation can differ among simulation copies. A
receiver must separately authenticate the issuing player's deck owner, validate
the ordered-pool fingerprint and entry, and resolve entity IDs from its own
executing model. If the resolved entry is virtual, it returns
`RequiresMaterialization` rather than guessing an entity ID.

**This is not an enabled multiplayer action.** The game stock `Play` serializer
and `SelectTarget` do not expose a spare safe deck-entry payload. A dedicated
message/command, authenticated owner mapping, server-side dispatch and the
stock card-creation transfer operation must be implemented and validated before
using the identity for effects or multiplayer. In particular, do not serialize
these words inside `Play.Targets` or `Play.CardId` by default, and do not
create entities during candidate enumeration.

Tests: `tests/DeckEntryWireIdentityRegression.cpp`, including two identical
GUID copies, an independent receiver's entity-ID changes, wrong-owner rejection,
shuffle/fingerprint mismatch, ambiguous IDs and malformed payload.
