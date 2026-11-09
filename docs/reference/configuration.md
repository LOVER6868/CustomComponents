# Configuration

Edit `config.ini` before starting the game. Keep gameplay-affecting settings the same across PvP clients.

The main sections cover module enablement (`General`), PickFromThree selection and presentation, targeting prompts, lifecycle repair, and deck prepopulation for PvP. Use the comments in the supplied `config.ini` for actual option names and defaults; this page isn't intended to duplicate every setting.

Custom frame/stat icons are configured separately in `[StatIcons]` and require PNG files at the paths you choose. See [presentation](presentation.md) and [the INI example](../../examples/configuration/stat-icons.ini).

For a new diagnostic option, prefer the shared settings service rather than another independent read of `config.ini`.
