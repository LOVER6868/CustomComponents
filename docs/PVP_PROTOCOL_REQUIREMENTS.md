# PvP implementation still required

## Why the stock transport is insufficient

The current stock `Play.Type=6` `Targets` list serializes live *entity IDs*.
It does not encode a CardDatabase selection's *card GUID*, a three-choice
subset, picker mode, random seed, or the local `PickedFromThreeQuery` cache.
The downloaded `pvzh-server` repository is a public community server project,
not evidence that official stock servers or both clients understand custom
PickFromThree. No server modifications were made to this artifact.

## Minimum authoritative protocol

The multiplayer transport or custom match server must agree on a versioned
PickFromThree feature and authoritative game-state support on BOTH clients:

1. During effect activation, identify `(matchId, effectEntityId, effectGroupId,
   selectingPlayerId, selectionMode, requiredTargets)`.
2. Generate/approve the same list of up to three eligible options from the
   authoritative state. For live selection, these contain runtime entity IDs
   and relevant GUID metadata. For CardDatabase selection, these contain GUIDs
   and query/zone constraints; recipients remain a **separate** field.
3. Send the list and a unique offer revision/nonce from authority to the
   selecting client. Never trust an unauthenticated locally generated offer.
4. Return selected choice **indices** or explicit `(entityId,cardGuid)` with
   offer revision and required count. Server validates owner, eligibility,
   duplicates, pool bounds, expiry and effect still waiting.
5. Server resolves recipient targets independently, attaches the selected
   CardDatabase GUIDs to the authoritative effect/query state, executes the
   effect, and replicates the resulting game state to both clients.
6. Support clear failure codes, idempotency/replay rejection, timeouts,
   disconnect/rollback behavior, and match reset cleanup. Only report UI
   success after authoritative completion; no manual PvE-like local state
   signal for remote actions.

The user's `SubmitInPVP` setting is honored and is not silently overridden.
That does **not** establish working CardDatabase PvP. A complete implementation
requires the authoritative handshake above. Canonical sampling removes one
client-only source of divergence but does not replace steps 1–6. Do not
claim functional PvP without a two-client/server acceptance test.
