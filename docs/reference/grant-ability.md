# Granting abilities

Use `GrantAbilityEffectDescriptor` with the registered ability name. Armor is
numeric; Charge and Thrash use inherited counters; RandomKeyword resolves to a
concrete keyword. Withering uses a mode, and attack markers use presence.

Durations are Permanent, EndOfTurn and NextFighter. History records grants and
removals. Native removal uses the descriptor's `ToRemovalEffect` path, such as
continuous-effect cleanup, plus duration expiry. It matches source and duration;
the module also supplies an explicit `RemoveGrantAbilityEffectDescriptor`
authoring component. Its default All mode removes innate and granted state;
SourceAndDuration removes the matching source grant.

`ModifyOperation` supports Add/Set for numeric abilities. Withering replaces the
mode for its source/duration and selects the most recent surviving grant. Marker
grants ignore numeric values and preserve independent sources and innate markers.

See [ability rules](../ABILITIES.md), [examples](../../examples/abilities/README.md)
and the [feature catalog](../FEATURES.md). Generated runtime removal effects under the
reference examples document runtime state; do not insert them as descriptors.
