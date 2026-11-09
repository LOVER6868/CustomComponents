# Features

This is a map of the card-authoring features, not a list of internal runtime components. For field names and restrictions, follow the linked guide or example.

| What you want to do | Use | Details |
| --- | --- | --- |
| Choose from deck, discard, or card database | `PickFromThree` selection modes | [Guide](PICKFROMTHREE.md) |
| Change a card's cost and stats when played | `Level`, `Leveled`, `LeveledQuery` | [Guide](LEVEL.md) |
| Remember counters, GUIDs, entities, or arrays | `Variables`, `SetVariableEffectDescriptor` | [Guide](VARIABLES.md) |
| Scale a stock effect using saved data | `VariableMultiplier`, `LevelSpentMultiplier` | [Variables](VARIABLES.md) |
| Grant or remove an ability | `GrantAbilityEffectDescriptor`, `RemoveGrantAbilityEffectDescriptor` | [Abilities](reference/abilities.md) |
| Restrict when an ability can be played | `PlayRequirement` | [Requirements](reference/play-requirements.md) |
| Move, tutor, create, discard, or return cards | Zone and deck effect descriptors | [Zone effects](reference/zone-effects.md) |
| Check identities, deck positions or legal lanes | Custom entity queries | [Queries](reference/queries.md) |
| Compare base/current stats or test buffs | `StatComparisonQuery`, `StatModifierQuery` | [Card stats](reference/card-stats.md) |
| Change a card's tags or subtypes | `Add/RemoveTag`, `Add/RemoveSubtype` descriptors | [Card stats](reference/card-stats.md) |
| Heal by each recipient's missing health | `HealEffectDescriptor` + `VariableMultiplier` | [Healing](reference/healing-and-swap.md) |
| Convert excess healing to health buffs | Grantable `OverhealToHealth` | [Healing](reference/healing-and-swap.md) |
| Exchange two fighters' board positions | `SwapPositionEffectDescriptor` | [Swapping](reference/healing-and-swap.md) |
| Fire or listen for custom events | Custom trigger components | [Abilities and triggers](reference/abilities.md) |
| Use a card-local extra play window | `SelfTeleport` | [Abilities](reference/abilities.md#selfteleport) |
| Change card frames, heralds and stat icons | Presentation extensions | [Presentation](reference/presentation.md) |

The [JSON example index](../examples/README.md) contains the actual snippets. If something behaves differently in a native build, check [compatibility](reference/compatibility.md) before treating a source test as a gameplay test.
