# Level

Add a `Level` component to a card. Its `Levels` array contains independent options
in the exact order the author supplies. Each option has an extra `SunCost`, an
`Attack` buff and a `Health` buff. The normal, unmodified option comes first when
cycling and is not included in the array.

```json
{
  "$type": "PvZCards.Engine.Components.Level, EngineLib",
  "$data": {
    "Levels": [
      { "SunCost": 2, "Attack": 2, "Health": 2 },
      { "SunCost": 3, "Attack": 4, "Health": 4 }
    ]
  }
}
```

For a 1-cost 2/2, these options are 3-cost 4/4 and 4-cost 6/6. Options replace one
another; cycling never accumulates their buffs. A quick back-and-forth shake while
dragging the card cycles normal -> first option -> next option -> normal. Selection
is restricted to your actual hand. Unaffordable options remain in the cycle and
use normal play validation. Returning a drag to hand keeps the selected option.
The hand glow also respects the selected cost: unaffordable options lose the
stock playable glow, and returning to an affordable option restores it when the
card otherwise remains playable. A selected option tints the sun-cost number
light blue. Returning to base restores the game's normal cost colour.

## Spending all resources and X

`SunCost: -1` spends all currently available sun/brains, provided the normal cost
can be paid. Other negative sun modes are reserved and rejected. Attack and health
accept signed integers or `"X"`. Numeric negatives remain literal negative buffs.
X is the extra payment above the card's current normal cost, and works with fixed
cost options too.

```json
{ "SunCost": -1, "Attack": "X", "Health": -2 }
```

With normal cost 1 and five available sun, this costs five and grants +4/-2. If
normal cost and available sun are both five, the dynamic option resets to base. Ordinary cost changes are
included before adding the selected option. A stock free-play marker makes the
normal cost zero; selecting a paid option still charges that option.

## Leveled and cleanup

A positive extra payment adds `Leveled` with `Value` equal to that payment. +1 adds
Leveled=1. Zero extra payment adds no public Leveled component. The selected
option's fixed buffs can still apply; internal bookkeeping is separate.

Use `LeveledQuery` with `Value` and `ComparisonOperator` (Equal, NotEqual, Greater,
GreaterOrEqual, Less, LessOrEqual). It does not match a card without Leveled. Combine
it with SelfQuery in a trigger filter to select a particular paid level.

Selecting an option applies native `ModifiableInt` modifiers to the hand card.
Queries see the selected cost, attack and health, including stock buff/nerf checks.
No BuffEffect is emitted, so changing mode does not fire a buff trigger. Cycling
back to base removes only Level-owned modifiers and the public Leveled marker.

`Level` is the permanent definition of the available options. It stays on the
card at base. `Leveled` describes positive extra payment; checking for `Level`
cannot distinguish base from a selected option. Both LeveledQuery and component
presence checks can inspect the synchronized hand state.

Full-cost and X options recalculate after resource or cost changes. If available
resources fall to the card's current cost without Level modifiers, or below it,
the option resets to base. Fixed options without X remain selected when
unaffordable. Once played, the applied values freeze until normal Level cleanup.

A stock `SelfEntityFilter` is an effect condition, not a general play requirement.
Failing it can suppress the effect while still allowing the trick to be released.
Use [PlayRequirement](PLAY_REQUIREMENTS.md) with `LeveledQuery` when the selected
level must govern whether a trick can be played.

The state is present before play listeners run. Level-owned modifiers reset when
the card leaves the board; moving between lanes retains them. Hand re-entry also
clears a previous application. Other modifiers and damage are left to normal game
rules. Tricks can retain their committed state while their play effects resolve;
returning them to hand starts a fresh choice. Copied state uses native deep-copy
and pool lifecycles.

## Presentation and multiplayer

Each shake sends an ordered hand-mode command through the existing local/remote
play service. Both clients validate ownership, hand membership and the table
index, then apply the same modifiers without paying resources or advancing the
phase. Payment still happens only when the card is played. The opponent's hand
remains facedown and does not receive the local pulse.

The command uses the stock None play type with a versioned marker in
OrderInTargetLocation and one target containing entry index + 1 (zero means
base). None is reserved for this marked control action in matching module builds.
The existing Level extension on actual card plays verifies the selected entry.
Mode requests supply native success and failure callbacks. Synchronous success
refreshes the current drag immediately; asynchronous acknowledgements queue a
scene-checked frontend refresh for playability, targets and the cost tint.
Native copies and pooling preserve application state and committed choices.

The current Python PvP relay forwards these payloads unchanged and needs no
change. Both clients need matching native code and card data. Native build and
policy checks do not replace a two-client prediction/replay test.

## Changing the controls

`inc/level/LevelSelectionController.hpp` centralizes NextLevel, ChooseEntry and
ResetChoice. `LevelGesturePolicy.hpp` contains gesture thresholds and recognition.
`LevelVisualStyle.hpp` contains the cost tint, pulse duration and amplitude.
`LevelPulseAnimation.hpp` contains the easing. The pulse continues after releasing
a card back into hand, composes with the view's scale and restores its own scale
change on completion or cancellation. `LevelHandHighlight.hpp` uses the current native play cache together with the
selected option's affordability. Changing an option rebuilds that cache and
refreshes the current input state, so card targets, lanes and casting zones follow
the new selection. `LevelPlacementPresentation.hpp` clears invalid drag targets.
`LevelFrameState.hpp` feeds native cost, attack and health updates from the
selected entity. Stock colour rules compare these values with original stats;
Level cost text uses the centralized blue tint.
The cost-number tint follows the stock cost-colour update path and is removed
when returning to base or leaving the hand. The text owns its temporary font
face, cached vertex colour, gradient and tag settings; shared font assets are
not edited.

See [Level examples](../examples/abilities/README.md) and
[query examples](../examples/queries/README.md).

`LevelStatColours.hpp` updates the card dimmer's undimmed text-colour cache for
the three stat labels. The native fade then applies to the correct blue or stock
stat colour. Other labels and cards keep their normal dimmer behavior.

Frame binding also preserves original stat baselines when a snapshot already
contains the selected numbers, so unchanged-value optimizations cannot hide the
buff or nerf colour.

## Scaling effects by the selected Level

`Leveled.Value` is the **extra sun** selected by a Level option; the normal
play has no public `Leveled` marker and reads as zero. This differs from the
zero-based option `EntryIndex` and the card's total sun cost.

The optional `LevelSpentMultiplier` multiplies native multiplier-aware effect
amounts by this value; it inherits `Query` and `Divider` from `QueryMultiplier`.
For example, damage 2 at Level 3 deals 6 with Divider 1.

In `SetVariableEffectDescriptor`, set `ValueSource` to `Level`, `ValueType` to
`Int` and `Owner` to `Self` to copy the same selected extra-sun number into a
card variable. Both features read the existing `Leveled` component and do not
create a separate payment component. See `docs/VARIABLES.md` and examples.
