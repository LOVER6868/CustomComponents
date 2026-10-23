# Pick3 command ownership handoff diagnostic

This patch records the intercepted SelectTargetCommand only if its injected SelectTargetService is identical to the active submission service. The command identity is kept in a GC handle until the matching transaction completes and then dropped. A mismatched command cannot supply the PvE refresh signal.

The patch does **not** call original Execute, SelectTarget, Release, or the stock success callback on a suppressed command. These would be unsafe without knowing the original command's retain/target binding lifecycle and could duplicate the network request. Existing PvP and CardDatabase transport behavior remains unchanged.

Inspect `COMMAND_OWNER: blocked` and `COMMAND_OWNER: finish` in logs to establish whether the intercepted stock command is even the same owner as the custom target service. This is an instrumented incremental patch, **not a verified PvP submission fix**.
