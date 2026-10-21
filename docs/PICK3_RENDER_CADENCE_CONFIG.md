# Pick3 animated portrait render cadence

Config in `[Pick3AnimationExperiment]`:

```ini
RenderEveryNTicks=1
RenderBudgetFps=60
```

`RenderBudgetFps` is the **total** render-call budget shared across all active animated portraits (not per portrait). The runtime clamps it to 1..180. `RenderEveryNTicks` is an additional UI-tick throttle (1..12); leave it at 1 for smoothness. The renderer draws at most one visible portrait per eligible tick, round-robin. On a 60 Hz UI loop with three avatars, this means at most 60 total renders/s, or around 20 updates/s per portrait. The actual rate can be lower if the UI updates more slowly or rendering is expensive.

If game performance degrades, try `RenderBudgetFps=30`, then `RenderBudgetFps=18`. These are experimental values, not guaranteed frame rates.

Existing mask, 256x256 RenderTextures, native loading, first-render behavior, and static fallback are unchanged.
