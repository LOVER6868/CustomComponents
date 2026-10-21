# Preliminary release hardening (2026-10-20)

Based on the user-provided CustomComponents(20261020-044236).zip.

- Retains the match-scoped PickedFromThree cache; never clears selections on popup dismissal, clears them at match end.
- Hard-disables PvP gameplay submission in C++ (not just config) until tested.
- In render-stage Stop(), disable/detach the bound RawImage before releasing the 512x512 RenderTexture and camera. Roots are released afterward. Unity object validity on destroyed scenes is still a risk; Android validation required.
- Restricts the aggregate RenderTexture paint budget to 75 frames/sec, fighter 30 / environment 20 / other 20.
- Removes user-facing experimental native-avatar configuration switches. Maintains three native inspection avatars by default to avoid regressing UI functionality. The CardView factory remains inherently device-dependent and **not crash-certified**.
- Drops early Frida trace script and noisy asset capture logs from the shipping tree. Does not delete the internal diagnostic infrastructure needed for crash analysis.
- Excludes .git, archived probes, and test documentation from distributable source ZIP.

## Open risks

- Native CardView asynchronous callbacks and lifecycle cancellation need on-device soak tests and crash tombstones.
- 128 submitted-play root cap remains unresolved; do not free game-owned plays prematurely.
- The project does not include the full Sprig/BNM Android build toolchain; no APK compilation or device verification performed.
- Original README and older docs reflect older debug configurations. Consult this note for current release baseline.
