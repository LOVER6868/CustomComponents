# Trick play requirements

Use `PlayRequirement` on a trick's **effect entity**, alongside `PlayTrigger` and
its existing targeting and effect descriptors. Its `Query` is a normal entity
query, including composite queries. It is evaluated against the source trick.

```json
{
  "$type": "PvZCards.Engine.Components.PlayRequirement, EngineLib",
  "$data": {
    "Query": {
      "$type": "PvZCards.Engine.Queries.LeveledQuery, EngineLib",
      "$data": { "Value": 0, "ComparisonOperator": "Greater" }
    }
  }
}
```

This ability requires positive extra Level payment. Base mode does not qualify.
The selected hand option is synchronized and visible to `LeveledQuery`.
Received card plays verify their transmitted choice against that state.

## Several abilities

A trick needs at least one qualifying on-play ability. An ability without
`PlayRequirement` is unrestricted and can keep the card playable. If every
on-play ability has a failing requirement, the trick cannot be played.

Each ability is checked independently. For example, use `LeveledQuery` equal to
1 on one ability and equal to 3 on another. Only the matching ability runs. Add a
requirement to **every** on-play ability if the card must be unavailable at base.

Normal costs, phase restrictions, targets and placement checks still apply.
Requirements narrow stock playability; they do not manufacture valid targets.
An ability must have its own native primary-target pool before its requirement
can qualify it. Targets from one ability cannot satisfy another ability's
requirement. The final stock validator still decides casting and lane legality.
Keep the normal `TriggerTargetFilter(SelfQuery)` when the ability should react
only to its own card being played.

## Evaluation and lifetime

The query runs before payment and before Level's stat modifiers are applied.
`SelfId` and `SourceId` refer to the source trick; `EffectEntityId` identifies the
ability being checked. There is no selected PF3 card yet, and `TargetId` is unset.
Put target eligibility in the normal target filters.

Eligible abilities are recorded before payment. Resolution uses that decision,
so spending resources or moving the trick out of hand does not invalidate an
ability that already qualified. Native model copies preserve this state; a new
play checks the requirements again. `PlayRequirementState` is internal state and
must not be authored in card JSON.

This component supports on-play abilities on stock `Burst` cards. Conditions on
other triggers keep their normal behavior. A missing or invalid requirement query
rejects its ability.

## Stock alternatives

Stock `Unplayable`, placed on the card, makes it unconditionally unplayable.
Use that marker when no query is needed. `SelfEntityFilter` and other trigger
conditions control effect execution and do not reliably prevent trick release.
Put a condition in `PlayRequirement.Query` when it must govern casting.

The [examples](../examples/play-requirements/README.md) include individual
requirements, a composite query, the stock marker and two Level-specific drawing
abilities. The drawing setup targets the plant player; adapt its faction query
when using it on a zombie trick. Pair it with the supplied Level component on an
existing plant `Burst` card.

Both PvP clients need matching code and card definitions. Native validation,
copy/pool and policy checks do not replace a two-client gameplay test.

Locally generated Level play candidates refresh when the synchronized hand
option changes. Received or serialized commands retain their committed choice.
For physical PF3 choices, validation also evaluates that ability's primary query;
a casting zone alone is not evidence that eligible cards exist. Database choices
and additional-first queries retain their native staged targeting.

The final cast gate checks each on-play ability as a pair: its requirement must
pass and its own primary query must have eligible targets. A passing base
requirement cannot use targets belonging only to a different, gated ability.
