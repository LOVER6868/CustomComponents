# Source layout

| Path | Responsibility |
| --- | --- |
| `main.cpp` | Module startup and initialization order |
| `config.ini`, `bloom.ini` | Runtime settings and Bloom manifest |
| `inc/abilities/` | Armor, Charge, Thrash, and ability lifecycle |
| `inc/effects/` | Managed descriptors and runtime effect definitions |
| `inc/queries/` | Entity/database query implementations |
| `inc/zones/` | Discard, return, and deck transfers |
| `inc/triggers/` | Trigger components and model-specific dispatch |
| `inc/pick3/` | Choices, transport, model transactions, and provenance |
| `inc/presentation/` | Card views, picker, history presentation, and reconciliation |
| `inc/core/` | Registration, serialization, shared lifecycle services |
| `inc/diagnostics/` | Diagnostic switches and observations |
| `examples/` | Card-data examples |
| `tests/` | Behavioral regressions and retained historical source checks |
| `tools/` | Validation utilities |
| `docs/reference/` | Component usage guides |
| `docs/history/` | Earlier patch notes, investigations, and changesets |
| `.build/` | Ignored local test executables and generated files |

The public umbrella headers are `inc/CustomComponents.hpp`,
`inc/DiscardPileZoneEffects.hpp`, and `inc/DrawCardFromSubsetInDeck.hpp`.
Header-only systems follow the module's existing build integration. `.inl`
fragments are included inside their owning namespace and are not standalone units.

## ReturnToLane

`zones/DiscardPileTransferSystem.hpp` owns metadata and hooks. Its
`detail/DiscardRevivePairLegality.inl` is the shared read-only feasibility check;
`detail/DiscardToLane.inl` performs the transfer. `ReturnToLanePolicy.hpp` contains
host-testable card-type and capacity rules.

The runtime borrows `MoveCardToLanesEffect`, so
`triggers/ReturnToLaneMoveFilter.hpp` distinguishes return-origin movement events.
The existing `TriggeredEffectCollection.Add` hook in `SelectionTransactionRuntime`
applies the filter before attaching a PF3 ticket. The owning trigger producer
returns suppressed data to its stock pool. No second Add detour is installed.

Keep simulation models separate from UI models. Preserve stock entity identity,
trigger validation, history records, and queue ownership when changing a transfer.
