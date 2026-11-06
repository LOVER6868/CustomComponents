# Source layout

| Path | Responsibility |
| --- | --- |
| `main.cpp` | Registration and startup |
| `bloom.ini`, `config.ini` | Module manifest and runtime settings |
| `inc/CustomComponents.hpp` | Public component umbrella |
| `inc/abilities/` | Grants, expiry and combat behavior |
| `inc/effects/` | Descriptors, runtime effects and creation extensions |
| `inc/zones/` | Discard, return, transfer and deck materialization |
| `inc/queries/` | Zone, identity, position and placement predicates |
| `inc/triggers/` | Trigger types, dispatch and return-origin filters |
| `inc/pick3/` | Offers, transactions, routing and creation provenance |
| `inc/pick3/submission/` | Private stock-handoff implementation pieces |
| `inc/presentation/` | Picker, inspection, frames, heralds and icons |
| `inc/engine/` | Shared stock adapters, queues and subset services |
| `inc/core/` | Registration, ownership, lifetime and scheduling |
| `inc/diagnostics/` | Optional trace support |
| `src/` | Included generated assets and their sources |
| `patches/` | Required dependency fix and installer |
| `examples/` | Categorized card-data recipes |
| `docs/` | Current public manual |

Most systems are header-based. Internal `.inl` files are included inside their
owner's namespace, not compiled separately. Optional local `tests/`, `tools/`,
`.build/` and `.local/` folders are excluded from the GitHub distribution and are
not module dependencies.
