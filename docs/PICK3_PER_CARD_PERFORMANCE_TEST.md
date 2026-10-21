# Per-card portrait scheduler (experimental)

- Each card has an independent elapsed-time target: `FighterFps`, `EnvironmentFps`, `OtherFps`.
- `RenderBudgetFps` is now the **global safety ceiling**, not a target distributed across every visible card.
- The scheduler renders **at most one** eligible card per frontend tick, and skips cards that are not yet due.
- Non-fighter cards default to 1 FPS, fighters to 18 FPS, and the combined ceiling is 30 calls/sec.
- Set `EnvironmentFps=0` / `OtherFps=0` to freeze after the first three render passes (the UI visibility threshold).
- `SpacingFrames=30` still stages native factory requests sequentially. This does not eliminate native load time.
- Avoid duplicate INI sections; there is now exactly one `[Pick3AnimationExperiment]`.
- All three avatars still remain loaded while the picker is open, so memory and animation cost of the objects themselves is not entirely removed.
- Check log `AVATAR_RENDER: PER_CARD` to verify config and `PERF` for runtime sampling.
- Untested on device. Restore prior ZIP if the experimental change regresses behavior.
