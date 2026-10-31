# PickFromThree — read-only event context / validator diagnostics

This adds logging only to the existing custom named-trigger dispatcher, without changing effect source/target IDs, validator results, or listener queue decisions.

## Test

1. Keep `[Pick3CreationTrace] Enabled=true` if you want the GUID-to-drawn-entity correlation in the same log.
2. Rebuild your normal Android mod and play a PvE game with card 716.
3. Choose a CardDatabase option once and capture `PICK3_CONTEXT`, `PICK3_VALIDATE`, `PICK3_LISTENER`, `PICK3_LISTENERS`, and `CREATION_TRACE` log lines.

`PICK3_CONTEXT` reports firing effect fields, copied fields, the injected `EntityQueryContext.Model` pointer, producer model, expected model, and `modelMatches`. A null or mismatched `queryModel` indicates a possible pooled context injection issue, but does not alone prove why the validator rejected a particular listener.

`PICK3_VALIDATE` differentiates missing listener-source membership (`sourceExists=0`) from an actual validator rejection (`validatorCalled=1 validatorResult=0`), and records `SelfId` used during validation. `PICK3_LISTENERS` still reports the actual queued count; `PICK3_TRIGGER queued=1` denotes dispatch completion only.

This patch does not hook or modify stock triggers and does not yet identify individual condition-component results. Stock trigger comparison can be added separately if needed, after the current custom event context is captured.
