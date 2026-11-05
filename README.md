# CustomComponents

Android C++ module for Sprig/Bloom that adds PvZ Heroes abilities, queries,
discard/return effects, and PickFromThree selection.

Patch 4 adds fighter/environment returns to lanes and filters the movement event
caused by the return effect's stock carrier. Patch 3's PvP selection-context fix is
included; the user confirmed that fix works. Patch 4 still needs device testing.

## Working with the module

- Copy this directory to `Sprig-main/Bloom/CustomComponents` and use your usual Sprig build.
- Edit gameplay in `inc/`; `main.cpp` owns startup and `config.ini` owns runtime options.
- Read [source layout](docs/SOURCE_LAYOUT.md) and [hook ownership](docs/HOOK_OWNERSHIP.md) before changing hooks.
- Use the [ReturnToLane guide](docs/reference/return-to-lane.md) and [PickFromThree guide](docs/PICKFROMTHREE.md) for card setup.
- Run [native checks](tests/README.md) before packaging changes.
- See [release notes](CHANGELOG.md) for validation and remaining device checks.

`docs/history/` contains older investigation notes and changesets. They are
reference material, not current setup instructions. Source and required runtime
assets stay in the module; build products stay under the ignored `.build/` folder.
