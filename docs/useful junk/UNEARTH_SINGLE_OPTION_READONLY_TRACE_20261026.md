# One-option Unearth read-only diagnostics

This build adds observational logs only. No new trigger broadcasts or auto-selection logic is added. Stock gameplay and Step 2B shuffle/materializer remain unchanged.

## Log markers
- `CustomComponents/UnearthTrace: FIND_RESULT`: stock finder result for small recipient sets; `stockCount=1` signals one stock target, not necessarily one offered UI option.
- `CustomComponents/UnearthTrace: OFFER_DECISION`: stock/candidate/preview counts for one-result situations.
- `CustomComponents/UnearthTrace: REPEAT_FIND`: repeated stock finder after offer.
- `CustomComponents/UnearthTrace: MANUAL_SUBMIT`: our custom UI submission path.
- `CustomComponents/UnearthTrace: CUSTOM_PLAY_LISTENER`: listener verdict **only for explicitly custom-dispatched PlayTrigger**. This does not instrument the stock generic PlayTrigger system; absence of a log does not prove stock has no listener.

Capture both phones with the existing two-phone PowerShell toolkit, testing one-option Unearth and three-option Unearth. Compare effect/group/model and listener verdicts. The diagnostic itself does not change behavior.
