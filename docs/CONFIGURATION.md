# Configuration

Edit `config.ini` before launch. Keep gameplay settings consistent across PvP
clients. The supplied file contains the supported options.

| Section | Purpose |
| --- | --- |
| `General` | Master enable switch |
| `PickFromThree` | Candidates, PvE/PvP submission and transport |
| `PickFromThreePresentation` | Custom menu and stock-command handoff |
| `Pick3TargetingText` | Prompt position, font size and line length |
| `Pick3LifecycleRepair` | Button/selection lifecycle recovery |
| `PvpDeckPrepopulation` | Physical deck setup for modded PvP |
| `StatIcons` | Optional PNG-backed icons |
| `Performance` | Aggregate and bounded slow-call measurements |

`UseStockTargeting=true` bypasses the custom renderer as a diagnostic fallback.
It is not a replacement renderer for every custom selection mode.

Deck setup retains legacy option names ending in `Lab`. The supplied values
select the stock-backed setup path. Change deck settings on both clients together.

## Diagnostics

`Performance.Enabled=true` reports elapsed durations and bounded slow calls.
Region totals can nest; `selfTotalMs` excludes measured child scopes only. These
are not exclusive CPU or GPU measurements. Disable profiling for a normal
performance comparison after collecting a capture.

`Pick3CreationTrace.Enabled` controls optional diagnostics. Functional creation/draw
binding initializes independently of this switch.

`Pick3PrePlayTrace`, `Pick3Diagnostics`, `PvpDeckDiagnostics`, `StartupDiagnostics`
and `Pick3Stage8Isolation` support troubleshooting. Startup cutoff/hook-isolation
settings can disable systems; retain the supplied full-startup values for normal
use. See [presentation](PRESENTATION.md) for icon configuration.
