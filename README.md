# CustomComponents

CustomComponents is a Sprig/Bloom module for authoring custom PvZ Heroes cards.
It adds PickFromThree selection, deck and discard effects, reusable queries,
custom abilities, and card presentation support.

## Start here

- [Installation](docs/GETTING_STARTED.md) — install the module and build it.
- [Features](docs/FEATURES.md) — the public component and system catalog.
- [JSON examples](examples/README.md) — recipes grouped by task and insertion point.
- [PickFromThree](docs/PICKFROMTHREE.md) — selection, additional targets and listeners.
- [Configuration](docs/CONFIGURATION.md) — runtime settings and optional icons.
- [Architecture](docs/ARCHITECTURE.md) — ownership and extension points.
- [Changelog](CHANGELOG.md) — changes from the linked upstream baseline.

## What you can build

Choose cards from a deck, discard pile or database; act on the exact selected card;
return fighters and environments to lanes; tutor or create cards; filter ordered
pools; grant Armor, Charge, Thrash or a random keyword; and connect effects through
named or built-in triggers.

The module supplies discarded-card herald text and discard inspection support.
Optional HeraldModifiers can append its modifier summary. Frame icons use
user-supplied PNGs configured in `config.ini`.

## Repository contents

`main.cpp`, `inc/`, `src/`, `bloom.ini` and `config.ini` form the module. `patches/`
contains its required dependency fix. `docs/` and `examples/` are the authoring
manual and recipe catalog. Generated asset sources are included in `src/`;
asset-generation tools are not needed to compile this checkout.

Local development can also contain ignored `tests/`, `tools/`, `.build/` and
`.local/` directories. The distributed module builds without them. See
[repository maintenance](docs/CONTRIBUTING.md) for Git tracking and packaging.

Use matching native builds, card data and gameplay configuration on both PvP
clients. See [compatibility](docs/KNOWN_ISSUES.md) for supported boundaries.
