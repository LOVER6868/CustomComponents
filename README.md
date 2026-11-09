# CustomComponents

CustomComponents is a Sprig/Bloom module for adding mechanics to *Plants vs. Zombies Heroes*. Most of the work happens in card JSON: you can add abilities, set variables, choose cards from decks or discard piles, and use new effects without writing a separate native hook for each card.

The module's native code lives in `inc/` and `main.cpp`. Card examples live in `examples/`.

## Getting started

Start with [installation and card layout](docs/GETTING_STARTED.md), then use the [feature list](docs/FEATURES.md) to find what you need. The guides for [Level](docs/LEVEL.md), [variables](docs/VARIABLES.md), and [PickFromThree](docs/PICKFROMTHREE.md) cover the systems that take a little more setup. More specific fields and engine behavior are in the [reference](docs/reference/README.md).

For copyable JSON, use the [example index](examples/README.md). Each file tells you what to insert; most are **components or effect groups**, not entire card definitions.

## Before you test a card

Stock component order still matters. A typical effect entity contains its grouping and trigger, then trigger filters, primary targeting, the effect descriptor, and any multiplier or secondary-target filter. The examples follow the game's serialized order.

Keep PvP clients on the same native module and card data. See [compatibility notes](docs/reference/compatibility.md) before testing online.

[Changelog](CHANGELOG.md) · [Build settings](docs/reference/configuration.md) · [Source layout](docs/reference/architecture.md)
