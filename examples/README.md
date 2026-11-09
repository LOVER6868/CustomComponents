# Card JSON examples

These files are recipes, not a second card database. Most show **one component** or an `EffectEntitiesDescriptor` to place inside a card's `entity.components`. Files under `templates/` vary; open the file before copying it.

When an example includes an effect entity, keep its nested `components` array intact. The usual order is grouping, trigger, trigger filters, primary-target filter, effect descriptor, then multiplier or secondary-target filter. That matches the stock card data. All `$type` and type-valued fields use full EngineLib assembly names.

## Find an example

| Folder | What you'll find |
| --- | --- |
| [abilities](abilities/README.md) | Innate abilities, grants, removal and Level choices |
| [variables](variables/README.md) | Counters, arrays, expressions and multipliers |
| [new-card-features](new-card-features/README.md) | Full heal, overheal and position swapping |
| [card-features](card-features/README.md) | Base stats, comparisons, tags and subtypes |
| [pick-from-three](pick-from-three/README.md) | Card and definition selection, additional targets |
| [queries](queries/README.md) | Deck/discard, placement, Level and identity checks |
| [play-requirements](play-requirements/README.md) | Trick requirements and Level gates |
| [zone-effects](zone-effects/README.md) | Discard, return, create and play-from-deck |
| [draw-effects](draw-effects/README.md) | Tutor cards from a deck subset |
| [deck-order](deck-order/README.md) | Move cards to deck positions |
| [triggers](triggers/README.md) | Named, discard, return and picked-card events |
| [templates](templates/README.md) | Larger setups and variants |
| [reference](reference/README.md) | Runtime shapes for reading only |
| [configuration](configuration/README.md) | INI settings |

For what each custom field does, use the [documentation](../docs/README.md). If you need an entire stock card wrapper, start from your current card data; don't assume a one-component recipe can be loaded as a whole card.
