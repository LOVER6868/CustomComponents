# Experimental PickFromThree PvP prototype

This is **not** verified working PvP. It is an opt-in test of the stock `PlayType.SelectTarget=6` transport using existing `RemotePlayService.Process(Play)` and the existing target-snapshot checks.

## Activation (only on controlled, mutually modded clients)

Under `[PickFromThree]` set:

```ini
LiveCandidateProbe=true
EnableGameplaySubmission=true
ExperimentalPvpPrototype=true
```

`ExperimentalPvpPrototype` defaults to `false`. The experimental flag activates canonical ordering and a nonce-free candidate seed **for both PvE and PvP sessions in that build**. Use a separate test installation/configuration if preserving PvE randomness matters.

## Deliberate limitations

- One live entity target per action only. No CardDatabase (mode 5), no multi-select.
- Identical candidate lists and effect/group IDs are required across peers. We have NOT proved this occurs.
- The game has client-authoritative and server-authoritative simulators; stock action submission does **not** establish server acceptance of new custom selection enum values.
- `std::mt19937` is deterministic locally but this is not the engine's replicated RNG, and the subset is not carried in the Play payload.
- Two-client controlled testing and response/sync verification are mandatory; never treat `DISPATCH_RETURN` as success.
- Existing Phase 2 read-only tracer remains; Phase 3 ownership hooks are not present.

## Expected signal

`PVP_PROTOTYPE: requested=1 canonicalSampling=1`, then `SUBMIT: SEND mode=PVP_REMOTE`, then stock `PVP_TRACE: REMOTE_CALL ... select=1 type=6`. A failure or desync indicates authoritative target validation or divergence still needs work.
