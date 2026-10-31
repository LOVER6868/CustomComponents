# PickFromThree listener and cost diagnostics (read-only)

The stock draw binding remains unchanged. Added non-verbose per-trigger `PICK3_LISTENERS` summary, per-listener `PICK3_LISTENER` verdicts, `PICK3_ENTITY_QUERY` matching, and stock `ModifySunCostSystem.ProcessEffect` enter/exit messages (`PICK3_COST_EFFECT`).

`PICK3_TRIGGER queued=1` is a historical boolean meaning dispatcher scan returned successfully; only `PICK3_LISTENERS queued=N` reports actual listeners. The stock cost hook is optional and logs `PICK3_COST_TRACE enabled=0` if method/fields are missing. No cost adjustment, trigger replay or PvP mutation was added.

Check `PICK3_LISTENERS`, `PICK3_LISTENER`, `PICK3_ENTITY_QUERY`, `PICK3_COST_EFFECT`, and `CREATION_TRACE`. Beware that unrelated stock cost modifiers may also be logged.
