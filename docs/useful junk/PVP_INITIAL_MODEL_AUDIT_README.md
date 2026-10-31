# Optional PvP InitialGameState snapshot

This is a read-only development probe, **not** a PvP deck materialization fix.

Change `config.ini`:

```ini
[PvpDeckDiagnostics]
InitSnapshot=true
```

Rebuild Sprig; enter one PvP match. Filter logcat for `CustomComponents/PVP_INIT_AUDIT`. The hook observes `PvpInitResponseExecutable.InitializeGame` after stock initialization and logs the base model pointer and live/virtual counts in the four fixed deck pools of the referenced public base server (zombie 1/2, plant 6/7). It makes no writes to game entities, card pools, prediction models or target lists. Defaults to false. Restore false after the test.

The base server uses GUID-only `CardPoolEntry` records. The correct proposed materializer is the stock `PrepopulateCardPoolSystem` inside the synchronized executing simulation, not the picker; this still needs verified lifecycle and network compatibility. See `PVP_DECK_SERVER_AND_INIT_HANDOFF_20261025.md` for the source analysis and constraints.
