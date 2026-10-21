# CustomComponents (Sprig/Bloom)

Android C++/BNM Bloom module implementing PvZ Heroes custom components,
combat/query systems and PickFromThree choice presentation.

- **PickFromThree setup, configuration, rendering, ADB modes and known limits:**
  [`docs/PICKFROMTHREE.md`](docs/PICKFROMTHREE.md)
- **Core component docs:** `docs/reference/` and `docs/FEATURES.md`
- **Native source:** `inc/`, `main.cpp`
- **Runtime configuration:** `config.ini`
- **Remove obsolete Pick3 notes and reference images:**
  run `./Clean-Pick3Junk.ps1` in PowerShell from this folder.

The `assets/pick3-static/*.png` files are build inputs, not disposable reference
art. Do not delete them. No auto-backup folders are created by cleanup.
