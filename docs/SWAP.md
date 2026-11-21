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
