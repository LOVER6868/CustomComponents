# PvP Sow Magic Beans runtime insertion — safety rollback (2026-10-26)

The earlier Step 2B experimental setting `MaterializeInsertedCardsPvp=true`
made four new virtual bean entries physical via the stock per-entry helper.
A same-session log captured:
`PVP_INSERT_RESULT exact=1 pool=6 guid=345 created=4 beforeNext=249 nextId=257`
followed by the opponent reportedly crashing/disconnecting. The supplied log is
from a surviving client and does **not** contain the opponent native crash trace;
this is evidence of a dangerous cross-client compatibility risk, not proof of
the precise crashing instruction.

This rollback ONLY changes the configuration to:

    [PvpDeckPrepopulation]
    MaterializeInsertedCardsPvp=false

This stops new PvP per-entry materialization writes and the PvP session binding.
The preexisting 88-card PvP prepopulation, dynamic fingerprints, stock card
insertion, PvE insertion repair, single shared hook, and Global PlayTrigger
model-scope hotfix are unchanged. Generated PvP entries can legitimately stay
virtual (`EntityId=-1`) until a proper multiplayer protocol is established.

Build from this source and reinstall on BOTH phones. Restart the game on BOTH
phones; do not continue matches that may already have divergent entity IDs.
Avoid enabling this switch again until tests prove symmetric allocation and
remote/prediction compatibility. To diagnose the opponent crash, capture its
`adb logcat -b crash -d` and its Sprig/Unity log around the insertion.
