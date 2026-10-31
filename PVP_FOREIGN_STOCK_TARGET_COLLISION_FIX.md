# PvP stock target collision guard

The 23:52:25, 23:52:30, 23:54:00 and 23:55:00 logs show SelectTargetCommand.Execute on a foreign service while a PickFromThree popup is capturing input. The stock command sends a separate play with CardId=-1 and Targets=[59] or [44] before the legitimate custom popup submits its selection. The local simulator rejects several with `CLIENT_SERVICE_FAILURE` and `DB5_PVP RX_INVALID`.

Changes:
- While modal gameplay selection is active AND a stock SelectTargetCommand has an injected service object DIFFERENT from the picker-bound service, do not forward `Execute` to the stock method. Emit always-on `FOREIGN_TARGET_EXECUTE_SUPPRESSED` log.
- Leave normal SelectTargetCommand execution unchanged outside the modal.
- Leave the same-service stock lifecycle interception intact.
- Preserve the previous read-only target discovery rollback (no bulk card allocation).
- CardDatabase and InDeckGuid unmodified.

This removes one demonstrated source of erroneous stock target commands. It does not prove or guarantee resolution of opponent-side desync; local failure is not an authoritative server verdict. Test both PvP clients, especially chained legitimate targeting effects. If target selection stalls, inspect suppressed command logs and effect ownership, and revert this guard.
