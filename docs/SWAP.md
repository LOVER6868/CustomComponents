# Swap position

`SwapPositionEffectDescriptor` exchanges the fighters selected by the authored
primary and secondary queries. Queries control faction and ownership restrictions.

Two optional JSON fields relax destination validation for this effect:

```json
{
  "IgnoreTerrainRules": true,
  "IgnoreOccupancyRules": true
}
```

Both default to `false`. Set either independently. `IgnoreTerrainRules` allows
non-aquatic fighters in water. `IgnoreOccupancyRules` bypasses destination capacity
and team-up restrictions. These fields do not change the queries or allow missing,
dead, identical, or non-fighter targets. Selection and execution use the same
policy. These are swap options, not global changes to movement rules.

Ignoring occupancy can intentionally create board arrangements that ordinary play
would reject. The transaction records both options alongside both placements.

## Random lane shuffle

`SwapRandomlyEffectDescriptor` uses the resolved primary targets. Use an `All` primary filter for the fighters you want to shuffle. Each selected face-up fighter must land in a different lane on its own side of the board.

Terrain and team-up rules apply to the whole final arrangement. If there are fewer than two fighters or no complete legal assignment, no fighters move. A successful shuffle uses one transaction for live animation and replay.

[Shuffle example](../examples/new-card-features/swap-randomly.json).
